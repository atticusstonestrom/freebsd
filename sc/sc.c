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

/////////////////////////////////////////////////////
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("hooks mkdirat to give virtual to physical addressing");
MODULE_VERSION("0.01");
#define SC_NUM "335"	//eventually write a parser for the relevant file
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
//need to be careful if there are so
//many arguments that stack is required
/*unsigned short tr;
struct __attribute__((packed)) {
	unsigned short lim_val;
	void *addr; }	//make struct gdte_t
	gdtr;*/
__attribute__((__used__))
unsigned long hook(void) {
	struct __attribute__((packed)) {
		unsigned short lim_val;
		void *addr; }	//make struct gdte_t
		gdtr={0};
	unsigned short tr=0;
	printk("\n");		//why is this necessary :(

	__asm__ __volatile__("sgdt %0"::"m"(gdtr));
	printk("[*]  gdtr dump\n"
	       "[**] address:\t0x%px\n"
	       "[**] lim val:\t0x%x\n"
	       "[*]  end dump\n\n",
	       gdtr.addr, gdtr.lim_val);
	
	//__asm__ __volatile__("str (tr)");
	__asm__ __volatile__("str %0"::"m"(tr));
	printk("[*] tr:\t0x%x\n\n", tr);
	
	//vol 3a.7-5: pg 3035
	struct __attribute__((packed)) {
		unsigned short seg_lim_0_15;
		unsigned short base_addr_0_15;
		unsigned char base_addr_16_23;		//interrupt stack table
		unsigned char type:4;
		unsigned char zero_12:1;
		unsigned char dpl:2;			//descriptor privilege level
		unsigned char p:1;			//present flag
		unsigned char seg_lim_16_19:4;
		unsigned char avl:1;			//available for use
		unsigned char zero_20_21:2;
		unsigned char granularity:1;
		unsigned char base_addr_24_31;
		unsigned int base_addr_32_63;
		unsigned int rsv; }
		*tssd;
	tssd=(void *)((unsigned long)gdtr.addr+tr);

	//vol 3a.7-19: pg3050
	struct __attribute__((packed)) {
		unsigned int rsv_0_3;
		unsigned long rsp0;
		unsigned long rsp1;
		unsigned long rsp2;
		unsigned long rsv_28_35;
		unsigned long ist1;
		unsigned long ist2;
		unsigned long ist3;
		unsigned long ist4;
		unsigned long ist5;
		unsigned long ist6;
		unsigned long ist7;
		unsigned long rsv_92_99;
		unsigned short rsv_100_101;
		unsigned short io_map_base_addr; }
		*tss;

	tss=(void *)(0
		| ((long)(tssd->base_addr_0_15))
		| ((long)(tssd->base_addr_16_23)<<16)
		| ((long)(tssd->base_addr_24_31)<<24)
		| ((long)(tssd->base_addr_32_63)<<32));
	printk("[*]  tss descriptor:\n"
	       "[**] addr:\t0x%lx\n"
	       "[**] seg limit:\t0x%x\n"
	       "[**] type:\t%d\n"
	       "[**] dpl:\t%d\n"
	       "[**] p:\t\t%d\n"
	       "[**] avl:\t%d\n"
	       "[**] granular:\t%d\n"
	       "[*]  end dump\n\n",
	       (unsigned long)tss, tssd->seg_lim_0_15|(tssd->seg_lim_16_19<<16), 
	       tssd->type, tssd->dpl, tssd->p, tssd->avl, tssd->granularity);
	printk("[*] rsp1:\t0x%lx\n"
	       "[*] rsp2:\t0x%lx\n\n",
	       tss->rsp1, tss->rsp2);
	
	union __attribute__((packed)) {
		struct __attribute__((packed)) {
			unsigned int eax;
			unsigned int edx; };
		unsigned long val; }
		gs_base;
	__asm__ __volatile__("rdmsr":"=a"(gs_base.eax), "=d"(gs_base.edx):"c"(0xc0000102));
	printk("[*] original gs base:\t0x%lx\n", gs_base.val);
	__asm__ __volatile__(
		"swapgs;"
		"rdmsr;"
		"swapgs;"
		:"=a"(gs_base.eax), "=d"(gs_base.edx)
		:"c"(0xc0000102));
	printk("[*] ia32_kernel_gs_base:\t0x%lx\n\n", gs_base.val);
	
	
	//*to_fill=paddr;
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
union __attribute__((packed)) {
	struct __attribute__((packed)) {
		unsigned int eax;
		unsigned int edx; };
	unsigned long val; }
	old_lstar, new_lstar;


static int __init
vtp_init(void) {
	old_lstar.val=0;
	__asm__ __volatile__("rdmsr":"=a"(old_lstar.eax), "=d"(old_lstar.edx):"c"(0xc0000082));
	printk("[*] ia32_lstar:\t0x%lx", old_lstar.val);
	printk("[*] asm_hook:\t0x%lx", (unsigned long)&asm_hook);
	printk("\n");
	new_lstar.val=(unsigned long)&asm_hook;
	__asm__ __volatile__("wrmsr"::"c"(0xc0000082), "a"(new_lstar.eax), "d"(new_lstar.edx));
	return 0; }


static void __exit
vtp_fini(void) {
	__asm__ __volatile__("wrmsr"::"c"(0xc0000082), "a"(old_lstar.eax), "d"(old_lstar.edx));
	return; }

module_init(vtp_init);
module_exit(vtp_fini);
/////////////////////////////////////////////////////
