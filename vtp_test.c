#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/module.h>

int main(int argc, char **argv) {
        int syscall_num;
        int modid;
        struct module_stat stat;
        if(argc!=2) {
                printf("usage:\t%s <virtual address to resolve>\n", argv[0]);
                exit(0); }
        stat.version=sizeof(stat);
        if((modid=modfind("sys/vtp"))==-1) {
                perror("fatal in modfind");
                exit(-1); }
        if(modstat(modid, &stat)==-1) {
                perror("fatal in modstat");
                exit(-1); }
        syscall_num=stat.data.intval;
        printf("%d\n", syscall_num);
        return syscall(syscall_num, argv[1]); }
        /*if((modid=modfind("hello"))==-1) {
                perror("fatal in modfind");
                exit(-1); }
        printf("%d\n", modid);
        if(modstat(modid, &stat)==-1) {
                perror("fatal in modstat");
                exit(-1); }
        return syscall(210, argv[1]); }*/
