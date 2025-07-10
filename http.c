#include "http.h"
#include "logger.h"  
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

const char* get_mime_type(const char* path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".pdf")) return "application/pdf";
    if (strstr(path, ".txt"))  return "text/plain";
    if (strstr(path, ".jpg"))  return "image/jpeg";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".svg")) return "image/svg+xml";
    return "application/octet-stream";
}

void send_message(int fd, char* statusline, response_header rh, char* body){
    char headers[1024];
    sprintf(headers,
        "%s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        statusline,
        rh.content_type,
        rh.content_length
    );

    send(fd, headers, strlen(headers), 0);
    send(fd, body, rh.content_length, 0);
}

void http_parse(char request[], int client_fd){
    char method[10], resource[100], version[10];
    
    FILE *fp;
    char *buffer = NULL;
    long filesize = 0;
    char http_404_response[1024];
    char http_400_response[1024];
    char *body_404 = "<html><body><h1>404 Not Found</h1></body></html>";
    snprintf(http_404_response, sizeof(http_404_response),
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        strlen(body_404),
        body_404
    );
    char *body_400 = "<html><body><h1>400 Bad Request</h1></body></html>";
    snprintf(http_400_response, sizeof(http_400_response),
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        strlen(body_400),
        body_400
    );

    // Log the raw request
    log_info("Received request from fd=%d: %s", client_fd, request);

    // Parse the request line
    sscanf(request, "%s %s %s", method, resource, version);
    log_info("METHOD: %s, RESOURCE: %s, VERSION: %s", method, resource, version);

    // Directory traversal protection
    if (strstr(resource, "../")) {
        log_warn("Potential directory traversal attempt: %s", resource);
        send(client_fd, http_400_response, strlen(http_400_response), 0);
        return;
    }

    if(strcmp("GET", method) == 0){
        char resource_path[256];

        if (strcmp(resource, "/") == 0) {
            snprintf(resource_path, sizeof(resource_path), "public/index.html");
        } else if (strncmp(resource, "/public/", 8) == 0) {
            snprintf(resource_path, sizeof(resource_path), "public/%s", resource + 8);
        } else {
            snprintf(resource_path, sizeof(resource_path), "public%s", resource);
        }

        log_debug("Mapped resource to path: %s", resource_path);

        const char *mode = (strstr(resource_path, ".html") || strstr(resource_path, ".txt")) ? "r" : "rb";

        fp = fopen(resource_path, mode);

        if (fp){
            fseek(fp, 0L, SEEK_END);
            filesize = ftell(fp);
            rewind(fp);

            buffer = (char *)calloc(filesize, sizeof(char));
            if (buffer == NULL){
                log_error("Memory allocation failed for buffer");
                fclose(fp);
                send(client_fd, http_400_response, strlen(http_400_response), 0);
                return;
            }

            fread(buffer, sizeof(char), filesize, fp);
            fclose(fp);

            response_header rh;
            char *statusline = "HTTP/1.1 200 OK";
            rh.content_type = get_mime_type(resource_path);
            rh.content_length =  filesize;
            
            send_message(client_fd, statusline, rh, buffer);
            log_info("200 OK - Served: %s (%ld bytes)", resource_path, filesize);

            free(buffer);
        }
        else{
            send(client_fd, http_404_response, strlen(http_404_response), 0);
            log_error("404 Not Found - Could not open: %s", resource_path);
        }
    } else {
        log_warn("Unsupported HTTP method: %s", method);
        send(client_fd, http_400_response, strlen(http_400_response), 0);
    }
}
