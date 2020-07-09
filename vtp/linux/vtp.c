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
#include "utilities.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("hooks mkdirat to give virtual to physical addressing");
MODULE_VERSION("0.01");

static void print_vtp(struct vtp_t *vtp_p) {
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

unsigned int hook(unsigned long vaddr, unsigned long *to_fill, struct vtp_t *vtp_p) {
	int ret=0;
	struct vtp_t kernel_vtp;
	if(vtp_p==NULL) {
		vtp_p=&kernel_vtp; }
	*vtp_p=(struct vtp_t){0};
	if(ret=vtp(vaddr, to_fill, vtp_p)) {
		return ret; }
	print_vtp(vtp_p);
	return 0; }

__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"pushf;"
	"cmp $"SC_NUM", %rax;"
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

union msr_t old_lstar, new_lstar;

static int __init
vtp_init(void) {
	old_lstar.val=0;
	READ_MSR(old_lstar, 0xc0000082)
	printk("[*] ia32_lstar:\t0x%lx", old_lstar.val);
	printk("[*] asm_hook:\t0x%lx", (unsigned long)&asm_hook);
	printk("\n");
	new_lstar.val=(unsigned long)&asm_hook;
	WRITE_MSR(new_lstar, 0xc0000082)
	return 0; }


static void __exit
vtp_fini(void) {
	WRITE_MSR(old_lstar, 0xc0000082)
	return; }

module_init(vtp_init);
module_exit(vtp_fini);
/////////////////////////////////////////////////////
