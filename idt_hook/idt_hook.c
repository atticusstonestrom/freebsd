//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/systm.h>


#define BP_INT 0x03
#define IDT_ENTRY_SIZE 8
struct idt_entry {
	unsigned long val1;
	unsigned long val2; };
struct idtr_t {
	unsigned short lim_val;
	unsigned long addr; }
	__attribute__((packed))
	idtr;

static int
mkdir_hook(struct thread *td, void *args) {
	//struct mkdir_args {
	//	char *path;
	//	int mode; };
	struct mkdir_args *uap=args;
	char path[255];
	size_t done;
	int error;
	if( (error=copyinstr(uap->path, path, 255, &done)) ) {
		return error; }
	uprintf("the directory \"%s\" will be created with the following permissions: %o\n",
		path, uap->mode);

	__asm__ __volatile__ (
		"sidt idtr;"
		: //"=r"(idtr)
		:: "memory");
	uprintf("idtr: addr: %p, lim_val: 0x%x \n", (void *)idtr.addr, idtr.lim_val);
	struct idt_entry idte;
	for(int i=0; i<=20; i++) {
		memcpy(&idte, (void *)(idtr.addr+i*sizeof(struct idt_entry)), sizeof(struct idt_entry));
		uprintf("idt entry %d:\t0x%lx%016lx\n",
			i, idte.val2, idte.val1); }
	
	return sys_mkdir(td, args); }


static int
load(struct module *module, int cmd, void *arg) {
	int error=0;
	switch(cmd) {
		case MOD_LOAD:
			sysent[SYS_mkdir].sy_call=(sy_call_t *)mkdir_hook;
			uprintf("sysent[] @\t%p\n", &sysent);
			uprintf("mkdir @\t\t%p\n", sys_mkdir);
			uprintf("mkdir_hook @\t%p\n", mkdir_hook);
			break;
		case MOD_UNLOAD:
			sysent[SYS_mkdir].sy_call=(sy_call_t *)sys_mkdir;
			break;
		default:
			error=EOPNOTSUPP;
			break; }
	return error; }

static moduledata_t mkdir_hook_mod = {
	"mkdir_hook",
	load,
	NULL };

DECLARE_MODULE(mkdir_hook, mkdir_hook_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
