//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

static int param_cpu_id;
module_param(param_cpu_id, int, (S_IRUSR|S_IRGRP|S_IROTH));
MODULE_PARM_DESC(param_cpu_id, "cpu id that operations run on");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atticus Stonestrom");
MODULE_DESCRIPTION("...");
MODULE_VERSION("0.01");

/////////////////////////////////////////


//CALL_FUNCTION_VECTOR, pg 170
//will cli disable preemption?
//	https://stackoverflow.com/questions/53919482/whats-the-process-of-disabling-interrupt-in-multi-processor-system
//msr 0x830
//https://stackoverflow.com/questions/62068750/kinds-of-ipi-for-x86-architecture-in-linux
//https://stackoverflow.com/questions/22310028/is-there-an-x86-instruction-to-tell-which-core-the-instruction-is-being-run-on
//	https://stackoverflow.com/questions/6146059/how-can-i-detect-the-number-of-cores-in-x86-assembly
//	https://wiki.osdev.org/Detecting_CPU_Topology_(80x86)
static void per_cpu_print(void *info) {
	cpuid_t cpuid;
	unsigned int ia32_tsc_aux;
	__asm__ __volatile__("rdtscp":"=c"(ia32_tsc_aux));
	CPUID(cpuid, 0x0b);
	printk("linux id: %d\t\tapic id: %d\t\ttsc_aux: %d\n", smp_processor_id(), cpuid.edx, ia32_tsc_aux); }

static int __init hvc_init(void) {
	printk("num_online_cpus: %d\n", num_online_cpus());
	on_each_cpu(per_cpu_print, NULL, 1);
	int cpu_id;
	if(param_cpu_id!=0) {
		printk("[*] unable to load module without cpu parameter 0\n");
		return -EINVAL; }
	cpu_id=get_cpu();
	printk("[*]  called on cpu: %d\n\n", cpu_id);
	put_cpu();
	return 0; }

static void __exit hvc_exit(void) {
	printk("[*] unloading\n"); }

module_init(hvc_init);
module_exit(hvc_exit);
