// CSCI 324: Implementing basic HTTP web server
// Luke Reichold, 2014

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Predefined static response headers
const char* const HTTP_OK_HEAD = "HTTP/1.1 200 OK\n";
const char* const HTTP_403 = "HTTP/1.1 403 Forbidden\nContent-Length: 166\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this webserver.\n</body></html>\n";
const char* const HTTP_404 = "HTTP/1.1 404 Not Found\nContent-Length: 146\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>404 Not Found</h1>\nThe requested resource was not found on the server.\n</body></html>\n";
const char* const CONT_TYPE_BASE = "Content-Type: ";
const char* const CONT_TYPE_HTML = "Content-Type: text/html\n\n";
const char* const CONTENT_LEN_BASE = "Content-Length: ";
const char* const SERVER = "Reipache 1.0\n";
const char* const CONN_CLOSE = "Connection: close\n";
const char* const HTTP_DATE_RESP_FORMAT = "%a, %d %b %Y %H:%M:%S %Z";
const char* const INDEX_PAGE_BODY = "<html><body><h1>It works!</h1><h3>This is the default web page for the \"Reipache\" server.</h3><hr>Luke Reichold, 2014 - luke@reikam.com</body></html>";

// Size constants
#define DATESTAMP_LENGTH    30
#define BUFFER_SIZE         1024

// Supported resource extensions / content-types
struct {
   char *ext;
   char *content_type;
} extensions [] = {
   {"gif", "image/gif" },
   {"jpg", "image/jpg" },
   {"jpeg","image/jpeg"},
   {"png", "image/png" },
   {"ico", "image/ico" },
   {"htm", "text/html" },
   {"html","text/html" },
   {0,0} };

const char* get_file_ext(const char* filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

int get_content_length(FILE *fp) {
   int size = 0;
   fseek(fp, 0, SEEK_END);
   size = ftell(fp);
   rewind(fp);
   return size;
}

void getDate(char* dateStr) {
   char buf[DATESTAMP_LENGTH];
   time_t now = time(0);
   struct tm tm = *(gmtime(&now));
   strftime(buf, sizeof(buf), HTTP_DATE_RESP_FORMAT, &tm);
   strcat(dateStr, buf);
   strcat(dateStr, "\n");
}

void* processRequest(void* newSock) {

   printf("processRequest:\n");
   int s =  *((int *)newSock);
   char buffer[BUFFER_SIZE];
   char *method;
   char *path;

   // Get the request body from the socket
   if (recv(s, buffer, BUFFER_SIZE-1, 0) == -1) {
      printf("Error handling incoming request \n");
      close(s);
      return NULL;
   }
   // If response is nil, don't try to parse it.
   if (buffer[0] == '\0') {
      close(s);
      return NULL;
   }

   // Print request to console
   printf("%s\n", buffer);
   
   method = strtok(buffer, " ");
   path = strtok(NULL, " ");
 
   // If request type is not GET, send back 403 since it's currently unsupported.
   if (strcasecmp(method, "GET") != 0) {
      write(s, HTTP_403, strlen(HTTP_403));
      close(s);
      return(NULL);
   }

   // Get file extension
   const char* ext;
   ext = get_file_ext(path);

   printf("Requested resource path: %s\n", path);
   printf("File extension: %s\n", ext);

   // Disallow relative paths to parent directories
   if (path[1] == '.') {
      write(s, HTTP_403, strlen(HTTP_403));
      close(s);
      return(NULL);
   }

   // Format the response date
   char date[DATESTAMP_LENGTH + 6] = "Date: ";
   getDate(date);

   // If path is '/', serve the default index page and return
   if (strcmp(path, "/") == 0) {
      write(s, HTTP_OK_HEAD, strlen(HTTP_OK_HEAD));
      write(s, SERVER, strlen(SERVER));
      char defaultContentLength[40];
      sprintf(defaultContentLength, "%s %lu\n", CONTENT_LEN_BASE, strlen(INDEX_PAGE_BODY));
      write(s, defaultContentLength, strlen(defaultContentLength));
      write(s, CONN_CLOSE, strlen(CONN_CLOSE));
      write(s, date, strlen(date));
      write(s, CONT_TYPE_HTML, strlen(CONT_TYPE_HTML));
      write(s, INDEX_PAGE_BODY, strlen(INDEX_PAGE_BODY));
      close(s);
      return NULL;
   }

   // Determine content type from file extenion
   int i;
   char *ctype = NULL;
   for (i=0; extensions[i].ext != 0; i++) {
      if (strcmp(ext, extensions[i].ext) == 0) {
         ctype = extensions[i].content_type;
         break;
      }
   }

   // Either filename given doesn't have an extension, or the content type isn't supported.
   if (ctype == NULL) {
      write(s, HTTP_403, strlen(HTTP_403));
      close(s);
      return(NULL);
   }

   // Remove leading slash from path
   path++;

   // Try to open the requested file
   FILE *fp;
   fp = fopen(path, "rb");
   if (fp == NULL) {
      write(s, HTTP_404, strlen(HTTP_404));
      close(s);
      return(NULL);
   } else {
      printf("file opened successfully\n");
   }

   // If we reached here, the requested resource was found and opened.
   int contentLength;
   contentLength = get_content_length(fp);
   if (contentLength < 0) {
      printf("Length is requested file < 0. Returning 403.\n");
      write(s, HTTP_403, strlen(HTTP_403));
      close(s);
      return(NULL);
   }

   // Write back headers and content
   write(s, HTTP_OK_HEAD, strlen(HTTP_OK_HEAD));
   write(s, SERVER, strlen(SERVER));

   // Format content length
   char contentLengthString[40];
   sprintf(contentLengthString, "%s %d\n", CONTENT_LEN_BASE, contentLength);
   write(s, contentLengthString, strlen(contentLengthString));

   write(s, CONN_CLOSE, strlen(CONN_CLOSE));
   write(s, date, strlen(date));

   // Format the content type
   char contentType[80];
   sprintf(contentType, "%s %s\n\n", CONT_TYPE_BASE, ctype);
   write(s, contentType, strlen(contentType));

   // Write each byte of the file to the socket
   int current_char = 0;
   do {
      current_char = fgetc(fp);
      write(s, &current_char, sizeof(char));
   } while (current_char != EOF);

   close(s);
   return NULL;
}

int main(int argc, char *argv[]) {

   if (argc < 3) {
      printf("Usage: web-server port path/to/files \n");
      printf("For the document root, simply enter '/' to use the current directory. \n");
      exit(1);
   }

   int sock, new_socket, status;
   socklen_t addrlen;
   struct sockaddr_in address;
   unsigned short port = (unsigned short) strtoul(argv[1], NULL, 0);

   // If not simply using server's directory, do some security checks
   if (strcmp(argv[2], "/") != 0) {
      if (argv[2][0] == '.') {
         printf("Error: Desired document root must be relative to server, and cannot enter parent directories.\n");
         exit(1);
      }

      if (chdir(argv[2]) < 0) { 
         printf("Error: Unable to open directory %s\n", argv[2]);
         printf("NOTE: Only paths relative to this program's directory are allowed.\n");
         exit(1);
      }
   }

   // Create a new socket
   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
      printf("The socket was created\n");
   }

   // Configure address attributes
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(port);
    
   // Bind our socket to an address
   if (bind(sock, (struct sockaddr *) &address, sizeof(address)) == 0) {
      printf("Successful binding\n");
   } else {
      perror("bind: error binding socket");
      exit(1);
   }

   pthread_t thread;
   while (1) {
      // Enable connection requests on the socket (allow backlog of 10 requests)
      if (listen(sock, 10) < 0) {
         perror("server: listen");
         exit(1);
      }

      if ((new_socket = accept(sock, (struct sockaddr *) &address, &addrlen)) < 0) {    
         perror("server: error accepting connection");
         continue;
      }
      else if (new_socket > 0) {
         printf("Client connection is active...\n");
      }

      // Each request handled on its own thread
      if (pthread_create(&thread, NULL, processRequest, &new_socket)) {
         perror("Error creating thread");
         continue;
      }

      pthread_detach(thread);
   }

   close(sock);
   return 0;
}
