#include "../csapp.h"

// gcc -o hostinfo_q hostinfo_q.c ../csapp.c
// 빨간줄 떠도 컴파일은 된다

// argc : argument count 
//  - 프로그램 실행 시 전달된 인수의 개수
//  - 프로그램의 이름도 인수이다 -> 최소값은 1
// argv : argument vector
//  - argv[0] : 실행한 프로그램 이름
//  - argv[1], argv[2] ... 인수들이 순서대로 들어간다.
//  - 마지막 인수 다음은 NULL 포인터가 저장된다. 
int main(int argc, char **argv) { 
    struct addrinfo *p, *listp, hints;
    char buf[MAXLINE];
    int rc, flags;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    /* addrinfo 기록에서 리스트를 가져온다. */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; /*IPv4만 검색*/
    hints.ai_socktype = SOCK_STREAM; /*Connections 만 검색*/
    if (( rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo error : %s\n", gai_strerror(rc));
        exit(1);
    }

    /* 모든 리스트 순회 & 모든 IP 주소 표시 */
    flags = NI_NUMERICHOST; /* 도메인명 대신 주소가 들어가는 문자열로 바꾼다. */
    for(p = listp; p; p = p->ai_next) {
        struct sockaddr_in *sa = (struct sockaddr_in *)p->ai_addr;
        // printf("sa : %p\n", sa);
        // printf("sa->sin_addr : %p\n", sa->sin_addr);
        // printf("&(sa->sin_addr) : %p\n", &(sa->sin_addr));
        if(inet_ntop(AF_INET, &(sa->sin_addr), buf, MAXLINE) != NULL) { // 두 번째 인자는 저 값을 담는 주솟값이 들어오도록 만들어졌으므로, &를 붙혀준다.
            printf("%s\n", buf);
        }
    }

    /*리스트 free 해주기*/
    Freeaddrinfo(listp);

    exit(0);
}
