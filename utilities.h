#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>

/////////////////////////////////////////////////////
// to-do: paging entry structure 
// push %fs? push %gs?
// struct gdte_t?
// syscall prologue
// irq stack
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
#define DISABLE_RW_PROTECTION			\
__asm__ __volatile__(				\
	"mov %%cr0, %%rax;"			\
	"and $0xfffffffffffeffff, %%rax;"	\
	"mov %%rax, %%cr0;"			\
	:::"rax");				

#define ENABLE_RW_PROTECTION \
__asm__ __volatile__(				\
	"mov %%cr0, %%rax;"			\
	"or $0x10000, %%rax;"			\
	"mov %%rax, %%cr0;"			\
	:::"rax");
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
#define SC_NUM "335"
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
union msr_t {
	struct __attribute__((packed)) {
		unsigned int eax;
		unsigned int edx; };
	unsigned long val; }
	__attribute__((packed));

#define READ_MSR(dst, id)  __asm__ __volatile__("rdmsr":"=a"((dst).eax), "=d"((dst).edx):"c"(id)); 
#define WRITE_MSR(src, id) __asm__ __volatile__("wrmsr"::"a"((src).eax), "d"((src).edx), "c"(id)); 
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
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
	__attribute__((packed));

struct idtr_t {
	unsigned short lim_val;
	struct idte_t *addr; }
	__attribute__((packed));

#define READ_IDT(dst)	\
__asm__ __volatile__(	\
	"cli;"		\
	"sidt %0;"	\
	"sti;"		\
	:: "m"(dst)	\
	: "memory");

#define WRITE_IDT(src)	\
__asm__ __volatile__(	\
	"cli;"		\
	"lidt %0;"	\
	"sti;"		\
	:: "m"(src)	\
	: "memory");
/////////////////////////////////////////////////////


/////////////////////////////////////////////////////
#define PUSHA \
	"push %rax;""push %rbx;""push %rcx;""push %rdx;"\
	"push %rbp;""push %rdi;""push %rsi;""push %r8;"\
	"push %r9;""push %r10;""push %r11;""push %r12;"\
	"push %r13;""push %r14;""push %r15;""pushf;"

#define POPA \
	"popf;""pop %r15;""pop %r14;""pop %r13;"\
	"pop %r12;""pop %r11;""pop %r10;""pop %r9;"\
	"pop %r8;""pop %rsi;""pop %rdi;""pop %rbp;"\
	"pop %rdx;""pop %rcx;""pop %rbx;""pop %rax;"
/////////////////////////////////////////////////////
		

/////////////////////////////////////////////////////
struct gdtr_t {
	unsigned short lim_val;
	void *addr; }
	__attribute__((packed));

struct tssd_t {
	unsigned short seg_lim_0_15;
	unsigned short base_addr_0_15;
	unsigned char base_addr_16_23;		//interrupt stack table
	unsigned char type:4;
	unsigned char zero_12:1;
	unsigned char dpl:2;			//descriptor privilege level
	unsigned char p:1;			//present flag
	unsigned char seg_lim_16_19:4;
	unsigned char avl:1;			//available for use
	unsigned char zero_20_21:2;
	unsigned char granularity:1;
	unsigned char base_addr_24_31;
	unsigned int base_addr_32_63;
	unsigned int rsv; }
	__attribute__((packed));

struct tss_t {
	unsigned int rsv_0_3;
	unsigned long rsp0;
	unsigned long rsp1;
	unsigned long rsp2;
	unsigned long rsv_28_35;
	unsigned long ist1;
	unsigned long ist2;
	unsigned long ist3;
	unsigned long ist4;
	unsigned long ist5;
	unsigned long ist6;
	unsigned long ist7;
	unsigned long rsv_92_99;
	unsigned short rsv_100_101;
	unsigned short io_map_base_addr; }
	__attribute__((packed));

__attribute__((__always_inline__))
struct tss_t *get_tss(void) {
	struct gdtr_t gdtr={0};
	unsigned short tr=0;
	__asm__ __volatile__("sgdt %0"::"m"(gdtr):"memory");
	__asm__ __volatile__("str %0"::"m"(tr):"memory");
	struct tssd_t *tssd=(void *)((unsigned long)gdtr.addr+tr);
	return (struct tss_t *)(0
		| ((long)(tssd->base_addr_0_15))
		| ((long)(tssd->base_addr_16_23)<<16)
		| ((long)(tssd->base_addr_24_31)<<24)
		| ((long)(tssd->base_addr_32_63)<<32)); }
/////////////////////////////////////////////////////


//pg 2910
union pse_t {
	struct __attribute__((packed)) {
		unsigned char p:1;
		unsigned char rw:1;
		unsigned char us:1;
		unsigned char pwt:1;
		unsigned char pcd:1;
		unsigned char accessed:1;
		union __attribute__((packed)) {
			struct __attribute__((packed)) {
				unsigned char dirty:1;
				unsigned char page_size:1;
				unsigned char global:1; };
			unsigned char rsv_6_8:3; };
		unsigned char rsv_9_11:3;
		union __attribute__((packed)) {
			unsigned long addr_pse:40;		//bits 12 to 51
			struct __attribute__((packed)) {
				unsigned char pat:1;
				unsigned int rsv_13_29:17;
				unsigned int addr_1gb:22; };	//bits 30 to 51
			struct __attribute__((packed)) {
				unsigned char pat:1;
				unsigned char rsv_13_20:8;
				unsigned int addr_2mb:31; };	//bits 21 to 51
			unsigned long addr_4kb:40; };		//bits 12 to 51
		union __attribute__((packed)) {
			struct __attribute__((packed)) {
				unsigned char rsv_52_58:7;
				unsigned char prot_key:4; };
			unsigned short rsv_52_62:11; };
		unsigned char nx:1; };
	unsigned long val; }
	__attribute__((packed));

union vaddr_t {
	struct __attribute__((packed)) {
		unsigned short offset_pse:12;
		unsigned short pt_bits:9;	//bits 12 to 20
		unsigned short pd_bits:9;	//bits 21 to 29
		unsigned short pdpt_bits:9;	//bits 30 to 38
		unsigned short pml4_bits:9;	//bits 39 to 47
		unsigned short pml5_bits:9;	//bits 48 to 56
		unsigned char rsv_57_63:7; };
	struct __attribute__((packed)) {
		unsigned int offset_1gb:30;
		unsigned long rsv_30_63:34; };
	struct __attribute__((packed)) {
		unsigned int offset_2mb:21;
		unsigned long rsv_21_63:43; };
	struct __attribute__((packed)) {
		unsigned short offset_4kb:12;
		unsigned long rsv_12_63:52; };
	unsigned long val; }
	__attribute__((packed));

struct vtp_t {
	union pse_t *pml5e_p;
	union pse_t *pml4e_p;
	union pse_t *pdpte_p;
	union pse_t *pde_p;
	union pse_t *pte_p; };

__attribute__((__always_inline__)) unsigned int
vtp(unsigned long addr, unsigned long *to_fill) {
	//asm block checks to see if 4 or 5-level paging is enabled
	//if so, moves the cr3 register into the cr3 variable
	//and sets la57_flag to assert whether 4-level or 5-level
	int la57_flag=0;
	union pse_t cr3={0};
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
	
		: "=r"(cr3.val), "=r"(la57_flag)
		::"rax", "ecx", "memory");
	if(!cr3) {
		return -EOPNOTSUPP; }

	unsigned long paddr=0;
	union pse_t psentry={0};
	union vaddr_t vaddr=(union vaddr_t)addr;

	//pml5e (if applicable)
	if(la57_flag) {			//5-level paging
		//printk("[debug]: &pml5e:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(cr3)|(PML5_MASK(vaddr)>>45) ));
		psentry=*(unsigned long *)\
			phys_to_virt(cr3.addr_pse|(vaddr.pml5_bits>>45));
		//printk("[debug]: pml5e:\t0x%lx\n", psentry);
		if(!psentry.p) {
			return -EFAULT; }}
	else {
		psentry=cr3; }

	//pml4e
	//printk("[debug]: &pml4e:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PML4_MASK(vaddr)>>36) ));
	psentry=*(unsigned long *)\
		phys_to_virt(psentry.addr_pse|(vaddr.pml4_bits>>36));
	//printk("[debug]: pml4e:\t0x%lx\n", psentry);
	if(!psentry.p) {
		return -EFAULT; }

	//pdpte
	//printk("[debug]: &pdpte:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PDPT_MASK(vaddr)>>27) ));
	psentry=*(unsigned long *)\
		phys_to_virt(psentry.addr_pse|(vaddr.pdpt_bits>>27));
	//printk("[debug]: pdpte:\t0x%lx\n", psentry);
	if(psentry.page_size) {	//1GB page
		//bits (51 to 30) | bits (29 to 0)
		paddr=psentry.addr_1gb|vaddr.offset_1gb;
		*to_fill=paddr;
		return 0; }
	if(!psentry.p) {
		return -EFAULT; }

	//pde
	//printk("[debug]: &pde:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PD_MASK(vaddr)>>18) ));
	psentry=*(unsigned long *)\
		phys_to_virt(psentry.addr_pse|(vaddr.pd_bits>>18));
	//printk("[debug]: pde:\t0x%lx\n", psentry);
	if(psentry.page_size) {	//2MB page
		//bits (51 to 21) | bits (20 to 0)
		paddr=psentry.addr_2mb|vaddr.offset_2mb;
		*to_fill=paddr;
		return 0; }
	if(!psentry.p) {
		return -EFAULT; }

	//pte
	//printk("[debug]: &pte:\t0x%px\n", phys_to_virt( PE_ADDR_MASK(psentry)|(PT_MASK(vaddr)>>9) ));
	psentry=*(unsigned long *)\
		phys_to_virt(psentry.addr_pse|(vaddr.pt_bits>>9));
	//printk("[debug]: pte:\t0x%lx\n", psentry);
	paddr=psentry.addr_4kb|vaddr.offset_4kb;
	*to_fill=paddr;
	return 0; }
/////////////////////////////////////////////////////

