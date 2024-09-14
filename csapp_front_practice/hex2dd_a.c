#include "../csapp.h"
//gcc -o hex2dd_a hex2dd_a.c ../csapp.c

int main(int argc, char **argv) {
	struct in_addr inaddr; /*네트워크 바이트 오더의 주소*/
	uint16_t addr; /*호스트 바이트 오더의 주소*/
	char buf[MAXBUF]; /*dotted-decimal 문자열의 버퍼*/

	if (argc != 2) {
		fprintf(stderr, "usage: %s <hex number>\n", argv[0]);
		exit(0);
	}
	sscanf(argv[1], "%x", &addr);
	inaddr.s_addr = htons(addr);

	if(!inet_ntop(AF_INET, &inaddr, buf, MAXBUF))
		unix_error("inet_ntop");
	printf("%s\n", buf);

	exit(0);
}

// 0x400 -> 4.0.0.0
