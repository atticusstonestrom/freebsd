//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("Hooks the zero divisor IDT entry");
MODULE_VERSION("0.01");


struct idte_t {
	unsigned short offset_0_15;
	unsigned short segment_selector;
	unsigned char ist;			//interrupt stack table
	unsigned char type:4;
	unsigned char zero_12:1;
	unsigned char dpl:2;			//descriptor privilege level
	unsigned char p:1;			//present flag
	unsigned short offset_16_31;
	unsigned int offset_32_63;
	unsigned int rsv; }
	__attribute__((packed))
	*zd_idte;

#define ZD_INT 0x00
unsigned long idte_offset;			//contains absolute address of original interrupt handler
struct idtr_t {
	unsigned short lim_val;
	struct idte_t *addr; }
	__attribute__((packed))
	idtr;

int counter=0;
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"incl counter;"
	"jmp *(idte_offset);");
extern void asm_hook(void);

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


/////////////////////////////////////////////////////
unsigned long vtp(unsigned long vaddr, unsigned long *to_fill) {
	////////////////////////////////////////////////////////////////////
	//asm block checks to see if 4 or 5-level paging is enabled
	//if so, moves the cr3 register into the cr3 variable
	//and sets la57_flag to assert whether 4-level or 5-level
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
	__asm__ __volatile__ (
		"cli;"
		"sidt %0;"
		"sti;"
		:: "m"(idtr));
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

	unsigned long cr0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(long)0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
	__asm__ __volatile__("cli");
	zd_idte->offset_0_15=((unsigned long)(&asm_hook))&0xffff;
	zd_idte->offset_16_31=((unsigned long)(&asm_hook)>>16)&0xffff;
	zd_idte->offset_32_63=((unsigned long)(&asm_hook)>>32)&0xffffffff;
	__asm__ __volatile__("sti");
	cr0 |= 0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
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
	printk("[*] asm_hook: 0x%px\n", &asm_hook);
	if(vtp((unsigned long)&asm_hook, &paddr)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] paddr: 0x%lx\n\n", paddr); }
	
	printk("[*] offset: 0x%lx\n", idte_offset);
	if(vtp(idte_offset, &paddr)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] paddr: 0x%lx\n\n", paddr); }
		
	union __attribute__((packed)) {
		struct __attribute__((packed)) {
			unsigned int eax;
			unsigned int edx; };
		unsigned long val; }
		ia32_lstar;
	__asm__ __volatile__("rdmsr":"=a"(ia32_lstar.eax), "=d"(ia32_lstar.edx):"c"(0xc0000082));
	printk("[*] ia32_lstar:\t0x%lx", ia32_lstar.val);
	if(vtp(ia32_lstar.val, &paddr)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] paddr: 0x%lx\n\n", paddr); }
	
	
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
	unsigned long cr0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	cr0 &= ~(long)0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0));
	__asm__ __volatile__("cli");
	zd_idte->offset_0_15=idte_offset&0xffff;
	zd_idte->offset_16_31=(idte_offset>>16)&0xffff;
	zd_idte->offset_32_63=(idte_offset>>32)&0xffffffff;
	__asm__ __volatile__("sti");
	cr0 |= 0x10000;
	__asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0)); }

module_init(idt_init);
module_exit(idt_fini);
