//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/mutex.h>


#define BP_INT 0x03
#define IDT_ENTRY_SIZE 8
struct idte_t {
	unsigned short offset_0_15;
	unsigned short segment_selector;
	unsigned char ist;	//look up interrupt stack table; if 0, field is unused
	unsigned char type:4;
	unsigned char zero_12:1;
	unsigned char dpl:2;
	unsigned char p:1;
	unsigned short offset_16_31;
	unsigned int offset_32_63;
	unsigned int rsv; }
	__attribute__((packed))
	old_idte;
//void (*idte_offset)(void);
unsigned long idte_offset;
struct idtr_t {
	unsigned short lim_val;
	unsigned long addr; }
	__attribute__((packed))
	idtr;

static void
idt_hook() {
	/*uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");
	uprintf("in hook\n");*/
	__asm__ __volatile__(
		"pop %rbp;"
		//"jmp *idte_offset;"
		"mov $0xffffffff81080d70, %rax;"
		"jmp *%rax;"
		"push %rbp;"); }
	//(*(void (*)(void))idte_offset)();
	/*__asm__ __volatile__(
		"mov $0xffffffff81080d70, %%rax;"
		"jmp *%%rax;"
		:::"rax"); }*/
	//__asm__ __volatile__("jmp *idte_offset"); }
	//__asm__ __volatile__("jmp 0xffffffff81080d70"); }

static int
load(struct module *module, int cmd, void *arg) {
	int error=0;
	switch(cmd) {
		case MOD_LOAD:
			__asm__ __volatile__ (
				"sidt idtr;"
				: //"=r"(idtr)
				:: "memory");
			uprintf("idtr: addr: %p, lim_val: 0x%x \n", (void *)idtr.addr, idtr.lim_val);

			memcpy(&old_idte, (void *)(idtr.addr+BP_INT*sizeof(struct idte_t)), sizeof(struct idte_t));
			idte_offset=(long)old_idte.offset_0_15|((long)old_idte.offset_16_31<<16)|((long)old_idte.offset_32_63<<32);
			uprintf("old idt entry %d:\n"
				"\taddr:\t%p\n"
				"\tist:\t%d\n"
				"\ttype:\t%d\n"
				"\tdpl:\t%d\n"
				"\tp:\t%d\n",
				BP_INT, (void *)idte_offset, old_idte.ist, old_idte.type, old_idte.dpl, old_idte.p);
			struct idte_t new_idte;
			memcpy(&new_idte, &old_idte, sizeof(struct idte_t));
			new_idte.offset_0_15=((unsigned long)(&idt_hook))&0xffff;
			new_idte.offset_16_31=((unsigned long)(&idt_hook)>>16)&0xffff;
			new_idte.offset_32_63=((unsigned long)(&idt_hook)>>32)&0xffffffff;
			uprintf("new idt entry %d:\n"
				"\taddr:\t%p\n"
				"\tist:\t%d\n"
				"\ttype:\t%d\n"
				"\tdpl:\t%d\n"
				"\tp:\t%d\n",
				BP_INT, &idt_hook, new_idte.ist, new_idte.type, new_idte.dpl, new_idte.p);
			//lock should go here?
			memcpy((void *)(idtr.addr+BP_INT*sizeof(struct idte_t)), &new_idte, sizeof(struct idte_t));
			break;
		case MOD_UNLOAD:
			//lock here?
			memcpy((void *)(idtr.addr+BP_INT*sizeof(struct idte_t)), &old_idte, sizeof(struct idte_t));
			break;
		default:
			error=EOPNOTSUPP;
			break; }
	return error; }

static moduledata_t idt_hook_mod = {
	"idt_hook",
	load,
	NULL };

DECLARE_MODULE(idt_hook, idt_hook_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
