#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/module.h>

int main(int argc, char **argv) {
	unsigned long vaddr;
	if(argc!=2) {
		printf("usage:\t%s <virtual address to resolve>\n", argv[0]);
		exit(0); }
	vaddr=strtoul(argv[1], NULL, 16);
	unsigned long paddr=0;

	int syscall_num;
	int modid;
	struct module_stat stat;
	stat.version=sizeof(stat);
	if((modid=modfind("sys/vtp"))==-1) {
		perror("fatal in modfind");
		exit(-1); }
	if(modstat(modid, &stat)==-1) {
		perror("fatal in modstat");
		exit(-1); }
	syscall_num=stat.data.intval;
	
	if(syscall(syscall_num, vaddr, &paddr)) {
		perror("fatal in syscall");
		exit(-1); }
	printf("virtual address:	%p\n"
	       "physical address:	%p\n",
	       (void *)vaddr, (void *)paddr);
	return 0; }
