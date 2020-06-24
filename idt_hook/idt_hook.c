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
	return mkdir(td, syscall_args); }
	////////////////////////////////////////////////////////////////////
	//asm block checks to see if 4 or 5-level paging is enabled
	//if so, moves the cr3 register into the cr3 variable
	//and sets la57_flag to assert whether 4-level or 5-level
	/*int la57_flag=0;
	unsigned long cr3=0;
	__asm__ __volatile__ (
		"mov %%cr0, %%rax;"		//check bit 31 of cr0 (PG flag)
		"test $0x80000000, %%eax;"	//deny request if 0
		"jz fail;"			//(ie if paging is not enabled)

		"mov $0xc0000080, %%ecx;"	//check bit 8 of ia32_efer (LME flag)
		"rdmsr;"			//deny request if 0
		"test $0x100, %%eax;"		//(module currently can't handle pae paging)
		"jz fail;"
		
	"success:\n"
		"mov %%cr3, %0;"
		"mov %%cr4, %%rax;"
		"shr $20, %%rax;"
		"and $1, %%rax;"
		"mov %%eax, %1;"
		"jmp break;"
	"fail:\n"
		"mov $0, %0;"
	"break:\n"
	
		: "=r"(cr3), "=r"(la57_flag)
		::"rax", "ecx", "memory");
	if(!cr3) {
		return EOPNOTSUPP; }}*/

static int
load(struct module *module, int cmd, void *arg) {
	int error=0;
	switch(cmd) {
		case MOD_LOAD:
			sysent[SYS_mkdir].sy_call=(sy_call_t *)mkdir_hook;
			break;
		case MOD_UNLOAD:
			sysent[SYS_mkdir].sy_call=(sy_call_t *)mkdir;
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
