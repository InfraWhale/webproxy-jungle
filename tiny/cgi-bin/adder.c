/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "../csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /*인자 2개를 추출한다.*/
  if ((buf = getenv("QUERY_STRING")) != NULL) { // 환경변수 중 QUERY_STRING을 추출한다.
    p = strchr(buf, '&'); // buf에서 & 위치의 포인터를 리턴
    *p = '\0'; // 이거 왜해줌? split?
    strcpy(arg1, buf);
    strcpy(arg2, p+1);
    if (strncmp(arg1, "firstVal", strlen("firstVal")) == 0) {
      char *temp;
      temp = strchr(arg1, '=');
      strcpy(arg1, temp+1);
      
      if(!strcmp(arg1, "")) {
        strcpy(arg1, "0");
      }

      temp = strchr(arg2, '=');
      strcpy(arg2, temp+1);

      if(!strcmp(arg2, "")) {
        strcpy(arg2, "0");
      }
    }
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* response body를 만든다. */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* HTTP 응답을 만든다. */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout); // 지정된 출력 스트림과 연관된 버퍼를 비움

  exit(0);
}
/* $end adder */
