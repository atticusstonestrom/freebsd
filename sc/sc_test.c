#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SC_NUM 335

int main() {
	if( syscall(SC_NUM) ) {
		perror("fatal in syscall");
		exit(-1); }
	return 0; }
