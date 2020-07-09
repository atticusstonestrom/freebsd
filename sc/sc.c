//////////////////////////////////////////////////////
//most relevant information can be found in chapter //
//4 of volume 3A of the intel developers' manual,   //
//which starts at pg 2910                           //
//////////////////////////////////////////////////////
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <asm/io.h>
#include "utilities.h"

/////////////////////////////////////////////////////
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("hooks mkdirat to give virtual to physical addressing");
MODULE_VERSION("0.01");
//#define SC_NUM "335"	//eventually write a parser for the relevant file
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
//need to be careful if there are so
//many arguments that stack is required
unsigned long hook(void) {
	//check if previous rsp was kernel mode or user mode (&0xfff0000000000000?)
	//replace tss->rs2 with previous rsp?
	struct tss_t *tss=get_tss();
	printk("[*] rsp1:\t0x%lx\n"
	       "[*] rsp2:\t0x%lx\n\n",
	       tss->rsp1, tss->rsp2);
	
	union __attribute__((packed)) {
		struct __attribute__((packed)) {
			unsigned int eax;
			unsigned int edx; };
		unsigned long val; }
		gs_base;
	READ_MSR(gs_base, 0xc0000102)
	printk("[*] original gs base:\t0x%lx\n", gs_base.val);
	__asm__ __volatile__("swapgs;");
	READ_MSR(gs_base, 0xc0000102)
	__asm__ __volatile__("swapgs;");
	printk("[*] ia32_kernel_gs_base:\t0x%lx\n\n", gs_base.val);

	return 0; }
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"pushf;"
	"cmp $"SC_NUM", %rax;"
	"jne end;"
	"swapgs;"
	"push %rcx;"
	"call hook;"	//should move current %rsp to tss->rsp2
	"pop %rcx;"
	"popf;"
	"swapgs;"
	"sysretq;"
"end:;"
	"popf;"
	"jmp *(old_lstar);");
extern void asm_hook(void);
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
union msr_t old_lstar, new_lstar;


static int __init
vtp_init(void) {
	old_lstar.val=0;
	READ_MSR(old_lstar, 0xc0000082)
	printk("[*] ia32_lstar:\t0x%lx", old_lstar.val);
	printk("[*] asm_hook:\t0x%lx", (unsigned long)&asm_hook);
	printk("\n");
	new_lstar.val=(unsigned long)&asm_hook;
	WRITE_MSR(new_lstar, 0xc0000082)
	return 0; }


static void __exit
vtp_fini(void) {
	WRITE_MSR(old_lstar, 0xc0000082)
	return; }

module_init(vtp_init);
module_exit(vtp_fini);
/////////////////////////////////////////////////////
