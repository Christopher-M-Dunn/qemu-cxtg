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

Example from Zcx extension (current branch):
- Configuration: `ext_zcx` field in RISCVCPUConfig
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

## QEMU Build Automation

**Consult `docs/flowchart_QEMU_automation.svg` before adding any instruction, helper, trace event, extension flag, or source file.** QEMU has several interacting automation layers; editing a generated output file instead of its entry point produces silent failures or is overwritten on the next build. Always modify the topmost layer appropriate to the task.

### What is auto-generated (do not edit directly)

| Entry point (you write) | Build system generates |
|---|---|
| `target/riscv/insn32.decode` | `build/.../decode-insn32.c.inc` — arg structs, bit-extract, dispatch tree; calls `trans_NAME()` by name |
| `target/riscv/helper.h` — `DEF_HELPER_N(...)` | `helper-proto.h` C prototype + `helper-gen.h` `gen_helper_*()` TCG wrapper |
| `target/riscv/trace-events` — one line per event | `trace/generated-events.h` — `trace_*()` callable function |
| `target/riscv/cpu_cfg_fields.h.inc` — `BOOL_FIELD(ext_zcx)` | Field in `RISCVCPUConfig` struct |
| `target/riscv/cpu.c` — `MULTI_EXT_CFG_BOOL` + `ISA_EXT_DATA_ENTRY` | QOM property getter/setter; `-cpu rv64,zcx=on` parsing; ISA string output |

### Fully manual (not wired to any generator)

`disas/riscv.c` — three independent additions required per new instruction: (1) enum entry, (2) opcode list entry, (3) decode case in the SYSTEM opcode block. Changes to `.decode` files do **not** update this file.

### Key rules

- `insn_trans/trans_*.c.inc` files are `#include`d directly into `translate.c` — do **not** add them to `meson.build`.
- New `.c` source files go in `target/riscv/meson.build` (`riscv_ss.add`) — this compiles them into both riscv32 and riscv64 targets.
- The `tl` type in `DEF_HELPER_N` is `target_ulong` — 32-bit in an RV32 build, 64-bit in an RV64 build.

### Quick reference

| Change | Files to touch | Auto-generated for you |
|---|---|---|
| New instruction | `insn32.decode`, `insn_trans/trans_X.c.inc`, `helper.h`, `cx.c`, `disas/riscv.c` | arg struct, dispatch, `gen_helper_*` wrapper |
| New extension flag | `cpu_cfg_fields.h.inc`, `cpu.c` (×2: MULTI_EXT + ISA_EXT), `kvm-cpu.c` | QOM getter/setter, `-cpu` flag parsing, ISA string |
| New helper | `helper.h` (`DEF_HELPER_N`), `cx.c` (body) | `gen_helper_*()` inline TCG wrapper, C prototype |
| New trace point | `trace-events` (1 line) | `trace_*()` callable, conditional no-op |
| New `.c` file | `target/riscv/meson.build` | Compiled into both targets |
| Disassembler | `disas/riscv.c` — enum, opcode list, decode case | Nothing — fully manual |
| `trans_*.c.inc` | `translate.c` (`#include` only) | Compiled as part of `translate.c` TU |

## Documentation

- **docs/devel/style.rst** - Coding style
- **docs/devel/build-system.rst** - Build system details
- **docs/devel/testing/main.rst** - Test infrastructure
- **docs/system/target-riscv.rst** - RISC-V machines and features
- **docs/specs/riscv-aia.rst** - Advanced Interrupt Architecture
- **docs/specs/riscv-iommu.rst** - IOMMU specification

## CX Extension Workflow

This repo implements the **Zcx/ZcxMulti** composable extensions. It lives as a submodule inside `runtime-cxtg/`.

### Branches

```
master        ← upstream QEMU; do not touch
cxtg          ← stable; merge only at phase milestones, always tagged
cxtg-dev      ← integration branch; all feat/ branches merge here
feat/<block>  ← one branch per block, cut from cxtg-dev
```

### Per-block workflow

1. Cut `feat/<block-id>` from `cxtg-dev`.
2. Implement the feature.
3. All block tests pass.
4. PR `feat/<block-id>` → `cxtg-dev`; review diff; merge.
5. At phase milestone (all blocks in phase green): merge `cxtg-dev` → `cxtg` and tag (e.g. `cxtg-v0.phase1`).
6. After tagging: in `runtime-cxtg`, commit the updated submodule pointer on `cxtg-dev`, merge `cxtg-dev` → `cxtg`, and apply the same tag.

**Rule:** never commit directly to `cxtg` or `cxtg-dev`. All changes come through a `feat/` branch.

**Commits:** always ask the user "Ready to commit?" and wait for confirmation before staging any files or running `git add` / `git commit`.

### Implementation status

See `runtime-cxtg/docs/progress.md` for current block status and release notes.
See `runtime-cxtg/docs/CXTG_QEMU_Action_Plan.md` for full block specs.
See `runtime-cxtg/docs/todo.md` for deferred implementation decisions.

## Wiki

Every file touched by this project has a corresponding wiki page under `runtime-cxtg/wiki/runtime-cxtg/qemu-cxtg/`. The wiki is a live reference manual — present tense, no history, no phase annotations.

**Format rules:** read `runtime-cxtg/wiki/schema.md` before writing or editing any wiki page.

**Update trigger — after every file write:** update the wiki immediately after writing or editing any source file, before writing the next file. The wiki update is part of the write action, not a post-step. Do not batch wiki updates to commit time.

**When renaming a symbol:** update the sub-page filename and the bullet link text in the corresponding Format B main page. The sub-page `.md` filename must match the current symbol name — a stale filename breaks Obsidian links silently.

**New files:** if the file has no wiki page yet, add it to `runtime-cxtg/wiki/index.md` and create its page at `runtime-cxtg/wiki/runtime-cxtg/qemu-cxtg/<path-to-file>.md` before moving on.
