/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

/***********************************************************8
*  Cite: http://www.gnu.org/software/libc/manual/html_node/Server-Example.html
*        https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/selectserver.c
*/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "parse.h"
#define ECHO_PORT 9999
#define BUF_SIZE 40960
#define MAX_CLIENTS 1025
#define TYPE_LEN 64
#define PATH_LEN 128 //does it have request about path len?
#define DATE_LEN 64

int client_sock[MAX_CLIENTS+2], client_close[MAX_CLIENTS+2], buff_len[MAX_CLIENTS + 2];
char buff[MAX_CLIENTS+2][BUF_SIZE+9];
int connections; 
FILE *fp;
char *logfile;
char *wwwpath;
int port;

void server_log(FILE *f, char *msg){
  time_t t;
  struct tm* tm;
  t = time(NULL);
  tm = localtime(&t);
  fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            tm->tm_year+1900,
            tm->tm_mon+1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec
  );
  fprintf(f, "%s\n",msg);
}

int close_socket(int id){
    if (close(id)){
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    connections --;
    return 0;
}

// Close socket for the client id 
int close_client(int id){
    int fd = client_sock[id];
    client_sock[id] = 0;
    client_close[id] = 1;
    return close_socket(fd);    
}

void free_request(Request *request){
  free(request->headers);
  free(request);
}

// check http version, if not HTTP/1.1, then exit
int check_http_version(int id, char* request){
  if(strcasecmp(request, "HTTP/1.1")){
    close_socket(id);
    server_log(fp, "505_HTTP_VERSION_NOT_SUPPORTED");
    server_log(fp,"505: client request a http server not supported by the server.");
    return -1;
  }
  return 0;
}

int check_implemented(int id, char* request){

  int res = (strcasecmp(request, "GET") != 0) &&
            (strcasecmp(request, "POST") != 0) &&
            (strcasecmp(request, "HEAD") != 0);
  if(res){
    close_socket(id);
    server_log(fp, "501: Function Not Implement.");
    return -1;
  }
  return 0;
}

void parseURI(Request *request, char *filename){
  strcpy(filename,wwwpath);
  char *uri = request->http_uri;
  if(uri[strlen(uri)-1] == '/'){
    strcat(filename,"index.html");
  }
}

int postRequestHandler(int id, Request *request){
  struct tm tm;
  struct stat sbuf;
  char content[BUF_SIZE],lastModified[DATE_LEN], date[DATE_LEN];
  
  tm=*gmtime(&sbuf.st_mtime);
  strftime(lastModified, DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);
  time_t now = time(0);
  tm = *gmtime(&now);
  strftime(date, DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);

  sprintf(content,"HTTP/1.1 204 No Content\r\n");
  sprintf(content, "%sServer:Liso/1.0\r\n\r\n", content);
  sprintf(content, "%sDate: %s\r\n", content, date);
  if(!client_close[id]){
     sprintf(content, "%sConnection: keep-alive\r\n", content);
  }

   sprintf(content, "%sContent-Length: 0\r\n",content);
   sprintf(content, "%sContent-Type: text/html\r\n", content);
  
 
  send(client_sock[id],content,strlen(content),0);
  return 0;
}

void parseFileType(char *filename, char *filetype){
  if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".css"))
        strcpy(filetype, "text/css");
    else if (strstr(filename, ".js"))
        strcpy(filetype, "application/javascript");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg") || strstr(filename, "jpeg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".wav"))
        strcpy(filetype, "audio/x-wav");
    else
        strcpy(filetype, "text/plain");
}







void serveError(int id, char *errorNum, char *shortMsg, char *longMsg){

    struct tm tm;
    time_t now;
    char header[BUF_SIZE], body[BUF_SIZE], dbuf[TYPE_LEN];

    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, TYPE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);

    // response header
    sprintf(header, "HTTP/1.1 %s %s\r\n", errorNum, shortMsg);
    sprintf(header, "%sDate: %s\r\n", header, dbuf);
    sprintf(header, "%sServer: Liso/1.0\r\n", header);
    if (client_close[id]) 
        sprintf(header, "%sConnection: close\r\n", header);
    sprintf(header, "%sContent-type: text/html\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, (int)strlen(body));
    
    // response body
    sprintf(body, "<html><title>Liso Error</title>");
    sprintf(body, "%s<body>\r\n", body);
    sprintf(body, "%sError %s -- %s\r\n", body, errorNum, shortMsg);
    sprintf(body, "%s<br><p>%s</p></body></html>\r\n", body, longMsg);

    printf("serverError header is %s\n\n, body is %s\n\n, id is %d\n\n",header, body, client_sock[id]);
    send(client_sock[id], header, strlen(header), 0);
    send(client_sock[id], body, strlen(body), 0);
}


int validateFile(int id, Request *request){
    struct stat sbuf;
    char filename[BUF_SIZE];

    parseURI(request, filename);
    // check if file exist
    if (stat(filename, &sbuf) < 0)
    {
        serveError(id, "404", "Not Found",
                    "Couldn't find this file.");
        return 0;
    }

    // check we have permission
    if ((!S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
        serveError(id, "403", "Forbidden",
                    "Cannot read this file.");
        return 0;
    }

    return 1;
}











int headRequestHandler(int id, Request *request){

  // printf("Head Request Handler, http_version is %s\n", request->http_version);
  // printf("Head Request Handler, http_method is %s\n", request->http_method);
  // printf("Head Request Handler, http_uri is %s\n", request->http_uri);

  printf("enter head !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n\n\n\n" );
  struct tm tm;
  struct stat sbuf;
  char content[BUF_SIZE], filename[BUF_SIZE],filetype[PATH_LEN],
       lastModified[DATE_LEN], date[DATE_LEN];

  //check whether the request id is validate or not
  //xxxxxx
  if (validateFile(id, request) == 0) return 1;

  parseURI(request,filename);//need to check???
  stat(filename,&sbuf);
  parseFileType(filename,filetype);
  
  tm=*gmtime(&sbuf.st_mtime);
  strftime(lastModified, DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);
  time_t now = time(0);
  tm = *gmtime(&now);
  strftime(date, DATE_LEN, "%a, %d %b %Y %H:%M:%S %Z", &tm);

  sprintf(content,"HTTP/1.1 200 OK\r\n");
  sprintf(content, "%sServer:Liso/1.0\r\n\r\n", content);
  sprintf(content, "%sDate: %s\r\n", content, date);
  if(!client_close[id]){
     sprintf(content, "%sConnection: keep-alive\r\n", content);
  }

  sprintf(content, "%sContent-Length: %lld\r\n", content, sbuf.st_size);
  sprintf(content, "%sContent-Type: %s\r\n", content, filetype);
  sprintf(content, "%sLast-Modified: %s\r\n", content, lastModified);
 
  printf("headRequestHandler content is %s\n, id is %d\n", content, id);
  send(client_sock[id],content,strlen(content),0);

  
  return 0;
}


int getRequestHandler(int id, Request *request){
   
    headRequestHandler(id,request);


    int fd, filesize;
    char *ptr;
    char filename[BUF_SIZE];
    struct stat sbuf;
     
    parseURI(request, filename);

    if ((fd = open(filename, O_RDONLY, 0)) < 0)
    {
        fprintf(fp,"Error: Cann't open file \n");
        return -1;
    }

    stat(filename, &sbuf);
    filesize = sbuf.st_size;
    ptr = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    int bytes;
    bytes = send(client_sock[id], ptr, filesize, 0); 
    munmap(ptr, filesize);

    return 0;
}


// requestHandler from Sarah
int requestHandler(int id){

  printf("Request handler buff is %s\n,lenth is %d\n",buff[id],buff_len[id]);
  printf("requestHandler id is %d, client_sock[id] is %d\n", id, client_sock[id]);
  Request *request = parse(buff[id], buff_len[id],id);

  if (check_http_version(id, request->http_version)){
    free_request(request);
    return -1;
  }

  if (check_implemented(id, request->http_method)){
    free_request(request);
    return -1;
  }

  if(!strcasecmp(request->http_method, "GET")){
    getRequestHandler(id,request);
  }

  if(!strcasecmp(request->http_method, "POST")){
    postRequestHandler(id,request);
  }

  if(!strcasecmp(request->http_method, "HEAD")){
    headRequestHandler(id,request);
  }
  free_request(request);
  return 0;
}

/*
 * parent_sock is used to binding 
 * child_sock is used to connection
 */
int main(int argc, char* argv[]){
    // if(argc != 4){
    //   fprintf(stderr, "Failed creating socket.\n");
    // }
    if(argc!=4){
      printf("ERROR, should have 4 arguments!");
      return EXIT_FAILURE;
    }

    port = atoi(argv[1]);
    logfile = argv[2];
    wwwpath = argv[3];



    connections = 0;
    int parent_sock, child_sock, fdmax;
    ssize_t nbytes;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    fd_set readfds; 
    fd_set master;
    //fp=fopen(logfile,"w");
    fp = stdout;
    fprintf(stdout, "----- Echo Server -----\n");

    int i;
    for (i = 0; i < MAX_CLIENTS; i ++){
        client_sock[i] = 0;
        buff_len[i] = 0;
        client_close[i] = 1;
    }

    if ((parent_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1){
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    client_sock[0] = parent_sock;
    client_close[0] = 0;
    connections ++;

    server_log(fp,"Server-socket() is OK...\n");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(parent_sock, (struct sockaddr *) &addr, sizeof(addr)) == -1){
        close_socket(parent_sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    server_log(fp, "Server-bind() is OK...\n");

    if (listen(parent_sock, 5)){
      close_socket(parent_sock);
      fprintf(stderr, "Error listening on socket.\n");
      return EXIT_FAILURE;
    }

    server_log(fp, "Server-listen() is OK...\n");

    
    
    /* finally, loop waiting for input and then write it back */
  while (1){
      FD_ZERO(&readfds);
      FD_SET(parent_sock, &readfds);
      fdmax = parent_sock;

      for (i = 0; i < 512; i ++){
         if (client_sock[i] > 0)
             FD_SET(client_sock[i], &readfds);
         if (client_sock[i] > fdmax)
             fdmax = client_sock[i];
      }

      server_log(fp, "selection 1.");
      printf("fdmax is %d\n", fdmax);
      if (select(fdmax + 1, &readfds, NULL, NULL, NULL ) == -1){
        server_log(fp, "Error in Selection.");
        return EXIT_FAILURE;
      }
      server_log(fp, "selection 2.");

      server_log(fp,"Server-select() is OK...\n");



      if (FD_ISSET(parent_sock, &readfds)){
         if ((child_sock = accept(parent_sock, (struct sockaddr *) &cli_addr, &cli_size)) == -1){
             close(parent_sock);
             printf("Error accepting connection.\n");
             return EXIT_FAILURE;
         }
         connections ++;
         int i;
         for (i = 0; i < 512; i++){
             if (client_sock[i] == 0){
                 client_sock[i] = child_sock;
                 buff_len[i] = 0;
                 client_close[i] = 0;
                 memset(buff[i], 0, BUF_SIZE);
                 break;
             }
         }
         if (connections == MAX_CLIENTS){
             client_close[i] = 1;
             serveError(child_sock, "503", "Service Unavailable", "Exceed maximum connection number limit.");
             close_client(i);
         }    
      }

      for(i = 0; i <= 512; i ++){
          int id = client_sock[i];
          if (FD_ISSET(id, &readfds)){
              cli_size = sizeof(cli_addr);
              if((nbytes = recv(id, buff[i], BUF_SIZE, 0)) >= 1){
                  buff_len[i] = nbytes;
                  printf("Get int handler\n");
                  //requestHandler(i);
              }

              printf("buff is %s\n id is %d\n",buff[i], id);
              if(nbytes < 0){
                server_log(fp, "Error reading from client socket.\n");
                return EXIT_FAILURE;
              }
              // close_socket(id);
              // FD_CLR(id, &master);
              

              if (client_close[i] == 1){
                   if (close_client(i))
                   {
                       close_socket(parent_sock);
                       printf("Error closing client socket.\n");
                       return EXIT_FAILURE;
                   }
              }
          }
       }
       

    }
    close_socket(parent_sock);
    return EXIT_SUCCESS;
}


