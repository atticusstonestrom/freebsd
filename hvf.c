//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/io.h>
#include "utilities.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("Hooks the zero divisor IDT entry");
MODULE_VERSION("0.01");


#define ZD_INT 0x00
#define BP_INT 0x03
struct idte_t *idte;			//points to the start of the IDT
unsigned long zd_handler;		//contains absolute address of division error IRQ handler
unsigned long bp_handler;		//contains absolute address of soft breakpoint IRQ handler
#define STUB_SIZE 0x2b			//includes extra 8 bytes for the old value of cr3
unsigned char orig_bytes[STUB_SIZE];	//contains the original bytes of the division error IRQ handler
struct idtr_t idtr;			//holds base address and limit value of the IDT

int counter=0;
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"incl counter;"
	"movq (bp_handler), %rax;"
	"ret;");
extern void asm_hook(void);

__asm__(
	".text;"
	".global stub;"
"stub:;"
	"push %rax;"	//bp_handler	
	"push %rbx;"	//new cr3, &asm_hook
	"push %rdx;"	//old cr3
	"mov %cr3, %rdx;"
	"mov .CR3(%rip), %rbx;"
	"mov %rbx, %cr3;"
	"mov $asm_hook, %rbx;"
	"call *%rbx;"
	"mov %rdx, %cr3;"
	"pop %rdx;"
	"pop %rbx;"
	"xchg %rax, (%rsp);"
	"ret;"
".CR3:;"
	//will be filled with a valid value of cr3 during module initialization
	".quad 0xdeadbeefdeadbeef;");
extern void stub(void);

static int __init
idt_init(void) {
	READ_IDT(idtr)
	printk("[*]  idtr dump\n"
	       "[**] address:\t0x%px\n"
	       "[**] lim val:\t0x%x\n"
	       "[*]  end dump\n\n",
	       idtr.addr, idtr.lim_val);
	idte=(idtr.addr);

	zd_handler=0
		| ((long)((idte+ZD_INT)->offset_0_15))
		| ((long)((idte+ZD_INT)->offset_16_31)<<16)
		| ((long)((idte+ZD_INT)->offset_32_63)<<32);
	printk("[*]  idt entry %d:\n"
	       "[**] addr:\t0x%px\n"
	       "[**] segment:\t0x%x\n"
	       "[**] ist:\t%d\n"
	       "[**] type:\t%d\n"
	       "[**] dpl:\t%d\n"
	       "[**] p:\t\t%d\n"
	       "[*]  end dump\n\n",
	       ZD_INT, (void *)zd_handler, zd_idte->segment_selector, 
	       zd_idte->ist, zd_idte->type, zd_idte->dpl, zd_idte->p);
	if(!zd_idte->p) {
		printk("[*] fatal: handler segment not present\n");
		return ENOSYS; }
		
	bp_handler=0
		| ((long)((idte+BP_INT)->offset_0_15))
		| ((long)((idte+BP_INT)->offset_16_31)<<16)
		| ((long)((idte+BP_INT)->offset_32_63)<<32);
	printk("[*]  breakpoint handler:\t0x%lx\n\n", bp_handler);


	unsigned long cr3;
	__asm__ __volatile__("mov %%cr3, %0":"=r"(cr3)::"memory");
	printk("[*] cr3:\t0x%lx\n\n", cr3);

	memcpy(orig_bytes, (void *)zd_handler, STUB_SIZE);
	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	memcpy((void *)zd_handler, &stub, STUB_SIZE);
	*(unsigned long *)(zd_handler+STUB_SIZE-8)=cr3;
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION

	return 0; }

static void __exit
idt_fini(void) {
	printk("[*] counter: %d\n\n", counter);

	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	memcpy((void *)zd_handler, orig_bytes, STUB_SIZE);
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION }

module_init(idt_init);
module_exit(idt_fini);
