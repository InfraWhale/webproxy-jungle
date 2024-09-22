#include <stdio.h>
#include "csapp.h"
#include <signal.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// gcc -o proxy proxy.c csapp.c

typedef struct cache_t {
  char host[MAXLINE]; /* 저장된 호스트명 */         
  char port[MAXLINE]; /* 저장된 포트 */
  char path[MAXLINE]; /* 저장된 경로*/
  char object[MAX_OBJECT_SIZE]; /* 저장된 객체 */
  int size;
  struct cache_t *prev;       /* 이전 위치 */
  struct cache_t *next;       /* 다음 위치 */
} cache_t;

cache_t *start;
cache_t *end;
int cache_size = 0;

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *host, char *port, char *path, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void send_get_request_from_server(int clientfd, char *hostname, char *path, char *method);

void *thread(void *vargp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  pthread_t tid; // 쓰레드 식별자
  

  signal(SIGPIPE, SIG_IGN); // SIGPIPE 신호 무시
  // SIGPIPE : 프로세스가 파이프 또는 소켓을 통해 데이터를 쓰려고 했을 때, 상대 프로세스가 종료되거나 연결을 닫았을 경우 발생하는 신호.
  // 이 신호가 발생 시, 기본적으로 프로그램은 종료된다.
  // 이 신호를 무시할 경우 프로그램은 종료되지 않으나, write()또는 send()는 오류를 반환한다.  

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /*캐시 시작점 초기화*/
  start = malloc(sizeof(cache_t));
  end = malloc(sizeof(cache_t));
  start->next = end;
  end->prev = start;

  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int)); // 메모리 할당한다.
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    Pthread_create(&tid, NULL, thread, connfdp);
  }
}

/* 쓰레드 루틴*/
void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int fd) {
  int is_static, is_head, client_fd;
  struct stat sbuf;
  char buf_from[MAXLINE], buf_to[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], path[MAXLINE], cgiargs[MAXLINE];
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
  is_static = parse_uri(uri, host, port, path, cgiargs);
  
  printf("host : %s, port : %s, path : %s, cgiargs : %s\n", host, port, path, cgiargs);

  /* 캐시에 있는지 확인 */
  /* 있으면 가져온다 */
  cache_t *find_pos = start->next;
    while (find_pos != end) {
      if( !strcmp(host, find_pos->host) && !strcmp(port, find_pos->port) && !strcmp(path, find_pos->path) ) {
        Rio_writen(fd, find_pos->object, find_pos->size);
        find_pos->prev->next = find_pos->next;
        find_pos->next->prev = find_pos->prev;

        start->next->prev = find_pos;
        find_pos->next = start->next;

        start->next = find_pos;
        find_pos->prev = start;
        return;
      } else {
        find_pos = find_pos->next;
      }
    }

  client_fd = Open_clientfd(host, port);

  sprintf(buf_to, "GET %s HTTP/1.0\r\n\r\n", path); // HTTP 요청 작성
  Rio_writen(client_fd, buf_to, strlen(buf_to)); // 서버로 요청 전송

  // send_get_request_from_server(client_fd, host, path, method);

  int n;
  int now_size = 0;
  char temp_object[MAX_OBJECT_SIZE];  // 캐시에 저장할 임시 객체
  while ((n = Rio_readn(client_fd, buf_from, MAXLINE)) > 0) {
    Rio_writen(fd, buf_from, n);
    memcpy(temp_object + now_size, buf_from, n);  // 임시 객체에 데이터 누적 저장
    now_size += n;
  }
  
  /*캐시 저장 로직*/
  if(now_size <= MAX_OBJECT_SIZE) {
    /*초과시 식제*/
    while( cache_size + now_size > MAX_CACHE_SIZE ) {
      cache_t *del_pos = end->prev;
      if (del_pos != start) { // 시작점인지 체크
        cache_size -= del_pos->size;
        del_pos->prev->next = end;
        end->prev = del_pos->prev;
        free(del_pos);
      }
    }
    
    cache_t *new_cache = malloc(sizeof(cache_t));
    strcpy(new_cache->host, host);
    strcpy(new_cache->port, port);
    strcpy(new_cache->path, path);

    memcpy(new_cache->object, temp_object, now_size);  // 임시 객체 내용을 캐시로 복사
    new_cache->size = now_size;

    start->next->prev = new_cache;
    new_cache->next = start->next;

    start->next = new_cache;
    new_cache->prev = start;

    cache_size += now_size;
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

int parse_uri(char *uri, char *host, char *port, char *path, char *cgiargs) {
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
    strcpy(path, slash_pos);
    *slash_pos = '\0';
  } else {
    strcpy(path, "/");
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