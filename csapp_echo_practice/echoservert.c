#include "../csapp.h"

// gcc -o echoservert echoservert.c ../csapp.c

// 호스트명 검색 : hostnamectl

void *thread(void *vargp);

void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;
    
    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("while start!\n");
        if(strcmp(buf, "exit\n") == 0)
            break;
        
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
        printf("while end!\n");
    }
}

int main(int argc, char **argv) {
    int listenfd, *connfdp; // connfd로 바로 받지 말고, 포인터로 받는다.
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    
    char client_hostname[MAXLINE], client_port[MAXLINE];
    
    pthread_t tid; // 쓰레드 식별자

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    // printf("Open_listenfd success!!!\n");

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int)); // 메모리 할당해놓는다.
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        Pthread_create(&tid, NULL, thread, connfdp);

    }
}

/* 쓰레드 루틴*/
void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    echo(connfd);
    Close(connfd);
    return NULL;
}