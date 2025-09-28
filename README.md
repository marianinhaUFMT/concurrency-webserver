# Concurrent Web Server in C

This project is a multi-threaded web server implemented in C, developed as a practical assignment for an Operating Systems course. The server is built upon a producer-consumer model, utilizing a thread pool to handle multiple client requests concurrently.

## Project Overview

The initial codebase was a simple, single-threaded web server. The main goal of this project was to refactor it to handle concurrent connections efficiently. This was achieved by implementing a master thread (producer) that accepts new connections and places them into a shared buffer, and a pool of worker threads (consumers) that retrieve requests from this buffer and process them.

Synchronization between threads is managed using mutexes and condition variables to prevent race conditions and ensure thread safety.

## Features Implemented

-   **Multi-threading:** A pool of worker threads is created at startup to handle requests.
-   **Bounded Buffer:** A fixed-size shared buffer is used to hold incoming requests, decoupling the master thread from the worker threads.
-   **Producer-Consumer Model:** The master thread acts as the producer, and worker threads act as consumers.
-   **Synchronization:** Thread-safe access to the shared buffer is guaranteed through the use of `pthread_mutex_t` and `pthread_cond_t`, with no busy-waiting.
-   **Scheduling Policies:** The server supports two different scheduling policies for workers:
    -   **FIFO (First-In-First-Out):** The oldest request in the buffer is served first.
    -   **SFF (Smallest File First):** The request for the smallest file (in bytes) is served first to improve response time for short requests.

## How to Compile

The project includes a `Makefile` that simplifies the compilation process. To compile all necessary files, simply run:

```bash
make
```

This will generate three main executables:
-   `wserver`: The concurrent web server.
-   `wclient`: A simple web client for testing.
-   `spin.cgi`: A CGI script used to simulate long-running requests.

To clean up compiled files, you can run:
```bash
make clean
```

## How to Run the Server

The server can be configured via command-line arguments.

### Command-line Options

```
./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
```

-   `-d basedir`: Specifies the root directory for the server's files. (Default: current directory)
-   `-p port`: Specifies the port number to listen on. (Default: 10000)
-   `-t threads`: The number of worker threads in the pool. (Default: 1)
-   `-b buffers`: The number of slots in the request buffer. (Default: 1)
-   `-s schedalg`: The scheduling algorithm to use. Options are `FIFO` or `SFF`. (Default: FIFO)

### Example

To run the server on port 8080 with 8 worker threads, a buffer of 16 slots, and the SFF scheduling policy:

```bash
./wserver -p 8080 -t 8 -b 16 -s SFF
```

## How to Test

1.  **Run the server** in one terminal window using one of the commands above.
2.  **Open a new terminal** to run the client.

### Simple Test

Create a test file:
```bash
echo "<h1>Hello, World!</h1>" > index.html
```

Request the file using `wclient`:
```bash
./wclient localhost 10000 /index.html
```

### Concurrency Test

To see the concurrency and scheduling policies in action, you can use the `spin.cgi` script. The following test script (`test_sff.sh`) can be used to simulate a "traffic jam" of requests and verify the SFF policy.

```bash
#!/bin/bash

# This script sends multiple requests to a server with 2 threads
# to demonstrate that SFF will prioritize the smaller index.html file.

./wclient localhost 10000 "/spin.cgi?5" & 
./wclient localhost 10000 "/spin.cgi?5" &
./wclient localhost 10000 /index.html &
./wclient localhost 10000 "/spin.cgi?4" &

wait
```
