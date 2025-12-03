# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

QEMU uses a two-stage build system: configure script + Meson.

```bash
# Standard build
mkdir build && cd build
../configure --target-list=riscv32-softmmu,riscv64-softmmu,riscv32-linux-user,riscv64-linux-user
make -j$(nproc)

# Debug build
../configure --enable-debug --target-list=riscv64-softmmu
make

# List configure options
../configure --help
```

The configure script creates Python virtual environment for build dependencies and generates config-host.mak and config-meson.cross for Meson. Build artifacts are placed in the build/ directory outside the source tree.

## Testing

```bash
# Core test commands (run from build directory)
make check                  # All quick tests
make check-unit             # Unit tests only
make check-qtest            # Device emulation tests
make check-functional       # Full VM boot tests
make check-tcg              # TCG translation tests
SPEED=slow make check       # Include slow tests
V=1 make check-unit         # Verbose output

# RISC-V functional tests
python3 -m pytest tests/functional/riscv64/test_opensbi.py -v
python3 -m pytest tests/functional/riscv64/test_sifive_u.py -v

# Code style checking
scripts/checkpatch.pl --no-tree -f target/riscv/cpu.c
```

## RISC-V Architecture Overview

### Key Directories

- **target/riscv/** - CPU emulation, instruction translation, CSR implementation
  - `cpu.h` - CPU state (CPURISCVState), privilege levels, extension definitions
  - `cpu.c` - CPU initialization, extension registration, configuration
  - `csr.c` - Control and Status Register implementations (~8000 lines)
  - `translate.c` - Instruction decoding and TCG translation
  - `cpu_bits.h` - CSR addresses and bit field definitions
  - `cpu_cfg_fields.h.inc` - Configuration field macros for extensions
  - `kvm/` - KVM acceleration support

- **hw/riscv/** - Board and platform implementations
  - `virt.c` - Virtual RISC-V machine (main development platform)
  - `sifive_u.c` - SiFive U-series boards
  - `spike.c` - Spike ISA simulator
  - `riscv-iommu.c` - RISC-V IOMMU implementation
  - `boot.c` - Firmware/bootloader support

- **include/hw/riscv/** - RISC-V hardware headers

- **tests/functional/riscv{32,64}/** - Python-based functional tests
- **tests/qtest/** - C-based device tests (riscv-iommu-test.c, riscv-csr-test.c)

### Extension System Pattern

RISC-V extensions follow this pattern:

1. Define configuration field in `RISCVCPUConfig` (cpu.h)
2. Add to `cpu_cfg_fields.h.inc` using FIELD macros
3. Register extension in `cpu.c` (riscv_cpu_add_*_properties)
4. Implement CSR operations in `csr.c` with predicates and read/write handlers
5. Add KVM support in `kvm/kvm-cpu.c` if applicable
6. Update ISA string parsing if needed

Example from Zicx extension (current branch):
- Configuration: `ext_zicx` field in RISCVCPUConfig
- CSR: `cxsel` register for custom extension selection
- CSR ops: predicates check if extension enabled, handlers manage register state
- Initialization: cxsel set to 0 at reset (built-in custom extension)

### CPU State and Privilege Levels

CPURISCVState (in cpu.h) contains:
- General purpose registers (gpr[32])
- CSRs organized by privilege level (M/H/S/U)
- Extension state (vector, float, crypto)
- Current privilege level (priv)
- Memory management state (satp, hgatp)

Privilege levels: PRV_U (0), PRV_S (1), PRV_H (2), PRV_M (3)

### CSR Implementation

CSRs are registered in csr_ops[] table in csr.c with:
- Name and CSR address (from cpu_bits.h)
- Predicate function - checks if CSR accessible in current mode/config
- Read/write functions - implement CSR behavior
- Optional min_priv_ver - minimum privilege spec version

Pattern:
```c
static RISCVException read_mycsr(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = env->mycsr;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mycsr(CPURISCVState *env, int csrno, target_ulong val)
{
    env->mycsr = val;
    return RISCV_EXCP_NONE;
}

/* In csr_ops[] */
[CSR_MYCSR] = { "mycsr", ext_predicate, read_mycsr, write_mycsr }
```

## Architectural Layers

QEMU RISC-V implementation has four main layers:

1. **Target Layer** (target/riscv/)
   - CPU state definition and management
   - Instruction decoding (translate.c)
   - Target-specific helpers
   - CSR operations

2. **Accelerator Layer** (accel/)
   - TCG: software binary translation (most common for development)
   - KVM: hardware virtualization (Linux hosts)
   - Each provides target-specific implementations in target/riscv/{tcg,kvm}/

3. **Device Layer** (hw/)
   - Device models organized by type (block, network, char, etc.)
   - RISC-V specific devices in hw/riscv/
   - Uses QOM (QEMU Object Model) for abstraction

4. **Machine Layer** (hw/riscv/)
   - Board definitions (virt, sifive_u, spike)
   - Instantiate CPU, memory, and devices
   - Define memory map and boot configuration

## Code Style Guidelines

From docs/devel/style.rst and .editorconfig:

**Whitespace**
- 4-space indentation (NO tabs except Makefiles)
- 80-character line length (hard limit at 100)
- No trailing whitespace

**Naming**
- Variables: `lower_case_with_underscores`
- Types/Structs: `CamelCase`
- Common variables: `cs` (CPUState), `env` (CPURISCVState), `dev` (DeviceState)

**Control Flow**
- Opening brace on same line (except function definitions)
- Always use braces, even for single statements
- `else if` treated as single statement

**Comments**
- C-style `/* */` only (NO `//` comments)
- Multiline blocks with left-aligned stars

**Includes**
- ALWAYS first: `#include "qemu/osdep.h"` (in .c files only)
- Order: osdep.h, system headers `<...>`, QEMU headers `"..."`
- Don't include osdep.h in .h files

**Example**
```c
#include "qemu/osdep.h"
#include <sys/types.h>
#include "cpu.h"
#include "qemu/log.h"

static RISCVException read_csr(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    if (env->priv < PRV_M) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    /* Read the CSR value */
    *val = env->my_csr;
    return RISCV_EXCP_NONE;
}
```

## Build System Internals

Meson sourcesets organize code into four categories:

1. **Subsystem sourcesets** (chardev_ss, block_ss, etc.) - shared across targets
2. **Target-independent** (common_ss, system_ss, user_ss) - linked into all emulators
3. **Target-dependent** (specific_ss) - CPU and device code per target
4. **Module sourcesets** - optional loadable modules

When adding new files:
- Add to appropriate sourceset in meson.build
- Target-specific code goes in target/riscv/meson.build
- Device code goes in hw/riscv/meson.build

## Documentation

- **docs/devel/style.rst** - Coding style
- **docs/devel/build-system.rst** - Build system details
- **docs/devel/testing/main.rst** - Test infrastructure
- **docs/system/target-riscv.rst** - RISC-V machines and features
- **docs/specs/riscv-aia.rst** - Advanced Interrupt Architecture
- **docs/specs/riscv-iommu.rst** - IOMMU specification

## Current Branch: feature/cx

Working on Zicx extension implementation:
- Adds `cxsel` CSR for custom extension selection
- Extension config field: `ext_zicx`
- Files modified: cpu.h, cpu.c, csr.c, kvm/kvm-cpu.c, cpu_cfg_fields.h.inc
- Follows standard extension pattern described above
