#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

struct vtp_args {
        void *vaddr; };

static int
vtp(struct thread *td, void *args) {
        //struct vtp_args *uap=args;
        void *cr3=NULL;
        //void *vaddr=uap->vaddr;
        long word=0;
        __asm__ __volatile__ (
                "mov %%cr3, %%rax;"
                "mov %%rax, %0;"
                //"mov %2, %%rdx;"
                "mov %%cr0, %%rbx;"
                "btr $31, %%ebx;"
                "mov %%rbx, %%cr0;"
                "mov (%%rax), %%rdx;"
                "btc $31, %%ebx;"
                "mov %%rbx, %%cr0;"
                "mov %%rdx, $1"
                : "=r"(cr3), "=r"(word)
                : /*"r"(vaddr)*/
                : "rax", "rdx", "rbx");
        uprintf("word: 0x%lx @ cr3: %p\n", word, cr3);
        return 0; }

static
struct sysent vtp_sysent = {
        1,
        vtp };

static int offset=NO_SYSCALL;

static int
load(struct module *module, int cmd, void *arg) {
        int error=0;
        switch(cmd) {
                case MOD_LOAD:
                        uprintf("loading syscall at offset %d\n", offset);
                        break;
                case MOD_UNLOAD:
                        uprintf("unloading syscall from offset %d\n", offset);
                        break;
                default:
                        error=EOPNOTSUPP;
                        break; }
        return error; }

SYSCALL_MODULE(vtp, &offset, &vtp_sysent, load, NULL);
