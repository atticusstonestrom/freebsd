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
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"incl counter;"
	"jmp *(idte_offset);");
extern void asm_hook(void);

static void
print_vtp_s(struct vtp_t *vtp_p) {
	if(vtp_p->pml5e_p) {
		printk("[debug]: &pml5e:\t0x%px\n", vtp_p->pml5e_p);
		printk("[debug]: pml5e:\t0x%lx\n", *(unsigned long *)(vtp_p->pml5e_p)); }
	if(vtp_p->pml4e_p) {
		printk("[debug]: &pml4e:\t0x%px\n", vtp_p->pml4e_p);
		printk("[debug]: pml4e:\t0x%lx\n", *(unsigned long *)(vtp_p->pml4e_p)); }
	if(vtp_p->pdpte_p) {
		printk("[debug]: &pdpte:\t0x%px\n", vtp_p->pdpte_p);
		printk("[debug]: pdpte:\t0x%lx\n", *(unsigned long *)(vtp_p->pdpte_p)); }
	if(vtp_p->pde_p) {
		printk("[debug]: &pde:\t0x%px\n", vtp_p->pde_p);
		printk("[debug]: pde:\t0x%lx\n", *(unsigned long *)(vtp_p->pde_p)); }
	if(vtp_p->pte_p) {
		printk("[debug]: &pte:\t0x%px\n", vtp_p->pte_p);
		printk("[debug]: pte:\t0x%lx\n", *(unsigned long *)(vtp_p->pte_p)); }
	return; }


static int __init
idt_init(void) {
	READ_IDT(idtr)
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
	       
	unsigned long paddr;
	struct vtp_t vtp_s={0};
	
	vtp_s=(struct vtp_t){0};
	if(vtp((unsigned long)&asm_hook, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] asm_hook: 0x%px\n", &asm_hook);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); }
	
	unsigned long pte_p=(unsigned long)(vtp_s.pte_p);
	vtp_s=(struct vtp_t){0};
	if(vtp(pte_p, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] &pte: 0x%lx\n", pte_p);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); }

	vtp_s=(struct vtp_t){0};
	if(vtp(idte_offset, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] offset: 0x%lx\n", idte_offset);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); }
		
	/*union msr_t ia32_lstar;
	READ_MSR(ia32_lstar, 0xc0000082)
	printk("[*] ia32_lstar:\t0x%lx", ia32_lstar.val);
	if(print_vtp(ia32_lstar.val, &paddr)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] paddr: 0x%lx\n\n", paddr); }*/

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
