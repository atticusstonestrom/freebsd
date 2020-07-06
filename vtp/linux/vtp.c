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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("hooks mkdirat to give virtual to physical addressing");
MODULE_VERSION("0.01");

/////////////////////////////////////////////////////
//virtual address masks
//pg 2910 of intel developers' manual
#define PML5_MASK(x)	((x)&0x01ff000000000000)	//bits 56 to 48
#define PML4_MASK(x)	((x)&0x0000ff8000000000)	//bits 47 to 39
#define PDPT_MASK(x)	((x)&0x0000007fc0000000)	//bits 38 to 30
#define PD_MASK(x)	((x)&0x000000003fe00000)	//bits 29 to 21
#define PT_MASK(x)	((x)&0x00000000001ff000)	//bits 20 to 12
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
//page structure entry masks
#define PE_ADDR_MASK(x)	((x)&0x000ffffffffff000)	//bits 51 to 12
#define PE_PS_FLAG(x)	( (x) & ((long)1<<7) )		//page size flag
#define PE_P_FLAG(x) 	((x)&1)				//present flag
/////////////////////////////////////////////////////

union __attribute__((packed)) {
	struct __attribute__((packed)) {
		unsigned int eax;
		unsigned int edx; };
	unsigned long val; }
	old_lstar, new_lstar;

int counter=0;
void hook(void) {
	counter++;
	printk("[*] in the hook!\n");
	return; }
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"pushf;"
	//"cmp $83, %rax;"
	"cmp $335, %rax;"
	"jne end;"
	"swapgs;"
	"push %rcx;"
	//"incl counter;"
	"call hook;"
	"pop %rcx;"
	"popf;"
	"swapgs;"
	"mov $0, %rax;"
	"sysretq;"
"end:;"
	"popf;"
	"jmp *(old_lstar);");
extern void asm_hook(void);

static int __init
vtp_init(void) {
	unsigned long cr0=0;
	old_lstar.val=0;
	__asm__ __volatile__ ("mov %%cr0, %0":"=r"(cr0));
	//get ia32_lstar
	__asm__ __volatile__("rdmsr":"=a"(old_lstar.eax), "=d"(old_lstar.edx):"c"(0xc0000082));
	printk("[*] cr0:\t\t0x%lx\n", cr0);
	printk("[*] ia32_lstar:\t0x%lx", old_lstar.val);
	printk("[*] asm_hook:\t0x%lx", (unsigned long)&asm_hook);
	new_lstar.val=(unsigned long)&asm_hook;
	__asm__ __volatile__("wrmsr"::"c"(0xc0000082), "a"(new_lstar.eax), "d"(new_lstar.edx));
	return 0; }
		
/*
	int la57_flag=0;
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
		return EOPNOTSUPP; }


static int
hook(int dirfd, const char *pathname, mode_t mode) {
	printk("[*] pathname: %s\n"
	       "[*] permissions: %o\n",
	       pathname, mode);
	return (*original_call)(dirfd, pathname, mode); }*/

//static int __init
//vtp_init(void) {
	//printk("[*] %px\n", *SYS_CALL_TABLE);
	//printk("[*] %px\n", &hook);
	/*original_call=SYS_CALL_TABLE[__NR_mkdirat];
	printk("[*] old: %px\n"
	       "[*] new: %px\n",
	       original_call, &hook);*/
	/*unsigned long cr0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(long)0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
	SYS_CALL_TABLE[__NR_mkdirat]=&hook;
	cr0 |= 0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));*/
	//return 0; }

static void __exit
vtp_fini(void) {
	__asm__ __volatile__("wrmsr"::"c"(0xc0000082), "a"(new_lstar.eax), "d"(new_lstar.edx));
	printk("[*] counter:\t%d\n", counter);
	return;
	/*unsigned long cr0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(long)0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
	SYS_CALL_TABLE[__NR_mkdirat]=original_call;
	cr0 |= 0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));*/ }

module_init(vtp_init);
module_exit(vtp_fini);
