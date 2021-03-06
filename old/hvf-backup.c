//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include <asm/io.h>
#include "utilities.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("Hooks the zero divisor IDT entry");
MODULE_VERSION("0.01");


struct idte_t *zd_idte;

#define ZD_INT 0x00
unsigned long zd_handler;			//contains absolute address of original interrupt handler
unsigned long bp_handler;
#define STUB_SIZE 0x28
#define RSP1_OFFSET "28"	//STUB_SIZE-12
unsigned char orig_bytes[STUB_SIZE+8];
struct idtr_t idtr;

int counter=0;
/*__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	
	"push %rax;"		//struct tss_t *tss
	"push %rbx;"		//struct tssd_t *tssd
	"push %rdx;"		//placeholder
	"sub $12, %rsp;"
	"sgdt (%rsp);"
	"str 10(%rsp);"
	"movzwl 10(%rsp), %ebx;"
	"addq 2(%rsp), %rbx;"

	"movzwl 2(%rbx), %eax;"
	"movzbl 4(%rbx), %edx;"
	"shl $16, %rdx;"
	"or %rdx, %rax;"
	"movzbl 7(%rbx), %edx;"
	"shl $24, %rdx;"
	"or %rdx, %rax;"
	"mov 8(%rbx), %edx;"
	"shl $32, %rdx;"
	"or %rdx, %rax;"
	
	"add $12, %rsp;"
	"pop %rdx;"
	"lea 16(%rsp), %rbx;"	//original rsp
	
	"mov 12(%rax), %rsp;"	//lock sub 12(%rax)?
	"push 32(%rbx);"	//ss
	"push 24(%rbx);"	//rsp
	"push 16(%rbx);"	//rflags
	"push 8(%rbx);"		//cs
	"push (%rbx);"		//rip
	"push %rbx;"		//old rsp
	"mov -8(%rbx), %rax;"
	"mov -16(%rbx), %rbx;"

	"incl counter;"
	
	"push %rax;"
	"push %rbx;"
	"subq $8, 16(%rsp);"
	"movq (bp_handler), %rax;"
	"mov 16(%rsp), %rbx;"
	"mov %rax, (%rbx);"
	"pop %rbx;"
	"pop %rax;"
	"mov (%rsp), %rsp;"
	"ret;"
	
		"push %rax;"
		"push %rbx;"
		"mov 16(%rsp), %rax;"
		"movq (bp_handler), %rbx;"
		"mov %rbx, -8(%rax);"
		"pop %rbx;"
		"pop %rax;"
		"mov (%rsp), %rsp;"
		"sub $8, %rsp;"
		"ret;");
	//"jmp *(bp_handler);");
extern void asm_hook(void);*/
__asm__(
	".text;"
	".global stub;"
"stub:;"
		/*"push %rax;"
		//"push %rbx;"
		"mov %rsp, %rax;"
		"mov 32(%rsp), %rsp;"*/
//push, pop cr3
"movabs $0xdeadbeefdeadbeef, %rax;"	// $counter. phys_to_virt?
"mov %cr3, %rbx;"
"and $0xfffffffffffff000, %rbx;"
"and $0xfff, %rax;"
"and %rbx, %rax;"
"movq (%rax), %rdx;"
"incl counter;"				// not an absolute address?
	"push %rbx;"
	"push %rax;"
	"mov %rsp, %rax;"
	"movq "RSP1_OFFSET"(%rip), %rbx;"
	"movq (%rbx), %rsp;"
			//"movq (%rip), %rbx;"
	"incl counter;"
	"movq (bp_handler), %rbx;"
	"xchg %rbx, 8(%rax);"
	"mov %rax, %rsp;"
	"pop %rax;"
	"ret;");
	//"pushq (asm_hook);"
	//"add $8, %rsp;"	//
		/*"mov %rax, %rsp;"
		//"pop %rbx;"
		"pop %rax;"*/
	//"pushq $asm_hook;"
	//"ret;");
		//"jmpq asm_hook;");
		//"pushq $asm_hook;"
		//"ret;");
extern void stub(void);

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

struct page *mem_map;
static int __init
idt_init(void) {
	READ_IDT(idtr)
	printk("[*]  idtr dump\n"
	       "[**] address:\t0x%px\n"
	       "[**] lim val:\t0x%x\n"
	       "[*]  end dump\n\n",
	       idtr.addr, idtr.lim_val);
	zd_idte=(idtr.addr)+ZD_INT;

	zd_handler=0
		| ((long)(zd_idte->offset_0_15))
		| ((long)(zd_idte->offset_16_31)<<16)
		| ((long)(zd_idte->offset_32_63)<<32);
	bp_handler=0
		| ((long)((zd_idte+3)->offset_0_15))
		| ((long)((zd_idte+3)->offset_16_31)<<16)
		| ((long)((zd_idte+3)->offset_32_63)<<32);
	printk("[*]  old idt entry %d:\n"
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
	
	unsigned long cr3;
	__asm__ __volatile__("mov %%cr3, %0;":"=r"(cr3));
	printk("[debug]: cr3:\t0x%lx\n", cr3);
	unsigned long paddr;
	struct vtp_t vtp_s={0};
	if(vtp((unsigned long)&counter, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] &counter: 0x%px\n", &counter);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); 
		/*if(vtp_s.pte_p!=NULL) {
			__asm__ __volatile__("invlpg (%0)"::"r"(&counter):"memory");
			DISABLE_RW_PROTECTION
			vtp_s.pte_p->global=1;
			ENABLE_RW_PROTECTION
			__asm__ __volatile__("invlpg (%0)"::"r"(&counter):"memory"); }*/}

	memcpy(orig_bytes, (void *)zd_handler, STUB_SIZE+8);
	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	//zd_idte->ist=(zd_idte+1)->ist;
	memcpy((void *)zd_handler, &stub, STUB_SIZE);
	memcpy((void *)(zd_handler+2), &(vtp_s.pte_p), 8);
	//memcpy((void *)(zd_handler+17), &(vtp_s.pte_p), 8);
	*(void **)(zd_handler+STUB_SIZE)=&(get_tss()->rsp1);
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION
		/*DISABLE_RW_PROTECTION
		__asm__ __volatile__("cli":::"memory");
		zd_idte->offset_0_15=((unsigned long)(&asm_hook))&0xffff;
		zd_idte->offset_16_31=((unsigned long)(&asm_hook)>>16)&0xffff;
		zd_idte->offset_32_63=((unsigned long)(&asm_hook)>>32)&0xffffffff;
		__asm__ __volatile__("sti":::"memory");
		ENABLE_RW_PROTECTION*/
		/*idte_offset=0
			| ((long)(zd_idte->offset_0_15))
			| ((long)(zd_idte->offset_16_31)<<16)
			| ((long)(zd_idte->offset_32_63)<<32);*/
	/*printk("[*]  new idt entry %d:\n"
	       "[**] addr:\t0x%px\n"
	       "[**] segment:\t0x%x\n"
	       "[**] ist:\t%d\n"
	       "[**] type:\t%d\n"
	       "[**] dpl:\t%d\n"
	       "[**] p:\t\t%d\n"
	       "[*]  end dump\n\n",
	       ZD_INT, (void *)(\
	       (long)zd_idte->offset_0_15|((long)zd_idte->offset_16_31<<16)|((long)zd_idte->offset_32_63<<32)),
	       zd_idte->segment_selector, zd_idte->ist, zd_idte->type, zd_idte->dpl, zd_idte->p);*/
	       
	//unsigned long paddr;
	//struct vtp_t vtp_s={0};
	
	/*vtp_s=(struct vtp_t){0};
	if(vtp((unsigned long)&asm_hook, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] asm_hook: 0x%px\n", &asm_hook);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr);
		if(vtp_s.pte_p!=NULL) {
			__asm__ __volatile__("invlpg (%0)"::"r"(&asm_hook):"memory");
			DISABLE_RW_PROTECTION
			vtp_s.pte_p->global=1;
			ENABLE_RW_PROTECTION
			__asm__ __volatile__("invlpg (%0)"::"r"(&asm_hook):"memory"); }}*/
		/*mem_map = (struct page *) kallsyms_lookup_name("mem_map");
		if(mem_map==NULL) {
			return -1; }
		printk("[*] sizeof(struct page): %ld\n", sizeof(struct page));
		printk("[*] mem_map @ 0x%px\n\n", mem_map);
		unsigned long addr;
		addr=(unsigned long)&idt_init;
		printk("[*] idt_init @ 0x%lx\n\n", addr);
		//printk("[*] idt_init @ 0x%px\n\n", &idt_init);*/
	/*vtp_s=(struct vtp_t){0};
	if(vtp((unsigned long)&asm_hook, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] asm_hook: 0x%px\n", &asm_hook);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); }*/
		
		/*vtp_s=(struct vtp_t){0};
		if(vtp((unsigned long)&idte_offset, &paddr, &vtp_s)) {
			printk("[*] error\n\n"); }
		else {
			printk("[*] &idte_offset: 0x%px\n", &idte_offset);
			print_vtp_s(&vtp_s);
			printk("[*] paddr: 0x%lx\n\n", paddr);
			if(vtp_s.pte_p!=NULL) {
				__asm__ __volatile__("invlpg (%0)"::"r"(&idte_offset):"memory");
				DISABLE_RW_PROTECTION
				vtp_s.pte_p->global=1;
				ENABLE_RW_PROTECTION
				__asm__ __volatile__("invlpg (%0)"::"r"(&idte_offset):"memory"); }}*/
					/*unsigned long paddr;
					struct vtp_t vtp_s={0};
					if(vtp((unsigned long)&counter, &paddr, &vtp_s)) {
						printk("[*] error\n\n"); }
					else {
						printk("[*] &counter: 0x%px\n", &counter);
						print_vtp_s(&vtp_s);
						printk("[*] paddr: 0x%lx\n\n", paddr); 
						if(vtp_s.pte_p!=NULL) {
							__asm__ __volatile__("invlpg (%0)"::"r"(&counter):"memory");
							DISABLE_RW_PROTECTION
							vtp_s.pte_p->global=1;
							ENABLE_RW_PROTECTION
							__asm__ __volatile__("invlpg (%0)"::"r"(&counter):"memory"); }}*/
	
	/*unsigned long pte_p=(unsigned long)(vtp_s.pte_p);
	vtp_s=(struct vtp_t){0};
	if(vtp(pte_p, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] &pte: 0x%lx\n", pte_p);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); }*/

		/*vtp_s=(struct vtp_t){0};
		if(vtp(idte_offset, &paddr, &vtp_s)) {
			printk("[*] error\n\n"); }
		else {
			printk("[*] offset: 0x%lx\n", idte_offset);
			print_vtp_s(&vtp_s);
			printk("[*] paddr: 0x%lx\n\n", paddr); }*/

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
	/*unsigned long paddr;
	struct vtp_t vtp_s={0};
	
	vtp_s=(struct vtp_t){0};
	if(vtp((unsigned long)&asm_hook, &paddr, &vtp_s)) {
		printk("[*] error\n\n"); }
	else {
		printk("[*] asm_hook: 0x%px\n", &asm_hook);
		print_vtp_s(&vtp_s);
		printk("[*] paddr: 0x%lx\n\n", paddr); }*/
		/*DISABLE_RW_PROTECTION
		__asm__ __volatile__("cli":::"memory");
		zd_idte->offset_0_15=zd_handler&0xffff;
		zd_idte->offset_16_31=(zd_handler>>16)&0xffff;
		zd_idte->offset_32_63=(zd_handler>>32)&0xffffffff;
		__asm__ __volatile__("sti":::"memory");
		ENABLE_RW_PROTECTION*/
	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	zd_idte->ist=0;
	memcpy((void *)zd_handler, orig_bytes, STUB_SIZE+8);
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION }

module_init(idt_init);
module_exit(idt_fini);
