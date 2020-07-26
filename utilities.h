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
// constrain eax and edx in READ_MSR?
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
		unsigned long p:1;
		unsigned long rw:1;
		unsigned long us:1;
		unsigned long pwt:1;
		unsigned long pcd:1;
		unsigned long accessed:1;
		unsigned long dirty:1;
		unsigned long page_size:1;	//can be PAT here for 4kb page :/
		unsigned long global:1;
		unsigned long rsv_9_11:3;
		unsigned long addr:40;		//bits 12 to 51
		unsigned long rsv_52_58:7;
		unsigned long prot_key:4;
		unsigned long nx:1; };
	struct __attribute__((packed)) {
		unsigned long rsv_1gb_0_11:12;
		unsigned long pat_1gb:1;
		unsigned long rsv_1gb_13_29:17;
		unsigned long addr_1gb:22;	//bits 30 to 51
		unsigned long rsv_1gb_52_63:12; };
	struct __attribute__((packed)) {
		unsigned long rsv_2mb_0_11:12;
		unsigned long pat_2mb:1;
		unsigned long rsv_2mb_13_20:8;
		unsigned long addr_2mb:31;	//bits 21 to 51
		unsigned long rsv_2mb_52_63:12; };
	unsigned long val; }
	__attribute__((packed));

union vaddr_t {
	struct __attribute__((packed)) {
		unsigned long offset_pse:12;
		unsigned long pt_bits:9;	//bits 12 to 20
		unsigned long pd_bits:9;	//bits 21 to 29
		unsigned long pdpt_bits:9;	//bits 30 to 38
		unsigned long pml4_bits:9;	//bits 39 to 47
		unsigned long pml5_bits:9;	//bits 48 to 56
		unsigned long rsv_57_63:7; };
	struct __attribute__((packed)) {
		unsigned long offset_1gb:30;
		unsigned long rsv_30_63:34; };
	struct __attribute__((packed)) {
		unsigned long offset_2mb:21;
		unsigned long rsv_21_63:43; };
	struct __attribute__((packed)) {
		unsigned long offset_4kb:12;
		unsigned long rsv_12_63:52; };
	unsigned long val; }
	__attribute__((packed));

struct vtp_t {
	union pse_t *pml5e_p;
	union pse_t *pml4e_p;
	union pse_t *pdpte_p;
	union pse_t *pde_p;
	union pse_t *pte_p; };

unsigned int
vtp(unsigned long addr, unsigned long *paddr_p, struct vtp_t *vtp_p) {
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
		"shr $12, %%rax;"
		"and $1, %%rax;"
		"mov %%eax, %1;"
		"jmp break;"
	"fail:\n"
		"mov $0, %0;"
	"break:\n"
	
		: "=r"(cr3.val), "=r"(la57_flag)
		::"rax", "ecx", "edx", "memory");
	if(!cr3.val) {
		return -EOPNOTSUPP; }

	union pse_t psentry={0};
	union vaddr_t vaddr=(union vaddr_t)addr;
	if(vtp_p!=NULL) {
		*vtp_p=(struct vtp_t){ .pml5e_p=NULL, .pml4e_p=NULL, .pdpte_p=NULL, .pde_p=NULL, .pte_p=NULL }; }

	//pml5e (if applicable)
	if(la57_flag) {			//5-level paging
		if(vtp_p!=NULL) {
			vtp_p->pml5e_p=(void *)phys_to_virt(((unsigned long)cr3.addr<<12)|((unsigned long)vaddr.pml5_bits<<3)); }
		psentry.val=*(unsigned long *)\
			phys_to_virt(((unsigned long)cr3.addr<<12)|((unsigned long)vaddr.pml5_bits<<3));
		if(!psentry.p) {
			return -EFAULT; }}
	else {
		psentry.val=cr3.val; }

	//pml4e
	if(vtp_p!=NULL) {
		vtp_p->pml4e_p=(void *)phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pml4_bits<<3)); }
	psentry.val=*(unsigned long *)\
		phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pml4_bits<<3));
	if(!psentry.p) {
		return -EFAULT; }

	//pdpte
	if(vtp_p!=NULL) {
		vtp_p->pdpte_p=(void *)phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pdpt_bits<<3)); }
	psentry.val=*(unsigned long *)\
		phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pdpt_bits<<3));
	if(psentry.page_size) {	//1GB page
		//bits (51 to 30) | bits (29 to 0)
		*paddr_p=((unsigned long)psentry.addr_1gb<<30)|vaddr.offset_1gb;
		return 0; }
	if(!psentry.p) {
		return -EFAULT; }

	//pde
	if(vtp_p!=NULL) {
		vtp_p->pde_p=(void *)phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pd_bits<<3)); }
	psentry.val=*(unsigned long *)\
		phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pd_bits<<3));
	if(psentry.page_size) {	//2MB page
		//bits (51 to 21) | bits (20 to 0)
		*paddr_p=((unsigned long)psentry.addr_2mb<<21)|vaddr.offset_2mb;
		return 0; }
	if(!psentry.p) {
		return -EFAULT; }

	//pte
	if(vtp_p!=NULL) {
		vtp_p->pte_p=(void *)phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pt_bits<<3)); }
	psentry.val=*(unsigned long *)\
		phys_to_virt(((unsigned long)psentry.addr<<12)|((unsigned long)vaddr.pt_bits<<3));
	*paddr_p=((unsigned long)psentry.addr<<12)|vaddr.offset_4kb;
	return 0; }
/////////////////////////////////////////////////////
