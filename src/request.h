#ifndef __REQUEST_H__

void request_handle(int fd);
// function for handling requests in SFF mode
void request_handle_sff(int fd, char* uri, char* method);

#endif // __REQUEST_H__