#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define VTP_NUM 335

int main() {
	int r=0;
	unsigned long vaddr=(unsigned long)&r;
	unsigned long paddr=0;

	__asm__ __volatile__("syscall":"=a"(r):"a"(VTP_NUM), "D"(vaddr), "S"(&paddr));
	if(r) {
		perror("fatal in syscall");
		exit(-1); }
	printf("virtual address:	%p\n"
	       "physical address:	%p\n",
	       (void *)vaddr, (void *)paddr);

	return 0; }
