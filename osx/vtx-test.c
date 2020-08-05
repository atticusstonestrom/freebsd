//
//  vtx_test.c
//  vtx-test
//
//  Created by Atticus Stonestrom on 8/4/20.
//  Copyright © 2020 Atticus Stonestrom. All rights reserved.
//

#include <sys/systm.h>
#include <mach/mach_types.h>

kern_return_t vtx_test_start(kmod_info_t * ki, void *d);
kern_return_t vtx_test_stop(kmod_info_t *ki, void *d);

#define IA32_VMX_BASIC 0x480
#define IA32_VMX_PROCBASED_CTLS2 0x48b


#define READ_MSR(dst, id)  __asm__ __volatile__("rdmsr":"=a"((dst).eax), "=d"((dst).edx):"c"(id):"memory")

typedef union __attribute__((packed)) {
    struct __attribute__((packed)) {
        unsigned int eax;
        unsigned int edx; };
    unsigned long val;
} msr_t;

kern_return_t vtx_test_start(kmod_info_t * ki, void *d) {
    msr_t msr;
    READ_MSR(msr, IA32_VMX_BASIC);
    printf("ia32_vmx_basic:\t0x%lx\n", msr.val);
    READ_MSR(msr, IA32_VMX_PROCBASED_CTLS2);
    printf("cpu_based_ctls_2:\t0x%08x 0x%08x\n", msr.eax, msr.edx);
    return KERN_SUCCESS; }

kern_return_t vtx_test_stop(kmod_info_t *ki, void *d) {
    printf("unloading\n");
    return KERN_SUCCESS; }
