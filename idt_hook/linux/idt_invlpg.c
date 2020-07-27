//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include "utilities.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("Hooks the zero divisor IDT entry");
MODULE_VERSION("0.01");


struct idte_t *zd_idte;

#define ZD_INT 0x00
unsigned long idte_offset;			//contains absolute address of original interrupt handler
struct idtr_t idtr;

int counter=0;
__attribute__((__used__))
static void
hook(void) {
	printk("[*] in the hook! counter %d\n", ++counter);
	return; }

__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"push %rdi;"
	"mov %rsp, %rdi;"
	"mov 32(%rsp), %rsp;"
	PUSHA
	"swapgs;"
	"call hook;"
	"swapgs;"
	POPA
	"mov %rdi, %rsp;"
	"pop %rdi;"
	"jmp *(idte_offset);");
extern void asm_hook(void);


static int __init
idt_init(void) {
	READ_IDT(idtr);
	printk("[*]  idtr dump\n"
	       "[**] address:\t0x%px\n"
	       "[**] lim val:\t0x%x\n"
	       "[*]  end dump\n\n",
	       idtr.addr, idtr.lim_val);
	zd_idte=(idtr.addr)+ZD_INT;

	idte_offset=0
		| ((long)(zd_idte->offset_0_15))
		| ((long)(zd_idte->offset_16_31)<<16)
		| ((long)(zd_idte->offset_32_63)<<32);
	printk("[*]  old idt entry %d:\n"
	       "[**] addr:\t0x%px\n"
	       "[**] segment:\t0x%x\n"
	       "[**] ist:\t%d\n"
	       "[**] type:\t%d\n"
	       "[**] dpl:\t%d\n"
	       "[**] p:\t\t%d\n"
	       "[*]  end dump\n\n",
	       ZD_INT, (void *)idte_offset, zd_idte->segment_selector, 
	       zd_idte->ist, zd_idte->type, zd_idte->dpl, zd_idte->p);
	if(!zd_idte->p) {
		printk("[*] fatal: handler segment not present\n");
		return ENOSYS; }

	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	zd_idte->offset_0_15=((unsigned long)(&asm_hook))&0xffff;
	zd_idte->offset_16_31=((unsigned long)(&asm_hook)>>16)&0xffff;
	zd_idte->offset_32_63=((unsigned long)(&asm_hook)>>32)&0xffffffff;
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION
	printk("[*]  new idt entry %d:\n"
	       "[**] addr:\t0x%px\n"
	       "[**] segment:\t0x%x\n"
	       "[**] ist:\t%d\n"
	       "[**] type:\t%d\n"
	       "[**] dpl:\t%d\n"
	       "[**] p:\t\t%d\n"
	       "[*]  end dump\n\n",
	       ZD_INT, (void *)(\
	       (long)zd_idte->offset_0_15|((long)zd_idte->offset_16_31<<16)|((long)zd_idte->offset_32_63<<32)),
	       zd_idte->segment_selector, zd_idte->ist, zd_idte->type, zd_idte->dpl, zd_idte->p);
	
	//uprintf("%p\n", &asm_hook);
	/*unsigned short cs;
	__asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
	uprintf("cs: 0x%x\n", cs);
	for(int i=0; i<64; i++) {
		uprintf("idt entry %d:\t%p\n", i,
			(void *)((long)idtr.addr[i].offset_0_15|((long)idtr.addr[i].offset_16_31<<16)|((long)idtr.addr[i].offset_32_63<<32))); }*/

	return 0; }

static void __exit
idt_fini(void) {
	printk("[*] counter: %d\n\n", counter);
	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	zd_idte->offset_0_15=idte_offset&0xffff;
	zd_idte->offset_16_31=(idte_offset>>16)&0xffff;
	zd_idte->offset_32_63=(idte_offset>>32)&0xffffffff;
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION }

module_init(idt_init);
module_exit(idt_fini);
