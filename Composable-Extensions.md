# Requirements

## cxsel (URO) CSR

* cxsel is a read-only XLEN-wide CX selector CSR
* cxsel is a WARL register that MUST be able to hold all valid selector values
* cxsel is 0 at reset


## cxsetsel (custom instruction)

* cxsetsel is a non-custom 32-bit instruction.
* cxsetsel rd,rs1 atomically swaps the value in cxsel and an integer register.
* It writes the initial value of x[rs1] to cxsel, and if rd is not x0 it writes the initial value of cxsel to x[rd].

## CX State Context Index (cxsidx) CSR

* cxsidx is a read-write XLEN-wide CX state index CSR
* cxsidx specifies the index of the word of CX state context data to access via the cxsdata CSR.
* cxsidx can represent any valid index of the state context data of the selected CX.
* cxsidx is undefined following cxsetsel
* Valid values for cxsidx are 0 through 'size-1', where 'size' is the size in words

## CX State Context Data (cxsdata) CSR

* cxsdata is a read-write XLEN-wide CX state context data CSR
* cxsdata is used to read and/or write one word of the selected CX’s state context data.
* When cxsdata is read (e.g., csrrs rd,cxsdata,x0), one word of the selected CX state context data, at valid index cxsidx is written to the destination register.
* When cxsdata is written (e.g., csrrw x0,cxsdata,rs1), the source value is written to one word of the selected CX state context data, at valid index cxsidx.
* When cxsdata is read and written (e.g., csrrw rd,cxsdata,rs1), both effects occur.
* Following each cxsdata access, the value cxsidx + 1 is written to cxsidx.
* Following read/write of the last word of the selected CX’s state context data, the value of cxsidx is undefined.
* When cxsel is 0 or invalid, or when cxsidx is not a valid index of the selected CX’s state context data, read or write of cxsdata is undefined.

## Platform specific discovery mechanism

* CX UUID to cx_sel_t mapping
* size of the CX state 'context data' of the selected custom extension in n words

## Tests

### cxsel CSR / cxsetsel instr

* Read from cxset at reset MUST be the value 0 
* Write to the cxsel register MUST NOT cause an exception (since it is a WARL register)


## Open Questions

* Chapter 3.2 - cxsel is specified as (read-only URO) but also as WARL (-> different write behavior!)
* Chapter 3.2 - cxsel wirth a value of 0 defines a built-in custom extension?
     The document should define what a built-in custom extension is before using it.

* Chapter 3.4 - When cxsel is invalid, CX instructions are treated as illegal instructions...
     Maybe we should make clear that this does mean cxsetsel (Differentiate between cxsetsel and the rest)

* Chapter 4.3 Typo : The read-write WARL XLEN-wide CX >>>>state index>>>>> CSR,

* CX CSR's! Can they be interrupted mid-execution with visible progress state (ISA-defined)??? Do we have restart semantics?








* HARTs may have heterogeneous cores (w/o CX). How does a cx-open() will operate on a HART with and without CX?
    * CPU Affinity before cx_open? (sched_setaffinity()?) 
    * Is this operating system specific?
    * #include <sys/prctl.h>   ... nt vl = prctl(PR_SVE_SET_VL, 256);
    * RISC-V has the riscv_hwprobe() system call l (added in Linux 6.4) ... RISCV_HWPROBE_EXT_ZVE32X


Sample

``` source
#include <sys/syscall.h>
#include <sched.h>

// 1. Detect available extensions via hwprobe
// 2. Find which cores have those extensions (parse /sys or device tree)
// 3. Use sched_setaffinity() to pin to compatible cores
// 4. Optionally install signal handler for SIGILL to catch illegal instructions

void setup_for_vector_code() {
    // Step 1: Check if ANY core has vector
    struct riscv_hwprobe probe = {
        .key = RISCV_HWPROBE_KEY_IMA_EXT_0
    };
    syscall(__NR_riscv_hwprobe, &probe, 1, 0, NULL, 0);
    
    if (!(probe.value & RISCV_HWPROBE_EXT_ZVE64D)) {
        fprintf(stderr, "No vector extension available\n");
        exit(1);
    }
    
    // Step 2: Pin to cores with vector (hardcoded for now)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // Assuming CPU 0 has vector
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    
    // Step 3: Now safe to use vector instructions
}
```