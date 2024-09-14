#include "../csapp.h"
//gcc -o dd2hex_a dd2hex_a.c ../csapp.c

int main(int argc, char **argv) {
	struct in_addr inaddr; /*네트워크 바이트 오더의 주소*/
	int rc;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <dotted-decimal>\n", argv[0]);
		exit(0);
	}

	rc = inet_pton(AF_INET, argv[1], &inaddr);

	if (rc == 0)
		app_error("inet_pton error : invalid dotted-decimal address");
	else if (rc < 0)
		unix_error("inet_pton error");
	printf("0x%x\n", ntohs(inaddr.s_addr));
	exit(0);
}

// 0x400 -> 4.0.0.0
