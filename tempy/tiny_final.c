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
#include <signal.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int headflag);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd; // 듣기 & 연결 파일 디스크럽터
  char hostname[MAXLINE], port[MAXLINE]; // 호스트명, 포트

  struct sockaddr_storage clientaddr; // 클라이언트 주소
  socklen_t clientlen; // 클라이언트 길이
  signal(SIGPIPE, SIG_IGN); // SIGPIPE 신호 무시

  /* 커멘드 라인에 포트번호 입력되었는지 확인 */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 듣기 파일디스크럽터 준비 */
  listenfd = Open_listenfd(argv[1]);

  while (1) {
    /* 연결 파일디스크럽터 준비 */
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    
    /* 연결 정보 불러오기 */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    /* 작업 수행 */
    doit(connfd);   // line:netp:tiny:doit

    /* 연결 파일디스크럽터 종료 */
    Close(connfd);  // line:netp:tiny:close
  }
}

/* 실제 작업이 일어나는 함수 */
void doit(int fd) {
  int is_static, is_head;
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

  /* HEAD 메서드인 경우 헤더만 주고받을수 있도록 플래그값 변경 */
  if (!strcasecmp(method, "HEAD")) {
    is_head = 1;
  /* GET 메서드가 아닌 경우 501 에러 */
  } else if (strcasecmp(method, "GET")) {
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
    serve_static(fd, filename, sbuf.st_size, is_head);
  }
  else { /*dynamic content를 제공한다.*/
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

/* 클라이언트 에러 메시지를 만든다. */
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

/* 요청 헤더를 읽어서 화면에 표시해준다. */
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* uri에서 filename, cgiargs를 가져온다. */
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

/* static 콘텐츠를 가져온다. */
void serve_static(int fd, char *filename, int filesize, int headflag) {
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

  /* HEAD 메서드인 경우, 이 이후는 수행할 필요 없다. */
  if(headflag)
    return;

  /* response body를 클라이언트에게 보낸다.*/
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = malloc(MAXBUF);

  int n;
  while ((n = rio_readn(srcfd, srcp, MAXBUF)) > 0) {
      rio_writen(fd, srcp, n);
  }
  Close(srcfd);
  
  free(srcp);
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

/* dynamic 콘텐츠를 가져온다. */
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