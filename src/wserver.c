#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "request.h"
#include "io_helper.h"

#define MAXBUF (8192)

char default_root[] = ".";

// structure to hold request information
typedef struct {
    int fd;
    char uri[MAXBUF];
    char method[MAXBUF];
    int size; // used for SFF only
} request_t;

// shared buffer, counters, and pointers
request_t *buffer = NULL;
int buffer_size = 1;
int buffer_count = 0;
int buffer_in = 0;
int buffer_out = 0;

// mutex and condition variables for synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_empty = PTHREAD_COND_INITIALIZER;

// scheduling policy
enum { FIFO, SFF } sched_alg = FIFO;

//
// buffer handling functions (Producer/Consumer)
//

// puts a request in the buffer
void put_in_buffer(request_t req) {
    buffer[buffer_in] = req;
    buffer_in = (buffer_in + 1) % buffer_size;
    buffer_count++;
}

// gets a request from the buffer according to the scheduling policy
request_t get_from_buffer() {
    request_t req;

    if (sched_alg == FIFO) {
        req = buffer[buffer_out];
        buffer_out = (buffer_out + 1) % buffer_size;
    } else { // SFF
        int min_size = -1;
        int min_idx = -1;
        // find the smallest file in the buffer
        for (int i = 0; i < buffer_count; i++) {
            int current_idx = (buffer_out + i) % buffer_size;
            if (buffer[current_idx].size > 0 && (min_size == -1 || buffer[current_idx].size < min_size)) {
                min_size = buffer[current_idx].size;
                min_idx = current_idx;
            }
        }
        if (min_idx == -1) {
            min_idx = buffer_out;
        }
        
        req = buffer[min_idx];
        
        int current = min_idx;
        while(current != buffer_in -1 && buffer_in != 0) {
            int next = (current + 1) % buffer_size;
            buffer[current] = buffer[next];
            current = next;
        }
         buffer_in = (buffer_in - 1 + buffer_size) % buffer_size;
    }

    buffer_count--;
    return req;
}

//
// function for consumer threads
//
void *worker_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        
        // wait until the buffer has a request
        while (buffer_count == 0) {
            pthread_cond_wait(&cond_empty, &mutex);
        }
        
        request_t req = get_from_buffer();
        
        // signal master thread that there is space in the buffer
        pthread_cond_signal(&cond_full);
        
        pthread_mutex_unlock(&mutex);
        
        // process the request
        if (sched_alg == FIFO) {
            request_handle(req.fd);
        } else { // SFF
            request_handle_sff(req.fd, req.uri, req.method);
        }
        close_or_die(req.fd);
    }
}

//
// ./wserver [-d <basedir>] [-p <portnum>] [-t <threads>] [-b <buffers>] [-s <schedalg>]
//
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
    int num_threads = 1;

    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
        switch (c) {
        case 'd':
            root_dir = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            num_threads = atoi(optarg);
            break;
        case 'b':
            buffer_size = atoi(optarg);
            break;
        case 's':
            if (strcmp(optarg, "SFF") == 0) {
                sched_alg = SFF;
            } else if (strcmp(optarg, "FIFO") != 0) {
                fprintf(stderr, "Unknown scheduling algorithm: %s\n", optarg);
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]\n");
            exit(1);
    }

    // validate arguments
    if (num_threads <= 0 || buffer_size <= 0) {
        fprintf(stderr, "Number of threads and buffers must be positive.\n");
        exit(1);
    }

    // allocate the request buffer
    buffer = malloc(sizeof(request_t) * buffer_size);

	// run out this directory
    chdir_or_die(root_dir);

    // create the worker thread pool
    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    // producer code (main thread)
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr, (socklen_t *)&client_len);

        request_t req;
        req.fd = conn_fd;

        // for SFF, the master thread pre processes the request
        if (sched_alg == SFF) {
            char buf[MAXBUF], filename[MAXBUF], cgiargs[MAXBUF];
            struct stat sbuf;
            
            readline_or_die(conn_fd, buf, MAXBUF);
            sscanf(buf, "%s %s", req.method, req.uri);
            
            char uri_copy[MAXBUF];
            strcpy(uri_copy, req.uri);
            request_parse_uri(uri_copy, filename, cgiargs);
            
            if (stat(filename, &sbuf) < 0) {
                req.size = 0; 
            } else {
                req.size = sbuf.st_size;
            }
        }
        
        pthread_mutex_lock(&mutex);
        
        // wait until the buffer has free space
        while (buffer_count == buffer_size) {
            pthread_cond_wait(&cond_full, &mutex);
        }
        
        put_in_buffer(req);
        
        // signal worker threads that there is a new request
        pthread_cond_signal(&cond_empty);
        
        pthread_mutex_unlock(&mutex);
    }
    
    // free resources (doesnt reach but its good practice)
    free(buffer);
    free(threads);

    return 0;
}