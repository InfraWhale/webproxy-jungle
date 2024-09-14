#include <stdio.h>
#include <arpa/inet.h>

int main() {
	char ip_str[INET_ADDRSTRLEN];
	struct in_addr ip_addr;

	scanf("%s", ip_str);

	// 저장은 네트워크 바이트 순서로 되지만, 시스템의 호스트 바이트 순서를 따라서 출력될 수 있다.
    if (inet_pton(AF_INET, ip_str, &ip_addr) == 1) { // IP 주소를 바이너리로 변환
        printf("0x%x\n", htonl(ip_addr.s_addr)); // 네트워크 바이트 순서로 바꿔준다.
    } else {
        printf("fail.\n");
        return 1;
    }
}
