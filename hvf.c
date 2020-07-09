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
#include "utilities-backup.h"

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


/////////////////////////////////////////////////////
unsigned long print_vtp(unsigned long vaddr, unsigned long *to_fill) {
	////////////////////////////////////////////////////////////////////
	//asm block checks to see if 4 or 5-level paging is enabled
	//if so, moves the cr3 register into the cr3 variable
	//and sets la57_flag to assert whether 4-level or 5-level
	int la57_flag=0;
	unsigned long cr3=0;
	__asm__ __volatile__ ("mov %%cr3, %0;":"=r"(cr3));
	if(!cr3) {
		return -EOPNOTSUPP; }
	////////////////////////////////////////////////////////////////////
	unsigned long psentry=0, paddr=0;
	////////////////////////////////////////////////////////////////////
	//pml5e (if applicable)
	if(la57_flag) {			//5-level paging
		printk("[debug]: &pml5e:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(cr3)|(PML5_MASK(vaddr)>>45) ));
		psentry=*(unsigned long *)\
			phys_to_virt( PE_ADDR_MASK(cr3)|(PML5_MASK(vaddr)>>45) );
		printk("[debug]: pml5e:\t0x%lx\n", psentry);
		if(!PE_P_FLAG(psentry)) {
			return -EFAULT; }}
	else {
		psentry=cr3; }
	////////////////////////////////////////////////////////////////////
	//pml4e
	printk("[debug]: &pml4e:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PML4_MASK(vaddr)>>36) ));
	psentry=*(unsigned long *)\
		phys_to_virt( PE_ADDR_MASK(psentry)|(PML4_MASK(vaddr)>>36) );
	printk("[debug]: pml4e:\t0x%lx\n", psentry);
	if(!PE_P_FLAG(psentry)) {
		return -EFAULT; }
	////////////////////////////////////////////////////////////////////
	//pdpte
	printk("[debug]: &pdpte:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PDPT_MASK(vaddr)>>27) ));
	psentry=*(unsigned long *)\
		phys_to_virt( PE_ADDR_MASK(psentry)|(PDPT_MASK(vaddr)>>27) );
	printk("[debug]: pdpte:\t0x%lx\n", psentry);
	if(PE_PS_FLAG(psentry)) {	//1GB page
		//bits (51 to 30) | bits (29 to 0)
		paddr=(psentry&0x0ffffc00000000)|(vaddr&0x3fffffff);
		*to_fill=paddr;
		return 0; }
	if(!PE_P_FLAG(psentry)) {
		return -EFAULT; }
	////////////////////////////////////////////////////////////////////
	//pde
	printk("[debug]: &pde:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PD_MASK(vaddr)>>18) ));
	psentry=*(unsigned long *)\
		phys_to_virt( PE_ADDR_MASK(psentry)|(PD_MASK(vaddr)>>18) );
	printk("[debug]: pde:\t0x%lx\n", psentry);
	if(PE_PS_FLAG(psentry)) {	//2MB page
		//bits (51 to 21) | bits (20 to 0)
		paddr=(psentry&0x0ffffffffe0000)|(vaddr&0x1ffff);
		*to_fill=paddr;
		return 0; }
	if(!PE_P_FLAG(psentry)) {
		return -EFAULT; }
	////////////////////////////////////////////////////////////////////
	//pte
	printk("[debug]: &pte:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PT_MASK(vaddr)>>9) ));
	psentry=*(unsigned long *)\
		phys_to_virt( PE_ADDR_MASK(psentry)|(PT_MASK(vaddr)>>9) );
	printk("[debug]: pte:\t0x%lx\n", psentry);
	paddr=(psentry&0x0ffffffffff000)|(vaddr&0xfff);
	*to_fill=paddr;
	return 0; }
/////////////////////////////////////////////////////


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
	struct vtp_t vtp_s;
	/*printk("[*] asm_hook: 0x%px\n", &asm_hook);
	if(print_vtp((unsigned long)&asm_hook, &paddr)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] paddr: 0x%lx\n\n", paddr); }*/
	
	printk("[*] offset: 0x%lx\n", idte_offset);
	if(print_vtp(idte_offset, &paddr)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] paddr: 0x%lx\n\n", paddr); }

	//if(vtp(idte_offset, &paddr, &vtp_s)) {
		printk("[*] %d\n", vtp(idte_offset, &paddr, &vtp_s));
		//printk("[*] error\n\n"); }
	//else {
		printk("[*] offset: 0x%lx\n", idte_offset);
		printk("[debug]: &pml5e:\t0x%px\n", vtp_s.pml5e_p);
		//printk("[debug]: pml5e:\t0x%lx\n", *(unsigned long *)vtp_s.pml5e_p);
		printk("[debug]: &pml4e:\t0x%px\n", vtp_s.pml4e_p);
		//printk("[debug]: pml4e:\t0x%lx\n", *(unsigned long *)vtp_s.pml4e_p);
		printk("[debug]: &pdpte:\t0x%px\n", vtp_s.pdpte_p);
		//printk("[debug]: pdpte:\t0x%lx\n", *(unsigned long *)vtp_s.pdpte_p);
		printk("[debug]: &pde:\t0x%px\n", vtp_s.pde_p);
		//printk("[debug]: pde:\t0x%lx\n", *(unsigned long *)vtp_s.pde_p);
		printk("[debug]: &pte:\t0x%px\n", vtp_s.pte_p);
		//printk("[debug]: pte:\t0x%lx\n", *(unsigned long *)vtp_s.pte_p);
		printk("[*] paddr: 0x%lx\n\n", paddr); //}
		
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
