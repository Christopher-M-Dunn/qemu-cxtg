# Requirements

**Authors:** Artur Lojewski, Christopher Dunn

**Description:** This document contains all of the requirements for implementing the Composable Custom Extensions ISA extensions (Zcx, ZcxMulti) in QEMU. All requirements come directly from the spec document (info below). Open questions and working assumptions regarding any gaps due to inconsistent, incomplete, ambiguous, or misunderstood language in the spec document can be found in the Discussion section at the end of this document.

**Spec Title:** RISC-V Composable Custom Extensions  
**Spec Authors:** RISC-V Composable Custom Extensions Task Group (TG)  
**Spec Version:** v0.0.0, 2026-03-16: Draft  
**Spec Location:** https://github.com/riscv/composable-custom-extensions.git

---

## Unprivileged CX Multiplexing (`isa-unpriv`)

### `cxsel` CSR

* `cxsel` is a read-only (URO) XLEN-wide CSR.
* `cxsel` can **only** be updated by the `cxsetsel` instruction; it cannot be written via Zicsr instructions.
* The following text is quoted verbatim due to implementation-critical nuance around validity and clamping:

  > `cxsel` is a WARL register that must be able to hold all valid selector values. It need not be capable of holding all possible invalid selector values. Prior to writing `cxsel`, implementations may convert an invalid value into some other invalid value that `cxsel` is capable of holding.
  >
  > Valid selector values are 0, which indicates the built-in custom extension, and any value returned by the platform specific discovery mechanism for use as a selector.
  >
  > A value of all 1s is an invalid selector value.
  >
  > The all 1s value may be used by software to aid in debugging uninitialized variables. It is not required that the all 1s value be a legal value.

* `cxsel` is 0 at reset.
* A CX selector value may be hart-specific.

### `cxsetsel` Instruction

* `cxsetsel` is a non-custom 32-bit instruction (encoding TBD).
* `cxsetsel rd,rs1` atomically swaps the value in `cxsel` and an integer register.
* Writes the initial value of `x[rs1]` to `cxsel`.
* If `rd` is not `x0`, writes the initial value of `cxsel` to `x[rd]`.

### CX Multiplexing Behavior

The following text is quoted verbatim due to the tricky interaction between `cxsel` validity and illegal instruction behavior:

> When `cxsel` is 0, CX instructions are executed by the built-in custom extension.
>
> When `cxsel` is any other valid value, CX instructions are executed by the selected extension.
>
> When `cxsel` is invalid, CX instructions are treated as illegal instructions.

---

## Privileged CSRs (`isa-priv`)

New CSRs for `Zcx`:

| Address | Privilege | Name     | Description                        |
|---------|-----------|----------|------------------------------------|
| 0xTBD   | SRW       | `scxstp` | CX selection translation pointer   |
| 0xTBD   | SRW       | `scxxs0` | CX context status                  |
| 0xTBD   | SRW       | `scxxs1` | CX context status (RV32 only)      |
| 0xTBD   | SRW       | `scxxs2` | CX context status                  |
| 0xTBD   | SRW       | `scxxs3` | CX context status (RV32 only)      |

New CSRs for `Zcxmulti`:

| Address | Privilege | Name       | Description                          |
|---------|-----------|------------|--------------------------------------|
| 0xTBD   | SRW       | `scx1xs0`  | CX 1 context status                  |
| 0xTBD   | SRW       | `scx1xs1`  | CX 1 context status (RV32 only)      |
| ⋮       |           |            |                                      |
| 0xTBD   | SRW       | `scx63xs0` | CX 63 context status                 |
| 0xTBD   | SRW       | `scx63xs1` | CX 63 context status (RV32 only)     |

* `scxstp` and `scxxs0`–`scxxs3` are present when `Zcx` and the supervisor extension (`S`) are present.
* `scxNxs` registers are present when `Zcxmulti` and `S` are present.
  * Note: the spec text says "The privileged CSRs and opcodes are present when `Zcxmulti` is present and the supervisor extension is present," which read in isolation would condition `scxstp`/`scxxs0`–`scxxs3` on `Zcxmulti` as well; this doc assumes the narrower reading — see Discussion.
* When the supervisor extension is not present, `cxsel` is interpreted as in Direct mode.

### `scxstp` — CX Selection Translation Pointer

* `scxstp` is a WARL read-write XLEN-wide CSR.
* Bits `[XLEN-1:XLEN-4]` are the `mode` field:

  | Value | Meaning                    |
  |-------|----------------------------|
  | 0     | Disabled                   |
  | 1     | Direct                     |
  | 2     | Indirect (`Zcxmulti` only) |
  | 3–15  | Reserved                   |

#### Disabled Mode

* Remaining bits of `scxstp` are reserved.
* All use of CX opcodes and CX CSRs triggers an illegal instruction exception.
* The custom opcode space maps to the built-in custom extension.

#### Direct Mode

* Remaining bits of `scxstp` are reserved.
* `cxsel` is interpreted directly as a CX ID and context index:
  * Bits `[7:0]` — CXID (8-bit CX identifier)
  * Bits `[15:8]` — IDX (8-bit context index)
  * Bits `[XLEN-1:16]` — Reserved

#### Indirect Mode (`Zcxmulti` only)

* The lower bits of `scxstp` hold a Physical Page Number (PPN) pointing to a 4 KiB CX selection translation table:
  * RV32: bits `[31:28]` = mode (4), bits `[27:22]` = Reserved (6), bits `[21:0]` = PPN (22)
  * RV64: bits `[63:60]` = mode (4), bits `[59:44]` = Reserved (16), bits `[43:0]` = PPN (44)
* Each entry in the translation table is 32 bits:
  * Bit `[31]` — V (valid)
  * Bits `[30:16]` — Reserved
  * Bits `[15:8]` — IDX (8-bit context index)
  * Bits `[7:0]` — CXID (8-bit CX identifier)
* `cxsel` is used as an index into the translation table.
* If the selected table entry has V = 0: CX opcode access raises illegal instruction.
* If the selected table entry specifies an invalid IDX or CXID: CX opcode access raises illegal instruction.
* If all fields are valid: CX opcodes are executed by the indicated CXID at context index IDX.

### `scxxs0`–`scxxs3` — CX Context Status

* Four SRW registers hold 2-bit context state status for each CX ID; bit placement is determined by CX ID, analogous to `FS`/`VS` in `mstatus`.
* Layout:
  * For `scxxsN`, bits `[2k+1:2k]` = status for CX ID `(16 × N) + k`.
  * RV32: all four registers valid; k ∈ {0–15} (16 CX IDs per 32-bit register).
    * `scxxs0` → CX IDs 0–15; `scxxs1` → 16–31; `scxxs2` → 32–47; `scxxs3` → 48–63.
  * RV64: `scxxs1` and `scxxs3` are invalid; k ∈ {0–31} (32 CX IDs per 64-bit register).
    * `scxxs0` → CX IDs 0–31; `scxxs2` → CX IDs 32–63.
* If the status bits for a given CX ID are `Off`: any attempt to execute custom opcodes for that CX ID raises an illegal instruction exception.
* Without `Zcxmulti`: bits for CX IDs > 0 are writable and reflect the status of (and control) context zero of the respective extension.
* With `Zcxmulti`: bits for CX IDs > 0 are read-only and reflect a summary of the maximal status of all contexts for the respective extension.

### `scxNxs0`/`scxNxs1` — Additional Extension Status (`Zcxmulti`)

* Registers `scx1xs0`/`scx1xs1` through `scx63xs0`/`scx63xs1` (N = 1–63).
* Present only if `Zcxmulti` is implemented and an extension with the corresponding CX ID is present.
* Layout:
  * RV32: a pair of registers per CX ID (`scxNxs0` and `scxNxs1`); bits `[2k+1:2k]` = status for context index `(16 × register_index) + k`.
  * RV64: only the even register (`scxNxs0`) is present; bits `[2k+1:2k]` = status for context index `k` (up to 32 contexts).
* Meaning of bits is analogous to `scxxs` bits for each extension when `Zcxmulti` is not present.
* `Zcxmulti` requires `Sscsrind`; the `scxNxs` registers are expected to become indirect CSRs.

### Smstateen

* The CX framework adds a `C` bit to `mstateen0`, `sstateen0`, and `hstateen0`.
* `Smstateen` is not required by Composable Custom Extensions.
* `mstateen0.C`, `sstateen0.C`, and/or `hstateen0.C` must be set to enable access to custom state (including CX state), depending on which extensions are present:
  * `mstateen0.C` — gates all lower-privilege access; must be set on any system using CX state from S/U/VS/VU mode.
  * `sstateen0.C` — gates U-mode access when S-mode is present and `mstateen0.C` is set.
  * `hstateen0.C` — gates VS/VU-mode access when the H-extension is present and `mstateen0.C` is set.

### `scxdiscard`

* `scxdiscard` quickly overwrites the context selected by `cxsel`; the resulting state value is UNSPECIFIED.
* The corresponding XS bits for the context are set to `Initial`.
* Support is optional; presence is indicated by the platform-specific discovery mechanism.
* If the selected CX ID or context index is invalid: raises illegal instruction exception.
* If the selected CX does not support the discard operation: behavior is UNSPECIFIED.

---

## Unprivileged State Context Management (`isa-state`)

### `cxsidx` CSR

* `cxsidx` is a read-write WARL XLEN-wide CSR.
* Specifies the index of the word of CX state context data to access via `cxsdata`.
* Can represent any valid index of the state context data of the selected CX.
* `cxsidx` is **undefined** following `cxsetsel`.
* Valid values are 0 through `size-1`, where `size` is the size in words of the selected CX's state context data.
  * State context size is obtained through the platform-specific discovery mechanism.
  * State context size may vary across systems but is constant during execution on a given system.
* Writing an invalid index may result in a valid or invalid legal value (WARL); e.g., a 2-bit `cxsidx` for a 4-word CX can only represent values 0–3, all of which are valid indices.

### `cxsdata` CSR

* `cxsdata` is a read-write WARL XLEN-wide CSR used to read and/or write one word of the selected CX's state context data.
* On read (`csrrs rd,cxsdata,x0`): one word at valid index `cxsidx` is written to `rd`.
* On write (`csrrw x0,cxsdata,rs1`): source value is written to one word at valid index `cxsidx`.
* On read+write (`csrrw rd,cxsdata,rs1`): both effects occur.
* `csrrs`/`csrrc` variant: sets/clears bits of the indexed word and writes the original value to `rd`.
* Following each `cxsdata` access: `cxsidx + 1` is written to `cxsidx`.
* Following read/write of the last word of the selected CX's state context data: `cxsidx` is **undefined**.
* When `cxsel` is 0 or invalid, or when `cxsidx` is not a valid index: `cxsdata` access is **undefined**.

### `cxdiscard`

* `cxdiscard` informs the runtime that the context state data specified by `cxsel` is no longer useful and need not be saved.
* Does **not** actually write context state data; context state is UNSPECIFIED for ABI purposes after `cxdiscard`.
* Raises illegal instruction if the corresponding XS bits are `Off`.
* Changes XS to `Initial`.
* Raises illegal instruction if `cxsel` specifies an invalid selection.

### Example CX Context Switch (non-normative)

Quoted verbatim — this is the spec's model for the save/restore loop the runtime and tests will implement ("This code selects and swaps a CX state context with a previously saved state context blob"):

```asm
;; a0: mycx selector
;; a1: address of CX state context blob
;; a2: address just past CX state context blob
    cxsel a0            ; select mycx
    csrw cxsidx,x0      ; zero cxsidx
loop:
    ld t0,(a1)          ; load a word
    csrrw t1,cxsdata,t0 ; swap (R/W) it
    sd t1,(a1)          ; save a word
    add a1,a1,8         ; next word of blob
    blt a1,a2,loop      ; do-while
```

Implementation notes:

* The example is RV64 (`ld`/`sd`, 8-byte stride); word size tracks XLEN.
* The swap uses `csrrw` on `cxsdata` — the same pass both saves the old context and loads the new one, relying on the `cxsidx` auto-increment.
* `cxsidx` is explicitly zeroed after selecting (consistent with `cxsidx` being undefined following `cxsetsel`).
* The example uses a `cxsel a0` mnemonic where `cxsetsel` appears to be meant — possibly earlier naming (the Linux `cx.rst` in Appendix A uses the same phrasing).

---

## Composability Criteria (`criteria`)

Defines which custom extensions qualify as *composable*. A composable custom extension (CX) is a custom extension that satisfies the Composability Criteria. The stated goals (non-normative) are **composition invariance** (a CX's functional behavior is unchanged in the presence or absence of other CXs) and **portability** (the functional behavior of a system of harts and CXs is the same regardless of what hardware implements them).

### Definitions

* **Custom instruction** — a RISC-V instruction with a major opcode of `custom-0`/`-1`/`-2`/`-3`.
* **Custom extension** — an abstract instruction set *contract*: a set of custom instructions plus a set of RISC-V custom CSRs plus extension state, all with a specified *functional behavior* and state model.
* **Custom operation** — a custom instruction or a custom CSR access.
* **Functional behavior** — the set of emergent observable side-effects (observable by standard RISC-V instructions) that must occur upon performing the specified custom operations. Excludes timing-based behavior.
* **Composable custom extension (CX)** — a custom extension that satisfies the Composability Criteria.

### Criteria

A custom extension is composable **if-and-only-if** all of the following hold:

1. Its instructions appear to execute, one-at-a-time, in program order on the local hart.
2. Quoted verbatim due to the implementation-critical composable-state list (and a possible inconsistency regarding PC — see Discussion):

   > Each of its instructions only reads some *composable state*, computes a pure function of this state (and no other state), then either raises an exception or writes some composable state. *Composable state* comprises the selected extension's state, including its custom CSRs, plus the hart's integer registers, floating-point registers, `fcsr` CSR, vector registers, vector context status in `mstatus` and in `vsstatus`, `vtype`, `vl`, `vlenb`, `vstart`, `vcsr` CSRs, PC, and loads and stores to memory as if performed by the local hart.

3. Instructions that access memory appear to execute in program order on the local hart; they follow RVWMO at the instruction level, and additionally RVTSO at the instruction level if the Ztso extension is implemented.
4. Instructions that access memory may raise the usual memory access exceptions.
5. The extension is specified so that any implementation of the extension exhibits *identical* functional behavior.
6. A CX may have *extension state*. An implementation of the extension may support zero, one, or more state instances per hart.

Non-normative caveats the spec itself attaches to these criteria (areas it marks as still under development):

* Accessing privileged state (e.g., in `mstatus`) may be problematic and worth disallowing.
* "Selected extension's state" behavior needs more specification for non-idempotence, ordering, and access by other extensions.
* The memory criterion needs more specific language on ordering, partial completion, exceptions, and PMA memory types (in particular non-idempotent regions).
* "Identical" functional behavior may be too strong (e.g., it denies composability of a true random number generator extension).
* Isolation consequence: a CX cannot be specified to read/write extension state of another CX, or even of another instance of itself.

### Composability Examples (non-normative)

Useful as test-design references:

| Extension | Composable? | Reason |
|---|---|---|
| `dotprod rd,rs1,rs2` (`accum += X[rs1]*X[rs2]`) | Yes | Pure function of int regs + own state |
| `dotprod2` (reads `X[rs1+1]`, `X[rs2+1]` too) | Yes | Still only int regs + own state |
| `hash16 rd` (`X[rd] = hash(x16..x31)`) | Yes | |
| `hash4KB rd,disp(rs1)` (hashes 4 KiB of memory) | Yes | Dependable if buffer not shared-writeable with other CXs |
| `reg2 rs1,rs2` ; `func4 rd,rs1,rs2` (state-passing pair) | Yes | Each instruction individually composable |
| `sort rs1,rs2` (sorts array in memory) | Yes | Dependable if array not shared with other CXs |
| `begin_async rs1` … `end_async rd,rs1,rs2` | Yes | Computation proceeds during the interval but is only *observable* at `end_async` |
| `watch rs1,rs2` (trap whenever `X[rs1] == X[rs2]`) | No | Behavior extends beyond execution of the instruction |
| `stream-register` (PULP-like stream semantic registers) | No | Behavior extends beyond execution of the instruction |
| `loop rs1,imm` (repeat next `imm` instructions) | No | PC access is not composable state; not one-instruction-at-a-time |

---

## CX API (`api`)

### Types and Constants

* `uuid_t` — 128-bit UUID (RFC 9562); canonical and immutable identifier for a CX.
* `cx_sel_t` — XLEN-sized CX selector type.
  * `cx_sel_invalid = ~0UL` (all ones)
  * `cx_sel_builtin = 0UL`
* `cx_open_flags_t`:
  * `cx_open_shared = 0`
  * `cx_open_exclusive = 1`
  * `cx_open_ucontext = 2` (probationary; OS/runtime-specific)

### CX State Isolation Models

* **Stateless**: no additional state; CX instructions access only pre-existing hart state.
* **Shared state**: zero or one instance per hart; all CX libraries for this CX share the instance per a CX-defined sharing model (e.g., caller-save, callee-save, always-initialize).
* **Exclusive state**: zero or more instances per hart; the CX library holds sole ownership of its instance and need not save/restore state across invocations.

The thread's current CX selection is part of user thread state and must be preserved in user thread context objects (`ucontext_t`, `jmp_buf`).

### Functions

#### `cx_sel_t cx_open(const uuid_t* puuid, cx_open_flags_t flags)`

* Acquires a CX instance with UUID `*puuid` per `flags`; returns its CX selector.
* Returns `cx_sel_invalid` and sets `errno` on error:
  * `EACCESS` — permission denied
  * `EBUSY` — instance in use
  * `EFAULT` — `puuid` inaccessible
  * `EINVAL` — invalid `flags`
  * `ENODEV` — no CX with this UUID on this system
* Multiple shared opens on the same thread always return the same selector value.
* Multiple exclusive opens always return different selector values.
* No guarantee that reopening after close succeeds or returns the same selector value.
* CX instances are not shared across threads; selector values are scoped to the thread that called `cx_open`.
* Opening on one thread does not open on other threads; does not guarantee the CX can be opened on other threads.
* First open: instance initialized per CX specification. Subsequent shared opens of the same instance: not re-initialized.
* `cx_open_ucontext` (probationary): signals that this instance should participate in user thread context save/restore.

#### `int cx_close(cx_sel_t sel)`

* Releases one reference to a CX instance; returns 0 on success, -1 on error.
* `EBADF` — `sel` is not a valid CX selector.
* On last close: selector becomes invalid and instance resources are released.
* Must be called on the same thread as the corresponding `cx_open`.
* A shared instance opened N times on a thread must be closed N times.

#### `bool cx_valid(cx_sel_t sel)`

* Returns `true` iff `sel` indicates a valid CX instance on this thread.
* Returns `false` for an invalid, built-in (`cx_sel_builtin`), or stale (closed) selector.

#### `cx_sel_t cx_get_sel(void)`

* Returns the thread's current CX selector value.
* Each new thread starts with `cx_sel_builtin`.

#### `cx_sel_t cx_select(cx_sel_t sel)`

* Sets the thread's current CX selection to `sel`; returns the prior CX selection.
* Valid CX selector: custom instructions executed by that CX instance.
* `cx_sel_builtin`: custom instructions and custom CSR accesses are performed using the built-in custom extension.
* Invalid selector (including `cx_sel_invalid`): custom instructions and custom CSR accesses have undefined behavior; in some environments this is defined to signal an error condition.

#### `ssize_t cx_save(void* pv, size_t size)`

* `pv == NULL`: returns the number of bytes needed to save the current CX instance state (may be 0).
* `pv != NULL`: saves up to `size` bytes of current CX instance state at `*pv`; returns bytes written.
* Returns 0 if current selection is `cx_sel_builtin`.
* Returns -1 and sets `errno` on error:
  * `EBADF` — current selection is invalid
  * `EFAULT` — `pv` inaccessible

#### `ssize_t cx_restore(void* pv, size_t size)`

* `pv == NULL`: returns 0.
* `pv != NULL`: restores CX instance state from `*pv` (`size` bytes); returns bytes read (≤ `size`).
* Returns 0 if current selection is `cx_sel_builtin`.
* Returns -1 and sets `errno` on error:
  * `EBADF` — current selection is invalid
  * `EFAULT` — `pv` inaccessible

### Calling Conventions

Two calling conventions are defined:

1. **Default (legacy CC)**:
   * On entry to callee: current CX selection is `cx_sel_builtin` (caller ensures).
   * On return from callee: current CX selection is `cx_sel_builtin` (callee ensures).
   * Shared CX instance state is **not** preserved across calls (caller-saved).

2. **CX calling convention** (`riscv_cx_cc` attribute):
   * Callee preserves the caller's current CX selection (callee-saved).
   * Shared CX instance state is preserved across calls (callee-saved).

Each new thread's initial CX selection is `cx_sel_builtin`.

### CX Library Examples (non-normative)

Quoted verbatim — four worked examples of a dot-product CX library (built on a multiply-accumulate CX) covering the state-isolation-model × calling-convention matrix. These illustrate exactly the save/restore and selection discipline the runtime and tests will implement. All four assume:

```c
#define CX_CALL __attribute__((riscv_cx_cc))

// external legacy function
int legacy_func(int);
// external CX-aware function
int cx_cc_func(int) CX_CALL;

inline static int mac_reset() CX_CALL {
//  return CUSTOM0_R(".insn r 0x0B, 0, 0", 0, 0);
    return CUSTOM0_R("mac_reset", 0, 0);
}
inline static int mac_mac(int a, int b) CX_CALL {
//  return CUSTOM_R(".insn r 0x0B, 0, 1", a, b);
    return CUSTOM0_R("mac_mac", a, b);
}
```

**1. Exclusive state — CX CC — no CX instance save/restore:**

```c
int dotp(cx_sel_t mac_sel, int as[], int bs[], unsigned n) CX_CALL {
    if (cx_valid(mac_sel)) {
        cx_sel_t prev = cx_select(mac_sel);

        int ret = mac_reset();
        for (int i = 0; i < n; ++i)
            ret = mac_mac(func_cx_cc(as[i]), bs[i]);

        cx_select(prev);
        return ret;
    }
    else
        return dotp_sw(as, bs, n);
}
```

**2. Exclusive state — legacy CC — no CX instance save/restore.** "Here `dotp` must set the current selection to `cx_sel_builtin` prior to calling a legacy function, then restore `mac_sel`, prior to issuing further CX custom instructions `mac_mac`."

```c
int dotp(cx_sel_t mac_sel, int as[], int bs[], unsigned n) {
    if (cx_valid(mac_sel)) {
        cx_sel_t prev = cx_select(mac_sel);
        int ret = mac_reset();

        for (int i = 0; i < n; ++i) {
            cx_select(cx_sel_builtin);
            int func_a_i = legacy_func(as[i]);
            cx_select(mac_sel);

            ret = mac_mac(func_a_i, bs[i]);
        }

        cx_select(prev);
        return ret;
    }
    else
        return dotp_sw(as, bs, n);
}
```

**3. Shared state — CX CC — CX instance save/restore.** "Here this *shared state* CX `dotp` saves and restores *caller's* CX instance state using the *CX-agnostic* `cx_save` and `cx_restore` APIs." The spec notes an alternative implementation might employ CX-specific save/restore code (e.g., optimized to save only live registers across calls).

```c
int dotp(cx_sel_t mac_sel, int as[], int bs[], unsigned n) CX_CALL {
    if (cx_valid(mac_sel)) {
        cx_sel_t prev = cx_select(mac_sel);

        // save caller's MAC CX instance state
        size_t size = cx_save(0, 0);
        void* pv = alloca(size);
        cx_save(pv, size);

        // reset the state, perform the dot product
        int ret = mac_reset();
        for (int i = 0; i < n; ++i)
            ret = mac_mac(func_cx_cc(as[i]), bs[i]);

        // restore callee's CX state
        cx_restore(pv, size);

        cx_select(prev);
        return ret;
    }
    else
        return dotp_sw(as, bs, n);
}
```

**4. Shared state — legacy CC — CX instance save/restore.** "Here this shared state CX `dotp` saves and restores *its own* MAC CX instance state upon every call to an external legacy function. It also selects `cx_sel_builtin` prior to each such call."

```c
int dotp(cx_sel_t mac_sel, int as[], int bs[], unsigned n) {
    if (cx_valid(mac_sel)) {
        cx_sel_t prev = cx_select(mac_sel);
        size_t size = cx_save(0, 0);
        void* pv = alloca(size);

        int ret = mac_reset();
        for (int i = 0; i < n; ++i) {
            cx_save(pv, size);
            cx_select(cx_sel_builtin);
            int func_a_i = legacy_func(as[i]);
            cx_select(mac_sel);
            cx_restore(pv, size);

            ret = mac_mac(func_a_i, bs[i]);
        }

        cx_select(prev);
        return ret;
    }
    else
        return dotp_sw(as, bs, n);
}
```

Implementation-relevant details visible in the code:

* Every variant checks `cx_valid(mac_sel)` and falls back to a pure-software `dotp_sw` when the selector is invalid — the canonical fallback pattern.
* `cx_save(0, 0)` obtains the save size (the `pv == NULL` form of `cx_save`).
* The commented-out `.insn r 0x0B, ...` lines show the custom-0 opcode encodings behind the `mac_reset`/`mac_mac` intrinsics.
* Examples 1 and 3 call `func_cx_cc(...)` while the assume-block declares `cx_cc_func` — these appear to be the same function (minor naming drift in the example).

### Versioning and UUIDs

* A CX UUID (RFC 9562) is the immutable, canonical name of a CX; it completely specifies its instructions, CSRs, state, and behavior.
* Any change to a CX — adding, removing, or altering any state or behavior — constitutes a new CX and requires a new UUID.
* CX library binaries embed the UUID(s) of their required CX(s) and need not be recompiled for different systems.
* **Version negotiation**: a CX library requests its preferred UUID; if unavailable, falls back to an alternative version, then to a software fallback. No "recompile everything" recovery is assumed.
* Semantic versioning is proposed but not adopted.
* A CX subset (proper subset of a CX's instructions or state) should be treated as a new, distinct CX with its own UUID (non-normative recommendation in the spec).
* CX instances are not shared across threads; two threads opening the same CX may receive different selector values, and two threads opening different CXs may receive identical selector values.

### Spec TODO List (non-normative)

Subject to change — items the TG flags as "revisit": `cx_uuid` vs. `cxid`; `cxinfo`; `cxsaveall`/`cxrestoreall`; `RISCV_CX_INFO_STATE_SIZE` vs. `cx_save(0,0)` size; `hwprobe` keys; `cxenable`/`cxdisable`; shared and exclusive CX models atop an OS supporting only one; system topology / VMs / hotplug / partial reconfiguration / revocation.

---

## External Specifications (`external`, Appendix A)

The platform-specific discovery mechanisms and OS interfaces referenced by the ISA and API chapters:

* CX UUID → `cx_sel_t` mapping (used by `cx_open`) — via the Linux `prctl` interface below.
* Size of the CX state context data for a given CX (bounds valid `cxsidx` values) — via devicetree `state-size` / hwprobe `CONTEXT_SIZE` keys.
* Whether `scxdiscard` is supported for a given CX — via devicetree `discard`.
* State context size may vary across systems but is constant during execution on a given system.

### Devicetree Binding (`cxs.yaml`)

* Describes the CXs available on a CPU: one `cx@<CXID>` node per CX under a `cxs` container node.
* Properties:

  | Property | Type | Required | Description |
  |---|---|---|---|
  | `reg` | — (minimum 1) | no | The CXID of this CX node |
  | `uuid` | 16-byte array | **yes** (only required property) | The UUID of this CX |
  | `vendorid` | u64 | no | Vendor ID |
  | `archid` | u64 | no | Architecture ID |
  | `impid` | u64 | no | Implementation ID |
  | `state-size` | u32, default 0 | no | Size in bytes of one state context |
  | `state-count` | u32, default 1, min 1 | no | Total number of state contexts provided by this CX on this CPU |
  | `discard` | boolean | no | Indicates support for the discard operation used by the `scxdiscard` instruction |

* `reg` has minimum 1 — the built-in extension (CXID 0) is not described in the devicetree.

### User Space ABI (psABI)

* New ELF `e_flags` bit `EF_RISCV_RVCX`: set when the binary is compiled with the CX-aware calling convention. Linker policy: report errors when linking object files with different values for the CX field.
* **Default calling convention** (ABI wording): custom extension state is not preserved across function calls; `cxsel` is not preserved across function calls; procedures may assume `cxsel` is zero upon entry and zero upon return from a procedure call. Software that sets `cxsel` to a non-zero value must set it to zero before returning or calling another procedure.
* **CX calling convention** (`riscv_cx_cc` attribute): custom extension state is preserved across function calls; `cxsel` is preserved across function calls; procedures may assume `cxsel` is zero upon entry. Software that sets `cxsel` to a non-zero value must set it to zero before calling another procedure.
  * Note: it is unclear how this "zero upon entry" assumption combines with the API chapter's "callee preserves the caller's current selection (callee saved)" — see Discussion.

### Linux

The user-space interface for CXs in Linux; supports a single shared context per thread.

**hwprobe** — new keys for the `hwprobe()` syscall:

* `RISCV_HWPROBE_KEY_CXID_0` — bitmask of CX IDs available to user space (bit 0 = CX ID 0, etc.).
* `RISCV_HWPROBE_KEY_CX1_CONTEXT_SIZE` … `RISCV_HWPROBE_KEY_CX63_CONTEXT_SIZE` — size in bytes of one state context for the specified CX.
* `RISCV_HWPROBE_KEY_CX1_VENDORID` … `CX63_VENDORID` — vendor ID for the specified CX.
* `RISCV_HWPROBE_KEY_CX1_ARCHID` … `CX63_ARCHID` — architecture ID for the specified CX.
* `RISCV_HWPROBE_KEY_CX1_IMPID` … `CX63_IMPID` — implementation ID for the specified CX.
* `RISCV_HWPROBE_KEY_IMA_EXT_0` gains `RISCV_HWPROBE_EXT_CX` — the CX framework (not a specific CX) is supported.
* Note: hwprobe exposes no UUIDs, and the per-CX keys start at CX1 (no keys for the built-in CXID 0).

**prctl** (`Documentation/arch/riscv/cx.rst`) — new processes start with all custom extensions disabled:

* `prctl(PR_RISCV_CX_QUERY, unsigned long *cxid, uuid_t *uuid, unsigned int flags)` — query the CX UUID for a given `cxid`; with `flags = RISCV_CX_QUERY_CXID`, returns the `cxid` for a given `uuid`.
* `prctl(PR_RISCV_CX_ENABLE, unsigned long cxid, unsigned long *sel, unsigned int flags)` — request enabling the CX specified by `cxid` for the current process; on success returns in `sel` a CX selector usable to select the CX. A CX may be enabled multiple times; each enable must be matched with a corresponding disable before the extension is disabled. `flags` is currently unused and must be zero.
  * Note: the spec text says the selector "may be used by the `cxsel` instruction"; the ISA chapter's context-switch example likewise uses a `cxsel` mnemonic, so this may be earlier naming for `cxsetsel`. See Discussion.
* `prctl(PR_RISCV_CX_DISABLE, unsigned long sel)` — request disabling the CX specified by `sel`, the selector previously returned by a corresponding enable call.

**System call behavior**: CX framework state and custom extension state are preserved across system calls.

---

## Unprivileged Architecture Models (`unprivarch`, Appendix C — non-normative)

The spec presents two candidate architecture models "as a way to evaluate the suitability of decisions in other areas of the specification" and explicitly states it is **yet to be determined** which model(s) the specification will support. Of the four combinations of {one, multiple} state contexts × {included in, excluded from} hart state, these are the two commonly advocated positions:

* **Single Hart State Context**:
  * When enabled, a CX provides a given software thread exactly one instance of CX state (if any).
  * CX state is treated like other hart state (integer/floating-point registers): included in `ucontext_t`, handled by `makecontext` et al. and by `setjmp` et al.
* **Multiple Non-Hart State Contexts**:
  * A CX provides a given software thread an independent, isolated copy of its state for **each** request to enable the extension.
  * CX state is treated as unique and separate from other hart state.

---

## Empty / Placeholder Spec Sections

Nothing to capture from these (listed so future spec-vs-doc comparisons don't re-flag them):

* **Introduction** (`intro`) — lorem ipsum placeholder text only.
* **Logic Interface** (`li`) — stub; a TIP to "specify a logic signal level interface for composable custom extensions."
* **Guidance** (`guidance`, Appendix B) — empty Software/Hardware Recommendations headings; marked wholly non-normative.
* **Appendix A stubs** — ACPI (table definition TBD), User Space API (covered by the CX API chapter), SBI (unclear if an SBI extension is necessary).
* **`cxsel-format.adoc`** — exists in the spec `src/` directory but is not included by any chapter, so it is absent from the built document — see Discussion.

---

## Tests

### `cxsel` / `cxsetsel`

* Read from `cxsel` at reset MUST return 0.
* Write to `cxsel` via a Zicsr instruction MUST raise an illegal instruction exception (URO behavior).
  * Note: the spec also describes `cxsel` as WARL — see the URO vs. WARL open question for how the two are read together for this project.
* `cxsetsel rd,rs1` MUST atomically swap `cxsel` with `x[rs1]`, writing the prior `cxsel` value to `x[rd]` when `rd ≠ x0`.
* `cxsetsel` with `rd = x0` MUST still write `x[rs1]` to `cxsel` and discard the old value.

### `cxsidx` / `cxsdata`

* `cxsidx` MUST auto-increment by 1 following each `cxsdata` access.
* All CSR access variants (`csrr`, `csrw`, `csrrw`, `csrrs`, `csrrc`) MUST correctly read/write the indexed word and auto-increment `cxsidx`.

---

## Discussion

* **§cxsel — URO vs. WARL**: `cxsel` is URO (writes via Zicsr raise illegal instruction) but is also described as WARL.
  * status: Resolved — `cxsetsel` is the only way to update `cxsel`, so URO is correct; WARL describes the value-legalization scheme on the `cxsetsel` write path. Implementation: [ADR-027](../docs/adr/027.md).

* **§cxsetsel — illegal instruction conditions**: Under what conditions, if any, is `cxsetsel` itself an illegal instruction (candidates: `scxstp.mode` = Disabled, `sstateen0.C` = 0)?
  * status: Resolved — both apply. `scxstp.mode` = Disabled: Action Plan Block 2.2. `sstateen0.C`/`mstateen0.C` = 0: Action Plan Block 7.1, which lists `cxsetsel` explicitly among the gated CSRs/instructions.

* **§cxsel — "built-in custom extension" used before it is defined in spec**: `cxsel = 0` selects the "built-in custom extension" before the term is introduced. A definition prior to first use might help readers.
  * status: Resolved for this project — spec may adjust if appropriate.

* **§cxsetsel — Does "CX instructions treated as illegal" include `cxsetsel`?**: When `cxsel` is invalid, "CX instructions are treated as illegal instructions." It is not explicit whether `cxsetsel` — a CX framework instruction, not a CX-dispatched instruction — is subject to this restriction.
  * status: Resolved by reading — `cxsetsel` is the only way to change `cxsel` back to a valid value, so it presumably is not included. Depending on intent, the wording may just need to change to "custom operations" (a defined term), or "CX instructions" could be added to the definitions.

* **§cxsidx repeated text**: "The read-write WARL XLEN-wide CX >>>>state index>>>>> CSR" — appears to be accidentally duplicated text in the spec source (`isa-state.adoc`).
  * status: Resolved — looks like a small editing artifact; worth mentioning upstream.

* **CX CSR interrupt atomicity**: Can CX CSR operations (particularly `cxsdata` read-modify-write plus `cxsidx` auto-increment) be interrupted mid-execution with visible partial state? Does the ISA define restart semantics?
  * status: Resolved by reading — CSR instructions are single hardware operations; the read-modify-write and `cxsidx` auto-increment are taken to be atomic as a unit, following from the base ISA, so no restart semantics appear to be needed.

* **`cxsidx` out-of-bounds behavior and the state-size bound**: the spec leaves `cxsdata` access with invalid `cxsidx` and `cxsidx` past the last word as explicitly undefinded, and `cxsidx`'s own legal upper bound as unsettled.
  * status: Ambiguous — spec leaves this to the implementation. Implementation: [ADR-021](../docs/adr/021.md).

* **`cxsdata` auto-increment**: should there be a non-incrementing version of `cxsdata`?
  * status: Proposed, tabled. Implementation: [ADR-022](../docs/adr/022.md).

* **HARTs with heterogeneous CX support**: how does `cx_open` operate on a hart that lacks the requested CX's hardware?
  * status: Tabled, out of scope for now. Implementation: [ADR-023](../docs/adr/023.md).

* **`cxsetsel` atomicity scope**: The spec says `cxsetsel` "atomically swaps" `cxsel` and a register. Atomic with respect to what — interrupts on the same hart, context switches, memory visibility across harts? RISC-V typically reserves "atomic" language for AMO instructions, so the intended scope is not obvious here.
  * status: Resolved by reading — atomicity is taken to be instruction-scoped (single instruction, not interruptible mid-execution on the executing hart), with no cross-hart memory visibility implied. An explicit statement in the spec would confirm this.

* **`cxsdata` when `cxsel = 0`**: the spec defines `cxsdata` access as undefined when `cxsel` is 0 — the built-in extension (CX ID 0) has no state context accessible via `cxsidx`/`cxsdata`.
  * status: Ambiguous — spec explicitly doesn't support this (reserved), actual behaviour is left to the implementation. Implementation: [ADR-024](../docs/adr/024.md). A related idea (co-opting reserved `scxxs[1:0]` bits for builtin-extension status) is tracked separately: [ADR-025](../docs/adr/025.md).

* **`scxstp` reset value**: the spec does not specify `scxstp`'s reset value.
  * status: Ambiguous — spec omits this detail (most likely unintentionally). Implementation: [ADR-026](../docs/adr/026.md).

* **Indirect mode `cxsel` bounds**: in Indirect mode the 4 KiB table holds 1024 32-bit entries; the spec does not state what happens when `cxsel >= 1024`.
  * status: Ambiguous — spec leaves this to the implementation. A register diagram for `cxsel` might help settle this (see the `cxsel-format.adoc` item below). Implementation: [ADR-027](../docs/adr/027.md).

* **`scxxs` state machine**: the spec says `scxxs` bits are "analogous to `FS`/`VS` bits in `mstatus`" but does not define the CX context state machine explicitly.
  * status: Resolved by analogy. Implementation: [ADR-028](../docs/adr/028.md), which also records a related spec clarification, [ADR-029](../docs/adr/029.md).

* **`scxNxs` as future indirect CSRs**: the spec states `scxNxs` registers "are expected to become indirect CSRs" and `Zcxmulti` "will require `Sscsrind`" — both in future tense.
  * status: Resolved by reading — treated as a current requirement regardless of tense. Implementation: [ADR-030](../docs/adr/030.md).

* **`cxdiscard` vs. `scxdiscard`**: `isa-state.adoc` defines `cxdiscard` (unprivileged) as informing the runtime that state need not be saved — it does **not** write state, only sets XS to `Initial`. `isa-priv.adoc` defines `scxdiscard` (privileged) as **overwriting** the context (state becomes UNSPECIFIED), then setting XS to `Initial`. These read as two distinct operations with different semantics.
  * status: Open — TG discussion ongoing. See [ADR-017](../docs/adr/017.md), which also covers CX initialization procedure and data-leakage considerations.

* **Smstateen default behavior for CX CSR access**: the spec states `mstateen0.C` must be set to enable CX custom state access but does not specify the behavior when it is not set.
  * status: Resolved by assumption — standard Smstateen behavior applies (accesses raise illegal instruction when the bit is clear); matches Action Plan Block 7.1.

* **`scxstp.mode = 2` when `Zcxmulti` is absent**: writing mode = 2 (Indirect) to `scxstp` when `ext_zcxmulti` is not enabled: behavior TBD.
  * status: Resolved by WARL. Implementation: [ADR-004](../docs/adr/004.md).

* **CX thread-scoped selectors and OS context switch**: `cx_sel_t` values are thread-scoped, but `cxsel` is a hart CSR; on a context switch `cxsel` must be saved/restored as part of the thread context — whose responsibility is that?
  * status: Resolved in part. OS saves selector. Implementation: [ADR-031](../docs/adr/031.md).

* **Exclusive vs. shared CX instances on an OS supporting only one model**: the spec notes (non-normatively): "TODO: detail how both shared and exclusive CX models live atop an OS that supports only one, or the other."
  * status: Open — unresolved in spec; impact on runtime implementation TBD.

* **§isa-priv — privileged CSR presence condition**: the spec states "The privileged CSRs and opcodes are present when `Zcxmulti` is present and the supervisor extension is present," while the CSR tables are explicitly split between `Zcx` (`scxstp`, `scxxs0`–`scxxs3`) and `Zcxmulti` (`scxNxs`). Read literally, the sentence would condition all of these on `Zcxmulti`, leaving base `Zcx` with no mode-control register.
  * status: Resolved by this reading — `scxstp`/`scxxs0`–`scxxs3` conditioned on `Zcx` + S; `scxNxs` conditioned on `Zcxmulti` + S; matches the Action Plan's Build Configuration Matrix (§4). Possibly a spec wording oversight; will raise with the TG to confirm.

* **Single `cxsel` and the four custom opcode spaces**: a proposal was made for multiple simultaneous selection registers (`cxsel0`/`cxsel1`/..., one per custom opcode space).
  * status: Tabled by the TG. Implementation: [ADR-041](../docs/adr/041.md).

* **State management as a separate extension**: is the state management functionality (`cxsidx`, `cxsdata`, `cxdiscard`) intended to be a separate, optional extension (e.g. `Zcxstate`), or always bundled with `Zcx`?
  * status: Open — spec description hints at it being a separate extension. Awaiting spec clarification.

* **Replacement for the R (ready/busy) flag**: the basis spec included an R flag to indicate whether a CX is busy, especially useful during hardware initialization. It is not present in the current spec.
  * status: Open — awaiting spec clarification, but appears intentionally dropped; use CX-specific CSRs per CX contract.

* **`cx_status` CSR or `get_status` opcode**: state status Dirty/Clean moved to `scxNxs`, `state_size` moved to platform-specific discovery — the `cx_status` CSR and `get_status` instruction appear intentionally superseded, but `Requirements.md` still lists "collect accumulated errors..." as settled.
  * status: Open — reading any CX-specific status signaling as handled by individual CX contracts in CX CSRs. If the TG is removing the accumulator, `Requirements.md` should be corrected; worth raising with the TG.

* **`cxsel-format.adoc` — typed `cxsel` layout present in spec source but not in the built document**: `src/cxsel-format.adoc` defines a `cxsel` register layout — bit `XLEN-1` = `inv`, bits `XLEN-2:XLEN-4` = `type`, remaining bits = `sel` — but no chapter includes it, so it doesn't appear in the built document. Bears directly on the Indirect-mode bounds question above.
  * status: Open — clarification on whether the typed-selector format reflects current intent would be welcome.

* **§criteria — PC in the composable-state list**: normative criterion 2 includes PC in the composable-state list, while the non-normative rationale re-quotes the criterion *without* PC and rules the `loop` example non-composable because "PC access is not *composable state*." The two passages seem to differ.
  * status: Open — possibly an editing artifact; clarification would be welcome. Project assumption until clarified: PC is not writable composable state.

* **CX CC — callee-saved `cxsel` vs. zero-on-entry**: the API chapter's CX calling convention says the callee preserves the caller's current selection (callee saved); Appendix A's psABI CX CC says procedures may assume `cxsel` is zero on entry and that callers must zero a non-zero `cxsel` before calling. It's unclear how the two combine.
  * status: Open — awaiting spec clarification; possibly an inconsistency in Appendix A, or a subtlety not yet understood here.

* **Discovery path split across devicetree / hwprobe / prctl**: `cx_open` maps UUID → selector, but Appendix A splits discovery across devicetree, hwprobe, and `prctl` in a way that conflicts with how this project's Action Plan currently implements discovery.
  * status: Open — conflict with Action Plan Blocks 5.3/5.4 flagged. Implementation: [ADR-040](../docs/adr/040.md).

* **Which unprivileged architecture model (Appendix C)**: Single Hart State Context vs. Multiple Non-Hart State Contexts. The spec explicitly leaves undetermined which model(s) will be supported.
  * status: Open — spec explicitly undecided. Implementation: [ADR-032](../docs/adr/032.md).

* **§criteria — chapter still evolving (per its own notes)**: the chapter carries a "Requirements (temporary, will be deleted)" note and several NOTEs indicating the normative language is still being developed: memory ordering/partial completion/PMA types, exception constraints, whether "identical" functional behavior is too strong (it would deny e.g. a TRNG CX), and whether privileged-state access will be disallowed.
  * status: Open — track spec evolution before treating the criteria as fixed requirements.

* **§api Other TODOs**: the spec flags for revisiting: `cx_uuid` vs. `cxid`; `cxinfo`; `cxsaveall`/`cxrestoreall`; distinguishing `RISCV_CX_INFO_STATE_SIZE` vs. `cx_save(0,0)` size; `hwprobe` keys (Linux); `cxenable`/`cxdisable`.
  * status: Open — all unresolved in spec; awaiting TG.

* **"Valid"/"invalid" selector — several distinct senses**: the terms appear to be used in at least three senses across `isa-unpriv`, the API chapter (`cx_valid`/`cx_select`), and `isa-state`/`isa-priv`, which mostly work in context but don't quite align with each other.
  * status: Ambiguous. Implementation: [ADR-033](../docs/adr/033.md).

* **§cx_select repeated text**: "custom instructions and custom instructions have undefined behavior" — appears to be accidentally duplicated text.
  * status: Resolved — looks like a small editing artifact; worth mentioning upstream.

* **Indirect mode table entry — intended use of the `V` bit**: the spec states only that a translation-table entry with `V` clear causes custom operations to raise an illegal instruction exception (§2.2.3); it says nothing about how or why software would clear `V`, or what's expected afterward.
  * status: Open — non-normative, left to software. Eviction/lazy-switching use: [ADR-019](../docs/adr/019.md). Software-emulation-of-absent-CX use and a proposed `V`→`XE` rename: [ADR-034](../docs/adr/034.md).

* **`scx_invalidate(idx)` — possible explicit signaling instruction**: project idea, not raised with the TG.
  * status: Proposed. Implementation: [ADR-035](../docs/adr/035.md).

* **Number of contexts per hart**: `Requirements.md` offers two options for the number of contexts relative to the number of harts.
  * status: Open. Implementation: [ADR-036](../docs/adr/036.md).

* **A third `scxstp` mode — "Bare" (2026-08-14 TG meeting)**: proposed for M-mode-only lightweight embedded systems.
  * status: Open — may be its own `scxstp.mode`, or just describe a bare-bones Direct mode. Tracked: [ADR-042](../docs/adr/042.md). This project's machine-mode-only support commitment, regardless of how "Bare" resolves: [ADR-038](../docs/adr/038.md).

* **255 CXs vs. 64 `scxxs`/`scxNxs` status slots**: `Requirements.md` settles "at least 255 CXs," matching the 8-bit CXID field in Direct mode, but `scxxs0`–`scxxs3` plus `scxNxs1`–`scxNxs63` together give 2-bit status for only 64 CX IDs.
  * status: Open — verified, not assumed; worth raising with the TG.

* **Hypervisor support**: `Requirements.md` lists "Support the hypervisor extension", but the draft spec text defines no VS-level selection translation, no hypervisor-level mapping of a guest's CXID/IDX, and no VS-mode behavior for the CX CSRs; the only references are `hstateen0.C` gating and `vsstatus` in the composable-state list.
  * status: Open — possibly covered in material not reflected here; clarification on hypervisor intent would be welcome. Tracked: [ADR-047](../docs/adr/047.md).

* **Spec-impacting ADRs and proposals**: see `runtime-cxtg/docs/ADR_log.md`'s `Spec` column: any entry marked `X`, in any of its tables, is a decision or proposal that could still require a change to the actual TG spec text. The `X` clears once the entry no longer would change the spec, or once the spec — and this document — already reflects it (`runtime-cxtg/docs/adr/_schema.md` § Spec-impact flag).
