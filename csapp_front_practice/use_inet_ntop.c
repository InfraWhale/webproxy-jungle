#include <stdio.h>
#include <arpa/inet.h>

int main() {
	int ip_addr;
	char ip_str[INET_ADDRSTRLEN];

	scanf("%x", &ip_addr); // 이때 호스트 바이트 순서로 입력된다.

    // 호스트 바이트 순서를 네트워크 바이트 순서로 변환
    ip_addr = htonl(ip_addr);

    if (inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN) != NULL) { // 바이너리를 IP 주소로 변환
        printf("%s\n", ip_str);
    } else {
        printf("fail\n");
    }
}
