# RISC-V Zcx Extension Implementation Specification

## Overview

This document specifies the implementation of the **Zcx (Composable Extensions)** extension for QEMU's RISC-V target. The Zcx extension provides a standardized interface for multiplexing custom extensions through Control and Status Registers (CSRs), enabling dynamic selection and configuration of composable custom extensions.

**Project:** QEMU RISC-V Emulator  
**Branch:** cxtg  
**Authors:** Artur Lojewski (lojewski@gmail.com), Christopher Dunn (christopher.m.dunn@gmail.com)  
**Status:** In Development  

## Extension Purpose

The Zcx extension addresses the challenge of managing multiple custom RISC-V extensions by providing:

1. **Extension Selection** - Runtime selection of active custom extension via `cxsel` CSR
2. **Extension Configuration** - Indexed access to extension parameters via `cxsidx` and `cxsdata` CSRs
3. **Built-in Support** - A default built-in custom extension (selector value 0)
4. **Multiplexing** - Routing of CX instructions to appropriate extension handlers

## Architecture

### CSR Definitions

CSR addresses are placeholders pending spec finalisation (~permanent TODO).
See `todo.md` for tracking. Do not treat as stable.

| CSR Address | Name      | Access | Description              |
|-------------|-----------|--------|--------------------------|
| 0xCA0       | `cxsel`   | URO    | CX Selector              |
| 0x018       | `cxsidx`  | URW    | CX State Index           |
| 0x019       | `cxsdata` | URW    | CX State Data            |

**Note:** CSR addresses are defined in `target/riscv/cpu_bits.h`

### cxsel Field Layout (Direct Mode)

```
[XLEN-1:16] reserved | [15:8] SID | [7:0] CXID
```

**Naming departure from spec:** The spec (Figure 3) calls the [15:8] field **IDX**.
This implementation uses **SID** (State ID) for clarity. TODO: align with TG/spec
terminology once finalised.

### cxsetsel WARL Clamping (implementation-defined)

Two clamping rules applied by the `cxsetsel` translate handler before writing `cxsel`:

1. **CXID=0 (legacy): SID clamped to 0.** When CXID=0, SID is reserved and has no
   meaning. Any non-zero SID is masked to 0.

2. **Negative values clamped to CX_SEL_INVALID (~0UL).** If the value is negative
   (sign bit set), it is stored as ~0UL. This subsumes ~0UL itself.

```c
if (CXID(val) == 0) {
    val &= ~CXSEL_SID_MASK;
}
if ((intptr_t)val < 0) {
    val = ~0UL;
}
env->cxsel = val;
```

### CSR Behavior

#### CXSEL - Custom Extension Selector (0xCA0)

**Purpose:** Selects which custom extension handles CX instructions

**Properties:**
- **Access:** Read-Only (WARL - Write-Any-Read-Legal)
- **Reset Value:** 0 (built-in custom extension)
- **Valid Values:** 0, and implementation-specific extension IDs
- **Invalid Value:** 0xFFFFFFFF (all 1s)

**Semantics:**
- When `cxsel = 0`: CX instructions execute on the built-in custom extension
- When `cxsel` is a valid extension ID: CX instructions route to that extension
- When `cxsel` is invalid: CX instructions raise illegal instruction exception
- Write attempts return `RISCV_EXCP_ILLEGAL_INST` (enforcing read-only behavior)

**Implementation:** `target/riscv/csr.c` — `read_cxsel`, `write_cxsel`

#### CXSIDX - Custom Extension Index (0x018)

**Purpose:** Indirect addressing index for extension configuration

**Properties:**
- **Access:** Read-Write
- **Reset Value:** 0
- **Width:** XLEN bits (32 or 64)

**Usage:**
- Provides an index into extension-specific configuration space
- Combined with `cxsdata` for indirect configuration access
- Interpretation is extension-dependent

**Implementation:** `target/riscv/csr.c` — `read_cxsidx`, `write_cxsidx`

#### CXSDATA - Custom Extension Data (0x019)

**Purpose:** Data register for indexed extension configuration

**Properties:**
- **Access:** Read-Write
- **Reset Value:** 0
- **Width:** XLEN bits (32 or 64)

**Usage:**
- Read/write configuration data at offset specified by `cxsidx`
- Enables array-like access to extension parameters
- Interpretation is extension-dependent
- Accessing cxsdata auto-increments cxsidx (handled by combined `.op` handler)

**Implementation:** `target/riscv/csr.c` — `op_cxsdata` (combined read/write)

## Implementation Details

### File Structure

```
target/riscv/
├── cpu.h                    # CPURISCVState structure, ext_zcx field
├── cpu.c                    # Extension initialization, reset handler
├── cpu_bits.h               # CSR address definitions (CSR_CXSEL, etc.)
├── cpu_cfg_fields.h.inc     # Configuration field macros
├── csr.c                    # CSR operations and predicates
├── cx.c                     # CX CSR implementation logic
├── cx.h                     # CX interface declarations
├── trace-events             # Tracing definitions for CX CSRs
└── kvm/
    └── kvm-cpu.c            # KVM extension mapping
```

### CPU State Extension

The `CPURISCVState` structure is extended with three fields:

```c
struct CPUArchState {
    /* ... existing fields ... */
    
    /* CX extension */
    target_ulong cxsel;   /* Extension selector (read-only) */
    target_ulong cxsidx;  /* Configuration index */
    target_ulong cxsdata; /* Configuration data */
};
```

### Configuration System

**Extension Flag:** `ext_zcx` (Boolean)

**Registration:**
- Configuration field: `BOOL_FIELD(ext_zcx)`
- Property definition: `MULTI_EXT_CFG_BOOL("zcx", ext_zcx, false)`
- ISA string entry: `ISA_EXT_DATA_ENTRY(zcx, PRIV_VERSION_1_10_0, ext_zcx)`
- KVM mapping: `KVM_EXT_CFG("zcx", ext_zcx, KVM_RISCV_ISA_EXT_ZCX)`

**Enabling:**
```bash
qemu-system-riscv64 -cpu rv64,zcx=on ...
```

### Reset Behavior

On CPU reset:

```c
if (riscv_cpu_cfg(env)->ext_zcx) {
    env->cxsel  = 0;
    env->cxsidx = 0;
    env->cxsdata = 0;
}
```

### CSR Predicate Functions

Three predicate functions gate CSR access in `csr.c`. All three follow the same pattern:

```c
static RISCVException cxsel(CPURISCVState *env, int csrno)
{
    if (!riscv_cpu_cfg(env)->ext_zcx) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}
```

`cxsidx` and `cxsdata` predicates are identical in structure, checking `ext_zcx`.

### CSR Operation Handlers

The core CSR operations are implemented in `target/riscv/cx.c`:

**cxsel (read/write):**
```c
void cxsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong *val)
{
    *val = env->cxsel;
    trace_cxsel_csr_read(env->mhartid, reg_index, *val);
}

void cxsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxsel_csr_write(env->mhartid, reg_index, val);
    /* cxsel is read-only (in a range that QEMU hardware-enforces as read-only); write is traced but should never fire */
}
```

**cxsidx (read/write):**
```c
void cxsidx_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong *val)
{
    *val = env->cxsidx;
    trace_cxsidx_csr_read(env->mhartid, reg_index, *val);
}

void cxsidx_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    env->cxsidx = val;
    trace_cxsidx_csr_write(env->mhartid, reg_index, val);
}
```

**cxsdata (combined `.op` handler — auto-increments cxsidx):**
```c
RISCVException cxsdata_csr_op(CPURISCVState *env, int csrno,
                               target_ulong *ret_value,
                               target_ulong new_value, target_ulong write_mask)
{
    target_ulong old = env->cxsdata;
    if (ret_value) {
        *ret_value = old;
        trace_cxsdata_csr_read(env->mhartid, csrno, old);
    }
    if (write_mask) {
        env->cxsdata = (old & ~write_mask) | (new_value & write_mask);
        trace_cxsdata_csr_write(env->mhartid, csrno, env->cxsdata);
    }
    env->cxsidx++;
    return RISCV_EXCP_NONE;
}
```

### CSR Registration

CSRs are registered in the global `csr_ops[]` table in `csr.c`:

```c
[CSR_CXSEL]   = { "cxsel",   cxsel,  read_cxsel,  write_cxsel },
[CSR_CXSIDX]  = { "cxsidx",  cxsidx, read_cxsidx, write_cxsidx },
[CSR_CXSDATA] = { "cxsdata", cxsdata, .op = op_cxsdata },
```

`cxsdata` uses the combined `.op` field instead of separate `.read`/`.write` so that `cxsidx++` fires exactly once per instruction regardless of whether the access is a read, write, or swap.

### Tracing Support

Trace events are defined in `target/riscv/trace-events` for debugging:

```
cxsel_csr_read(uint64_t mhartid, uint32_t addr_index, uint64_t val) 
    "hart %" PRIu64 ": read reg %" PRIu32", val: 0x%" PRIx64

cxsel_csr_write(uint64_t mhartid, uint32_t addr_index, uint64_t val) 
    "hart %" PRIu64 ": write reg %" PRIu32", val: 0x%" PRIx64

# Similar patterns for cxsidx and cxsdata
```

**Usage:**
```bash
qemu-system-riscv64 -trace 'cxsel_*' ...
```

## Usage Examples

### Example 1: Query Extension Selector

```assembly
# Read current extension selector
csrr t0, 0xCA0          # t0 = cxsel (should be 0 after reset)
```

### Example 2: Configure Extension via Indexed Access

```assembly
# Write to extension configuration array
li   t0, 5              # Index = 5
csrw 0x018, t0          # cxsidx = 5

li   t1, 0xDEADBEEF     # Configuration value
csrw 0x019, t1          # cxsdata[5] = 0xDEADBEEF; cxsidx auto-increments to 6

# Read back (cxsidx is now 6 after the write above; reset if needed)
csrr t2, 0x019          # t2 = cxsdata[6]; cxsidx auto-increments to 7
```

### Example 3: QEMU Command Line

```bash
# Enable Zcx extension
qemu-system-riscv64 \
    -cpu rv64,zcx=on \
    -machine virt \
    -kernel my_kernel.elf

# With tracing
qemu-system-riscv64 \
    -cpu rv64,zcx=on \
    -machine virt \
    -trace 'cxsel_*' \
    -kernel my_kernel.elf
```

## Testing

### Unit Tests

1. **CSR Accessibility**
   - Verify CSRs are accessible when `ext_zcx=true`
   - Verify illegal instruction exception when `ext_zcx=false`

2. **Reset Behavior**
   - Verify `cxsel=0` after reset
   - Verify `cxsidx=0` and `cxsdata=0` after reset

3. **Read-Only Enforcement**
   - Verify writes to `cxsel` are ignored
   - Verify writes return illegal instruction exception

4. **Index/Data Mechanism**
   - Write various indices to `cxsidx`
   - Write data to `cxsdata`
   - Verify independent operation of index and data registers

### Integration Tests

1. **Extension Multiplexing**
   - Test CX instruction routing with `cxsel=0` (built-in)
   - Test illegal instruction with invalid `cxsel` values

2. **KVM Support**
   - Verify extension flag propagates to KVM
   - Test CSR access in KVM-accelerated guests

### Functional Tests

QEMU functional test framework (`tests/functional/riscv*/`):

```python
def test_zcx_csrs(self):
    """Test Zcx CSR access"""
    # Boot with Zcx enabled
    # Execute CSR read/write instructions
    # Verify expected behavior
```

## Compliance

### RISC-V Privilege Specification

- **Privilege Level:** Machine-mode (M-mode) CSRs
- **Minimum Priv Spec:** v1.10.0
- **CSR Address Space:** cxsel at 0xCA0 (user RO); cxsidx/cxsdata at 0x018/0x019 (user RW)
- **WARL Semantics:** Implemented for `cxsel`

### QEMU Coding Standards

Implementation follows QEMU RISC-V coding standards as specified in CLAUDE.md:

- ✓ 4-space indentation (no tabs)
- ✓ C-style `/* */` comments (no `//`)
- ✓ `#include "qemu/osdep.h"` first in .c files
- ✓ Variable naming: `lower_case_with_underscores`
- ✓ Type naming: `CamelCase`
- ✓ 80-character line length preference
- ✓ Braces on same line (except function definitions)

## Build Instructions

### Configuration

```bash
cd qemu
mkdir build && cd build

# Standard build
../configure \
    --target-list=riscv64-softmmu,riscv64-linux-user \
    --enable-debug

# Build
make -j$(nproc)
```

### Verification

```bash
# Check extension is recognized
./qemu-system-riscv64 -cpu rv64,help | grep zcx

# Run tests
make check-qtest
```

## Future Work

### Planned Enhancements

1. **Extension Enumeration**
   - Add CSRs to query available extension IDs
   - Provide extension capability descriptors

2. **Dynamic Extension Loading**
   - Support runtime addition of custom extensions
   - Plugin-based extension architecture

3. **CX Instruction Decoding**
   - Implement instruction routing based on `cxsel`
   - Add TCG translation for CX instruction family

4. **Multi-Hart Coordination**
   - Per-hart extension selection
   - Synchronized extension configuration

5. **Security Features**
   - Extension isolation mechanisms
   - Privilege-level access control for extension configuration

### Known Limitations

1. `cxsel` is read-only (always 0) - extension selection not yet implemented
2. No actual CX instruction routing - infrastructure only
3. No validation of extension IDs - accepts any value
4. No extension discovery mechanism
5. Single built-in extension (ID 0) only

## References

### QEMU Documentation

- **Build System:** docs/devel/build-system.rst
- **Testing:** docs/devel/testing/main.rst
- **RISC-V Target:** docs/system/target-riscv.rst
- **Coding Style:** docs/devel/style.rst

### RISC-V Specifications

- **Privilege Spec v1.13:** https://github.com/riscv/riscv-isa-manual
- **Custom Extensions:** RISC-V Custom Extension Guidelines
- **CSR Addressing:** RISC-V Privileged Architecture §2.2

### Source Code Locations

- Main implementation: `target/riscv/cx.{c,h}`
- CSR operations: `target/riscv/csr.c` — predicates `cxsel`/`cxsidx`/`cxsdata`, handlers `read_cxsel`, `write_cxsel`, `read_cxsidx`, `write_cxsidx`, `op_cxsdata`
- CPU state: `target/riscv/cpu.h` — `CPURISCVState` fields `cxsel`, `cxsidx`, `cxsdata`
- Configuration: `target/riscv/cpu.c` — `ISA_EXT_DATA_ENTRY(zcx)`, `MULTI_EXT_CFG_BOOL("zcx")`, reset in `riscv_cpu_reset`
- KVM support: `target/riscv/kvm/kvm-cpu.c` — `KVM_EXT_CFG("zcx", ext_zcx, KVM_RISCV_ISA_EXT_ZCX)`

## Commit History

see `../docs/progress.md`

## Contact

**Maintainer:** Christopher Dunn  
**Email:** christopher.m.dunn@gmail.com  
**Repository:** QEMU RISC-V  
**Branch:** cxtg

---

**Document Version:** 2.0  
**Last Updated:** 2026-05-19  
**QEMU Version:** Development (post-9.2)
