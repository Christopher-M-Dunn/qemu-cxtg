# RISC-V Zicx Extension Implementation Specification

## Overview

This document specifies the implementation of the **Zicx (Composable Extensions)** extension for QEMU's RISC-V target. The Zicx extension provides a standardized interface for multiplexing custom extensions through Control and Status Registers (CSRs), enabling dynamic selection and configuration of composable custom extensions.

**Project:** QEMU RISC-V Emulator  
**Branch:** feature/cx  
**Author:** Artur Lojewski (lojewski@gmail.com)  
**Status:** In Development  

## Extension Purpose

The Zicx extension addresses the challenge of managing multiple custom RISC-V extensions by providing:

1. **Extension Selection** - Runtime selection of active custom extension via `cxsel` CSR
2. **Extension Configuration** - Indexed access to extension parameters via `cxsidx` and `cxsdata` CSRs
3. **Built-in Support** - A default built-in custom extension (selector value 0)
4. **Multiplexing** - Routing of CX instructions to appropriate extension handlers

## Architecture

### CSR Definitions

The Zicx extension introduces three new Machine-mode CSRs in the custom CSR space (0x800-0x8FF):

| CSR Address | Name      | Access | Description                           |
|-------------|-----------|--------|---------------------------------------|
| 0x800       | `cxsel`   | RO     | Custom Extension Selector             |
| 0x801       | `cxsidx`  | RW     | Custom Extension Index                |
| 0x802       | `cxsdata` | RW     | Custom Extension Data                 |

**Note:** CSR addresses are defined in `target/riscv/cpu_bits.h`

### CSR Behavior

#### CXSEL - Custom Extension Selector (0x800)

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

**Implementation:** `target/riscv/csr.c:1453-1477`

#### CXSIDX - Custom Extension Index (0x801)

**Purpose:** Indirect addressing index for extension configuration

**Properties:**
- **Access:** Read-Write
- **Reset Value:** 0
- **Width:** XLEN bits (32 or 64)

**Usage:**
- Provides an index into extension-specific configuration space
- Combined with `cxsdata` for indirect configuration access
- Interpretation is extension-dependent

**Implementation:** `target/riscv/csr.c:1460-1468, 1482-1487`

#### CXSDATA - Custom Extension Data (0x802)

**Purpose:** Data register for indexed extension configuration

**Properties:**
- **Access:** Read-Write
- **Reset Value:** 0
- **Width:** XLEN bits (32 or 64)

**Usage:**
- Read/write configuration data at offset specified by `cxsidx`
- Enables array-like access to extension parameters
- Interpretation is extension-dependent

**Implementation:** `target/riscv/csr.c:1470-1476, 1489-1494`

## Implementation Details

### File Structure

```
target/riscv/
├── cpu.h                    # CPURISCVState structure, ext_zicx field
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

The `CPURISCVState` structure (cpu.h:520-522) is extended with three fields:

```c
struct CPUArchState {
    /* ... existing fields ... */
    
    /* CX extension */
    target_ulong cxsel;   /* Extension selector (read-only) */
    target_ulong cxidx;   /* Configuration index */
    target_ulong cxdata;  /* Configuration data */
};
```

### Configuration System

**Extension Flag:** `ext_zicx` (Boolean)

**Registration:**
- Configuration field: `cpu_cfg_fields.h.inc:35` - `BOOL_FIELD(ext_zicx)`
- Property definition: `cpu.c:1262` - `MULTI_EXT_CFG_BOOL("zicx", ext_zicx, false)`
- ISA string entry: `cpu.c:120` - `ISA_EXT_DATA_ENTRY(zicx, PRIV_VERSION_1_10_0, ext_zicx)`
- KVM mapping: `kvm/kvm-cpu.c:301` - `KVM_EXT_CFG("zicx", ext_zicx, KVM_RISCV_ISA_EXT_ZICX)`

**Enabling:**
```bash
qemu-system-riscv64 -cpu rv64,zicx=on ...
```

### Reset Behavior

On CPU reset (`cpu.c:802-805`):

```c
if (riscv_cpu_cfg(env)->ext_zicx) {
    env->cxsel = 0;   /* Reset to built-in extension */
    env->cxidx = 0;   /* Clear index */
    env->cxdata = 0;  /* Clear data */
}
```

### CSR Predicate Functions

Three predicate functions control CSR accessibility (`csr.c:5862-5907`):

```c
static RISCVException cxsel(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_cfg(env)->ext_zicx) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException cxsidx(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_cfg(env)->ext_zicx) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException cxsdata(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_cfg(env)->ext_zicx) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}
```

### CSR Operation Handlers

The core CSR operations are implemented in `target/riscv/cx.c`:

**Read Operations:**
```c
void cxsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong *val)
{
    *val = env->cxsel;
    trace_cxsel_csr_read(env->mhartid, reg_index, *val);
}

void cxsidx_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong *val)
{
    *val = env->cxidx;
    trace_cxsidx_csr_read(env->mhartid, reg_index, *val);
}

void cxsdata_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong *val)
{
    *val = env->cxdata;
    trace_cxsdata_csr_read(env->mhartid, reg_index, *val);
}
```

**Write Operations:**
```c
void cxsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxsel_csr_write(env->mhartid, reg_index, val);
    /* Note: CSR cxsel is read-only! */
    /* Write is traced but not applied */
}

void cxsidx_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    env->cxidx = val;
    trace_cxsidx_csr_write(env->mhartid, reg_index, val);
}

void cxsdata_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    env->cxdata = val;
    trace_cxsdata_csr_write(env->mhartid, reg_index, val);
}
```

### CSR Registration

CSRs are registered in the global `csr_ops[]` table (`csr.c:6785-6787`):

```c
[CSR_CXSEL]   = { "cxsel",   cxsel,   read_cxsel,   write_cxsel   },
[CSR_CXSIDX]  = { "cxsidx",  cxsidx,  read_cxsidx,  write_cxsidx  },
[CSR_CXSDATA] = { "cxsdata", cxsdata, read_cxsdata, write_cxsdata },
```

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
csrr t0, 0x800          # t0 = cxsel (should be 0 after reset)
```

### Example 2: Configure Extension via Indexed Access

```assembly
# Write to extension configuration array
li   t0, 5              # Index = 5
csrw 0x801, t0          # cxsidx = 5

li   t1, 0xDEADBEEF     # Configuration value
csrw 0x802, t1          # cxsdata[5] = 0xDEADBEEF

# Read back configuration
csrr t2, 0x802          # t2 = cxsdata[5]
```

### Example 3: QEMU Command Line

```bash
# Enable Zicx extension
qemu-system-riscv64 \
    -cpu rv64,zicx=on \
    -machine virt \
    -kernel my_kernel.elf

# With tracing
qemu-system-riscv64 \
    -cpu rv64,zicx=on \
    -machine virt \
    -trace 'cxsel_*' \
    -kernel my_kernel.elf
```

## Testing

### Unit Tests

1. **CSR Accessibility**
   - Verify CSRs are accessible when `ext_zicx=true`
   - Verify illegal instruction exception when `ext_zicx=false`

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
def test_zicx_csrs(self):
    """Test Zicx CSR access"""
    # Boot with Zicx enabled
    # Execute CSR read/write instructions
    # Verify expected behavior
```

## Compliance

### RISC-V Privilege Specification

- **Privilege Level:** Machine-mode (M-mode) CSRs
- **Minimum Priv Spec:** v1.10.0
- **CSR Address Space:** Custom (0x800-0x8FF)
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
./qemu-system-riscv64 -cpu rv64,help | grep zicx

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
- CSR operations: `target/riscv/csr.c` (lines 1440-1494, 5862-5907, 6785-6787)
- CPU state: `target/riscv/cpu.h` (line 520-522)
- Configuration: `target/riscv/cpu.c` (lines 120, 802-805, 1262)
- KVM support: `target/riscv/kvm/kvm-cpu.c` (line 301)

## Commit History

Key commits on `feature/cx` branch:

```
d65195f981 - target/riscv: add Composable Extension document
fb319ac3fd - target/riscv: remove CX CSR cxsetsel and fix typos
64a1e2acf0 - target/riscv: read/write CX CSRs from/into RISC-V CPU state
3a499be571 - target/riscv: add tracing options for all CX CSR registers
dca666dd27 - target/riscv: add CSR cxsetsel, cxidx and cxdata
d3eb9918ba - target/riscv: add CSR cxsel
828b181ec9 - target/riscv: add Zicx extension
```

## Contact

**Maintainer:** Artur Lojewski  
**Email:** lojewski@gmail.com  
**Repository:** QEMU RISC-V  
**Branch:** feature/cx

---

**Document Version:** 1.0  
**Last Updated:** 2025-12-29  
**QEMU Version:** Development (post-9.2)
