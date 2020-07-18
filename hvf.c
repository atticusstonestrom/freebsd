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
#define STUB_SIZE 0x2b
unsigned char orig_bytes[STUB_SIZE];
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
	/*"push %rdx;"	//bp_handler
	"push %rbx;"	//new cr3
	"push %rax;"	//old cr3
	"mov %cr3, %rax;"
	"mov .CR3(%rip), %rbx;"
	"mov %rbx, %cr3;"
		"incl counter;"				// invlpg?
		"movq (bp_handler), %rdx;"
	"mov %rax, %cr3;"
	"pop %rax;"
	"pop %rbx;"
	"xchg %rcx, (%rsp);"
	"ret;"*/
".CR3:;"
	".quad 0xdeadbeefdeadbeef;");
extern void stub(void);
__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"incl counter;"
	"movq (bp_handler), %rax;"
	"ret;");
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
	printk("[*]  breakpoint handler:\t0x%lx\n\n", bp_handler);

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

	unsigned long cr3;
	__asm__ __volatile__("mov %%cr3, %0":"=r"(cr3)::"memory");
	printk("[*] cr3:\t0x%lx\n\n", cr3);

	memcpy(orig_bytes, (void *)zd_handler, STUB_SIZE);
	DISABLE_RW_PROTECTION
	__asm__ __volatile__("cli":::"memory");
	//zd_idte->ist=(zd_idte+1)->ist;
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
	zd_idte->ist=0;
	memcpy((void *)zd_handler, orig_bytes, STUB_SIZE);
	__asm__ __volatile__("sti":::"memory");
	ENABLE_RW_PROTECTION }

module_init(idt_init);
module_exit(idt_fini);
