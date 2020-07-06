#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define VTP_NUM 335

int main() {
	int x=0;
	unsigned long vaddr=(unsigned long)&x;
	unsigned long paddr=0;

	__asm__ __volatile__("syscall":"=a"(paddr):"a"(VTP_NUM), "D"(vaddr));
	if(!paddr) {
		perror("fatal in syscall");
		exit(-1); }
	printf("virtual address:	%p\n"
	       "physical address:	%p\n",
	       (void *)vaddr, (void *)paddr);

	return 0; }
