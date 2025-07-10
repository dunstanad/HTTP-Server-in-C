#ifndef HTTP_H
#define HTTP_H

typedef struct{
    const char* content_type;
    long content_length;
} response_header;

const char* get_mime_type(const char* path);
void http_parse(char request[], int client_fd);
void create_resp_msg(int fd, char* statusline, response_header h, char* body);


#endif