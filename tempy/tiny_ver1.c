/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

// gcc -o tiny tiny.c
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  // struct timeval timeout; /*timeout 추가*/

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  // // 타임아웃 설정 (10초로 설정 예시)
  // timeout.tv_sec = 10;  // 10초
  // timeout.tv_usec = 0;  // 마이크로초 단위 (0으로 설정)

  // // 소켓에 수신 타임아웃 설정
  // setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

  // // 소켓에 송신 타임아웃 설정
  // setsockopt(listenfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* request line과 header를 읽는다. */
  rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* GET 요청으로부터 URI를 파싱한다. */
  is_static = parse_uri(uri, filename, cgiargs);
  
  // stat
  // 각 함수들의 호출 성공시 0을 반환하며 두번째 인자인 stat 구조체에 파일 정보들로 채워진다.
  // 실패 혹은 에러시 -1을 리턴하고 에러시에 errno 변수에 에러 상태가 set된다.
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) { /* static content를 제공한다.*/
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else { /*dynamic content를 제공한다.*/
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
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

  Rio_readlineb(rp, buf, MAXLINE); // while문 밖에서 굳이 한번 더해주는 이유가?
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { /* 정적 콘텐츠 */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else { /* 동적 콘텐츠 */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else 
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* 클라이언트에게 response header를 보낸다. */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // \r\n : 각 헤더 라인의 끝
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // \r\n\r\n : 헤더와 본문의 구분
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* response body를 클라이언트에게 보낸다.*/
  srcfd = Open(filename, O_RDONLY, 0);
  printf("Open success!\n");
  // Mmap() : 메모리 매핑
  // addr : 커널에게 파일 어디 매핑할지 제안하는 값 (보통 0)
  // len - filesize : 메모리 영역 길이
  // prot - PROT_READ : 메모리 보호 정책. 지금은 읽기 가능한 페이지
  // flags - MAP_PRIVATE : 매핑유형, 동작구성요소.
  // MAP_PRIVATE - 매핑을 공유하지 않는다. 파일은 쓰기 후 복사로 매핑되고,
  // 변경된 메모리속성은 실제 파일에는 반영되지 않는다. (왜쓰는거?)
  // srcfd - 연결할 파일 디스크립터
  // offset - 매핑 시 len의 시작점
  /* srcp =Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); */

  int n;
  while ((n = Rio_readn(srcfd, buf, MAXBUF)) > 0) {
      Rio_writen(fd, buf, n);
      printf("Successfully wrote %d bytes to client\n", n);
  }
  if (n < 0) {
      printf("Error reading from file\n");
  }
  printf("Finished serving static content\n");
  Close(srcfd);
  /*
  Rio_writen(fd, srcp, filesize);
  // Munmap() : 메모리 매핑 제거
  Munmap(srcp, filesize);
  */
}

/**
 * get_filetype - 파일명에서 파일타입을 생성한다.
 *  */ 
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* HTTP response의 첫 부분을 반환한다.*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /*자식 프로세스에서 실행된다.*/
    setenv("QUERY_STRING", cgiargs, 1); /*환경변수를 cgiargs 대로 덮어씌운다.*/
    Dup2(fd, STDOUT_FILENO); /* stdout에서 클라이언트로 리다이렉트 한다. */
    Execve(filename, emptylist, environ); /* CGI 프로그램 작동 */
  }
  Wait(NULL); /*child 종료시까지 parent가 대기한다.*/
}