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
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
__attribute__((__used__))
unsigned long hook(unsigned long vaddr, unsigned long *to_fill) {
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


/////////////////////////////////////////////////////
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"pushf;"
	"cmp $335, %rax;"
	"jne end;"
	"swapgs;"
	"push %rcx;"
	"call hook;"
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
