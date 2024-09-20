#include <stdio.h>
#include "csapp.h"
#include <signal.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// gcc -o tiny tiny.c

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *host, char *port, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void send_get_request_from_server(int clientfd, char *hostname, char *path, char *method);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  signal(SIGPIPE, SIG_IGN); // SIGPIPE 신호 무시
  // SIGPIPE : 프로세스가 파이프 또는 소켓을 통해 데이터를 쓰려고 했을 때, 상대 프로세스가 종료되거나 연결을 닫았을 경우 발생하는 신호.
  // 이 신호가 발생 시, 기본적으로 프로그램은 종료된다.
  // 이 신호를 무시할 경우 프로그램은 종료되지 않으나, write()또는 send()는 오류를 반환한다.  

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  
  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  int is_static, is_head, client_fd;
  struct stat sbuf;
  char buf_from[MAXLINE], buf_to[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio_from, rio_to;

  /* request line과 header를 읽는다. */
  rio_readinitb(&rio_from, fd);
  Rio_readlineb(&rio_from, buf_from, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf_from);
  sscanf(buf_from, "%s %s %s", method, uri, version);
  //printf("method : %s, uri : %s, version : %s\n", method, uri, version);

  if (!strcasecmp(method, "HEAD")) {
    is_head = 1;
  } else if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // read_requesthdrs(&rio_from);

  /* GET 요청으로부터 URI를 파싱한다. */
  is_static = parse_uri(uri, host, port, filename, cgiargs);
  
  printf("host : %s, port : %s, filename : %s, cgiargs : %s\n", host, port, filename, cgiargs);

  // client_fd = Open_clientfd(host, port);
  // Rio_readinitb(&rio_to, client_fd);

  // sprintf(buf_to, "GET %s HTTP/1.0\r\n\r\n", filename); // HTTP 요청 작성
  // Rio_writen(client_fd, buf_to, strlen(buf_to)); // 서버로 요청 전송

  // // send_get_request_from_server(client_fd, host, filename, method);

  // while (Rio_readlineb(&rio_to, buf_from, MAXLINE) >0) {
  //   Rio_writen(fd, buf_from, strlen(buf_from)); // 서버 응답을 클라이언트로 전송
  // }

  client_fd = Open_clientfd(host, port);

  sprintf(buf_to, "GET %s HTTP/1.0\r\n\r\n", filename); // HTTP 요청 작성
  Rio_writen(client_fd, buf_to, strlen(buf_to)); // 서버로 요청 전송

  // send_get_request_from_server(client_fd, host, filename, method);

  int n;
  while ((n = Rio_readn(client_fd, buf_from, MAXLINE)) > 0) {
      Rio_writen(fd, buf_from, n);
  }

  Close(client_fd); // 서버와 연결 종료
}

void clienterror(int fd, char *cause, char*errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* HTTP Response Body를 만든다.*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* HTTP response 를 출력한다. */
  sprintf(buf, "HTTP/1.0 %s %s\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  if(Rio_readlineb(rp, buf, MAXLINE) <= 0)
    return;
  while(strcmp(buf, "\r\n")) {
    if(Rio_readlineb(rp, buf, MAXLINE) <= 0)
    return;
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *host, char *port, char *filename, char *cgiargs) {
  char *slash_pos;
  int ret;

  /*http, https 제거*/
  if( strncmp(uri, "http://", 7) == 0 ) {
    memmove(uri, uri + 7, strlen(uri) - 7 + 1);
  } else if( strncmp(uri, "https://", 8) == 0 ) {
    memmove(uri, uri + 8, strlen(uri) - 8 + 1);
  } 

  slash_pos = strchr(uri, '&'); // '&' 문자의 위치
  if (slash_pos != NULL) {
    strcpy(cgiargs, slash_pos);
    *slash_pos = '\0';
    ret = 1;
  } else {
    strcpy(cgiargs, "");
    ret = 0;
  }

  slash_pos = strchr(uri, '/'); // '/' 문자의 위치
  if (slash_pos != NULL) {
    strcpy(filename, slash_pos);
    *slash_pos = '\0';
  } else {
    strcpy(filename, "/");
  }
  
  slash_pos = strchr(uri, ':'); // ':' 문자의 위치
  if (slash_pos != NULL) {
    strcpy(port, slash_pos+1);
    *slash_pos = '\0';
  } else {
    strcpy(port, "80");
  }

  strcpy(host, uri);

  return ret;
}

void send_get_request_from_server(int clientfd, char *hostname, char *path, char *method) {
    char buf[MAXLINE];
    // Prepare the GET request message
    sprintf(buf, "%s %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "Proxy-Connection: close\r\n"
             "%s\r\n"
             "\r\n", method, path,
             hostname, user_agent_hdr);
    // Send the GET request to the server
    Rio_writen(clientfd, buf, strlen(buf));
}