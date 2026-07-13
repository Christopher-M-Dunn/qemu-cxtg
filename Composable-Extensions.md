# Requirements

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
  * Note: the spec text literally says "The privileged CSRs and opcodes are present when `Zcxmulti` is present and the supervisor extension is present," which would incorrectly require `Zcxmulti` for `scxstp`/`scxxs0`–`scxxs3` — see Open Questions.
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

### Versioning and UUIDs

* A CX UUID (RFC 9562) is the immutable, canonical name of a CX; it completely specifies its instructions, CSRs, state, and behavior.
* Any change to a CX — adding, removing, or altering any state or behavior — constitutes a new CX and requires a new UUID.
* CX library binaries embed the UUID(s) of their required CX(s) and need not be recompiled for different systems.
* **Version negotiation**: a CX library requests its preferred UUID; if unavailable, falls back to an alternative version, then to a software fallback. No "recompile everything" recovery is assumed.
* Semantic versioning is proposed but not adopted.
* A CX subset (proper subset of a CX's instructions or state) should be treated as a new, distinct CX with its own UUID (non-normative recommendation in the spec).
* CX instances are not shared across threads; two threads opening the same CX may receive different selector values, and two threads opening different CXs may receive identical selector values.

---

## Platform-Specific Discovery

* A platform-specific discovery mechanism must provide:
  * CX UUID → `cx_sel_t` mapping (used by `cx_open`).
  * Size in words of the CX state context data for a given CX (bounds valid `cxsidx` values).
  * Whether `scxdiscard` is supported for a given CX.
* State context size may vary across systems but is constant during execution on a given system.

---

## Tests

### `cxsel` / `cxsetsel`

* Read from `cxsel` at reset MUST return 0.
* Write to `cxsel` via a Zicsr instruction MUST raise an illegal instruction exception (URO behavior).
  * Note: the spec also describes `cxsel` as WARL, which is in tension with URO — see Open Questions.
* `cxsetsel rd,rs1` MUST atomically swap `cxsel` with `x[rs1]`, writing the prior `cxsel` value to `x[rd]` when `rd ≠ x0`.
* `cxsetsel` with `rd = x0` MUST still write `x[rs1]` to `cxsel` and discard the old value.

### `cxsidx` / `cxsdata`

* `cxsidx` MUST auto-increment by 1 following each `cxsdata` access.
* All CSR access variants (`csrr`, `csrw`, `csrrw`, `csrrs`, `csrrc`) MUST correctly read/write the indexed word and auto-increment `cxsidx`.

---

## Open Questions

* **§cxsel — URO vs. WARL contradiction**: `cxsel` is URO (writes via Zicsr raise illegal instruction) but also WARL. The spec states `cxsel` "can only be updated by the `cxsetsel` instruction," which implies WARL describes the legal value range, not write accessibility via Zicsr. This reading is consistent with URO, but the spec text does not make this explicit and should be clarified.
  * status: Resolved by reading — WARL describes value-range behavior on the cxsetsel write path, not Zicsr write accessibility; URO and WARL are compatible under this interpretation. Spec should probably still be updated to make this clearer.

* **§cxsel — Built-in custom extension undefined before use**: `cxsel = 0` selects the "built-in custom extension" before the term is defined. The spec should define what a built-in custom extension is prior to first use.
  * status: Resolved — spec should define "built-in custom extension" prior to first use of the term.

* **§cxsetsel — Does "CX instructions treated as illegal" include `cxsetsel`?**: When `cxsel` is invalid, "CX instructions are treated as illegal instructions." It is not explicit whether `cxsetsel` — a CX framework instruction, not a CX-dispatched instruction — is subject to this restriction. Spec clarification needed.
  * status: Resolved by reading — cxsetsel is the instruction that must be executed in order to change cxsel to a valid value, and is the only way to do so, so it obviously must not be included; spec should make this distinction explicit.

* **§cxsidx typo**: "The read-write WARL XLEN-wide CX >>>>state index>>>>> CSR" — erroneous repeated text in the spec source (`isa-state.adoc`).
  * status: Resolved — fix typo in spec source (isa-state.adoc)

* **CX CSR interrupt atomicity**: Can CX CSR operations (particularly `cxsdata` read-modify-write plus `cxsidx` auto-increment) be interrupted mid-execution with visible partial state? Does the ISA define restart semantics?
  * status: Resolved by reading — CSR instructions are single hardware operations; the read-modify-write and cxsidx auto-increment are atomic as a unit; no restart semantics needed.

* **`cxsidx` out-of-bounds behavior**: The spec says `cxsdata` access with invalid `cxsidx` is "undefined," and `cxsidx` after the last word is "undefined." Implementation must choose one:
  * (a) Trap (`ILLEGAL_INST`) on any `cxsdata` access where `cxsidx >= state_size` — enforced in predicates once `state_size` is wired up (Phase 4).
  * (b) Clamp: `cxsidx` stops incrementing at `state_size-1`; caller detects end-of-state by observing no further advance.
  * (c) Silent wrap / undefined — no enforcement.
  Option (b) is self-signalling with no trap overhead. Option (a) catches bugs earlier. Deferred to Block 4.2.
  * status: Resolved by spec intent — spec leaves out-of-bounds behavior to software; QEMU implementation is (c) undefined/no enforcement, with possible internal guard to be determined in Block 4.2.

* **`cxsdata` auto-increment naming**: Should the current auto-incrementing CSR be renamed `cxsdatai` and a new non-incrementing `cxsdata` be added?
  * Current: `cxsdata` = access-and-increment (sequential spill/fill).
  * Proposed: `cxsdatai` = access-and-increment; `cxsdata` = plain access (no `cxsidx` side effect).
  * status: Tabled.

* **HARTs with heterogeneous CX support**: How does `cx_open()` operate on a hart that lacks CX hardware? Options include `sched_setaffinity()` to pin to CX-capable harts, or `riscv_hwprobe()` (Linux 6.4) for per-core extension detection. Whether this is OS-specific is unresolved.
  * status: Tabled — out of scope for this project. If project completes early, it can be revisited.

* **`cxsetsel` atomicity scope**: The spec says `cxsetsel` "atomically swaps" `cxsel` and a register. Atomic with respect to what — interrupts on the same hart, context switches, memory visibility across harts? RISC-V normally defines atomicity only for AMO instructions; the scope here needs spec clarification.
  * status: Resolved by reading — atomicity is instruction-scoped (same as a load immediate); single-instruction, non-interruptible on the executing hart; no cross-hart memory visibility implied. Spec should clarify this explicitly.

* **`cxsdata` when `cxsel = 0`**: The spec defines `cxsdata` access as undefined when `cxsel` is 0. This implies the built-in extension (CX ID 0) has no state context accessible via `cxsidx`/`cxsdata`, and that `cx_save`/`cx_restore` with `cx_sel_builtin` always return 0 bytes. Confirm this is intended.
  * status: Open — whether cx-aware software should be able to co-opt the CX infrastructure for built-in custom instructions is unresolved. `cxsidx`/`cxsdata` and a hypothetical `scx0xs0`/`scx0xs1` are unlikely to be made available. Bits `[1:0]` of `scxxs0` are currently reserved; options are to designate them for custom use or keep reserved for future use. Likely out of scope for this project.

* **`scxstp` reset value**: The spec does not specify the reset value of `scxstp`. If reset to 0 (Disabled), all CX use traps immediately from boot. If reset to 1 (Direct), CX is available immediately. Which is intended?
  * status: Open — TG intent is that built-in instructions are immediately available at reset, but since `cxsel` resets to 0 (`cx_sel_builtin`), this requirement is satisfied regardless of `scxstp.mode` (0, 1, or 2 with a properly initialized table). Project assumption: reset value = 0 (Disabled).

* **Indirect mode `cxsel` bounds**: In indirect mode the 4 KiB table holds 1024 32-bit entries. The spec does not state what happens when `cxsel >= 1024`. Illegal instruction exception, or `cxsel` masked to table size?
  * status: Open — RISC-V priv spec §2.3.3 does not distinguish valid vs. legal values, leaving this largely implementation-defined; only hard requirement is that `cxsel` can hold all valid values and any value the discovery mechanism may return. Ambiguity extends to whether an implementation must hold indices into table entries with V=0. `cxsel` also lacks a register diagram and needs more spec clarification. Project assumption: all custom instructions/CSR accesses with index < 0 or > 1023 raise illegal instruction. Mechanism deferred; candidates are: clamp to index 1023 (reserved, always holds invalid selector −1), trap on table lookup without clamping, or clamp `cxsel` to −1/sentinel/MSB-flip. Leaning toward no clamping on `cxsel` (or clamp to −1) plus writing −1 to `mcx_selector` (or whatever the internal de-translated selector will be called) on out-of-bounds lookup during `cxsetsel`; alternatively, perform the table lookup on each CX instruction dispatch with no detranslated state stored. Final approach depends on indirect mode implementation in Phase 6.

* **`scxxs` state machine**: The spec says `scxxs` bits are "analogous to `FS`/`VS` bits in `mstatus`" but does not define the CX context state machine explicitly. Is the full four-state model (Off / Initial / Clean / Dirty) adopted? What events drive each transition?
  * status: Resolved by analogy — full four-state model (Off/Initial/Clean/Dirty) adopted per priv spec §3.1.6.7 Table 12; transitions map directly: Off traps on CX instructions; Initial/Clean→Dirty on state-modifying CX instructions; Dirty→Clean on context save; any non-Off→Initial on cxdiscard/scxdiscard; explicit scxxs write drives Off/enable transitions. Without Zcxmulti, scxxs is writable (like FS/VS); with Zcxmulti, scxxs is a read-only summary (like XS) and scxNxs holds per-context status. Stateless CXs use only Off and Initial (no Clean or Dirty, as they have no state to save). Spec should reproduce the state-transition table explicitly rather than relying on "analogous."

* **`scxNxs` as future indirect CSRs**: The spec states `scxNxs` registers "are expected to become indirect CSRs" and `Zcxmulti` "will require `Sscsrind`" — both in future tense. The requirements section treats `Sscsrind` as a current requirement, but the spec has not yet made it normative. However, Adding 128 direct CSRs is probably a non-starter, and no other alternative is proposed, so `Sscrind` is required to implement the design as it is, and phase 6 will treat it as so. The TG might want to tighten up the wording here.
  * status: Open — spec wording is future tense but Sscsrind is treated as a current requirement for this project; phase 6 will implement scxNxs as indirect CSRs via Sscsrind. TG should normatively require Sscsrind for Zcxmulti.

* **`cxdiscard` vs. `scxdiscard` semantic split**: `isa-state.adoc` defines `cxdiscard` (unprivileged) as informing the runtime that state need not be saved — it does **not** write state, only sets XS to `Initial`. `isa-priv.adoc` defines `scxdiscard` (privileged) as **overwriting** the context (state becomes UNSPECIFIED), then setting XS to `Initial`. These appear to be two distinct instructions with different semantics. Confirm both are present in the final spec and that the semantic distinction is intentional.
  * status: Open — it is unclear whether cxdiscard and scxdiscard are ISA opcodes, CSR operations, or some combination; and whether 0, 1, or 2 encodings are needed to cover the unprivileged hint and privileged overwrite semantics. Awaiting spec clarification.

* **Smstateen default behavior for CX CSR access**: The spec states `mstateen0.C` must be set to enable CX custom state access but does not specify the behavior when it is not set. Per the Smstateen spec, accesses with the bit clear raise illegal instruction. Confirm this is the expected behavior for CX CSR accesses when `mstateen0.C = 0`.
  * status: Resolved by assumption — standard Smstateen behavior applies; CX CSR accesses raise illegal instruction when mstateen0.C = 0.

* **`cxsidx` WARL width and save-size probing**: Since `cxsidx` width is implementation-defined (WARL), software must obtain state context size via the platform-specific discovery mechanism, not by probing the `cxsidx` range. Confirm this is the canonical model for sizing save/restore buffers.
  * status: Resolved — canonical model is `cx_save(NULL)`, which returns the number of bytes needed to save the current CX instance state (0 for stateless); the API implementation obtains size via the platform-specific discovery mechanism. Project will use hwprobe / device tree for discovery. Probing `cxsidx` range is not the model.

* **`cx_select` implementation path**: `cx_select(sel)` must use `cxsetsel` since `cxsel` is URO and cannot be written via Zicsr. The API spec does not state this explicitly. Confirm.
  * status: Resolved — confirmed; cx_select() is implemented via cxsetsel.

* **`scxstp.mode = 2` when `Zcxmulti` is absent**: Writing mode = 2 (Indirect) to `scxstp` when `ext_zcxmulti` is not enabled: behavior TBD. Options: WARL-clamp on write, trap on write, or silently stored but trap on first CX instruction. Awaiting TG clarification. Deferred to Block 6.3.
  * status: Resolved by WARL — spec table lists mode 2 as "Zcxmulti only"; since scxstp is WARL, writing an invalid mode is a WARL write. Project implementation: clamp to 1 (Direct) by ANDing the 4-bit mode field with 0b0001).

* **CX thread-scoped selectors and OS context switch**: `cx_sel_t` values are thread-scoped, but `cxsel` is a hart CSR. On a context switch, `cxsel` must be saved/restored as part of the thread context. Is this the responsibility of the OS kernel or the CX runtime? Unresolved.
  * status: Resolved in part — spec requires cxsel be saved as thread context; project assumption is kernel task_struct (analogous to fcsr for floating point). ucontext_t support (cx_open_ucontext) deferred.

* **Exclusive vs. shared CX instances on an OS supporting only one model**: The spec notes (non-normatively): "TODO: detail how both shared and exclusive CX models live atop an OS that supports only one, or the other." Unresolved. Impact on runtime implementation TBD.
  * status: Open — unresolved in spec; impact on runtime implementation TBD.

* **§isa-priv typo — privileged CSR presence condition**: The spec states "The privileged CSRs and opcodes are present when `Zcxmulti` is present and the supervisor extension is present," but the CSR tables are explicitly split between `Zcx` (`scxstp`, `scxxs0`–`scxxs3`) and `Zcxmulti` (`scxNxs`). Requiring `Zcxmulti` for `scxstp` would leave `Zcx` with no mode control register. The sentence likely applies only to the `Zcxmulti`-specific CSRs; the `Zcx` CSRs should be conditioned on `Zcx` + `S` alone. Likely a spec typo.
  * status: Resolved by reading — spec typo; scxstp and scxxs0–scxxs3 are conditioned on Zcx + S; scxNxs registers are conditioned on Zcxmulti + S. Fix to be submitted to spec.

* **Single `cxsel` and the four custom opcode spaces**: A proposal for multiple simultaneous selection registers (`cxsel0`/`cxsel1`/..., one per custom opcode space) was tabled by the TG and is out of scope for this project.

* **State management as a separate extension**: Is the state management functionality (cxsidx, cxsdata, cxdiscard) intended to be a separate, optional extension (e.g., `Zcxstate`) that can be implemented independently of the base CX multiplexing extension, or is it always bundled with `Zcx`?
  * status: Open — awaiting spec clarification.

* **Replacement for the R (ready/busy) flag**: The basis spec included an R flag to indicate whether a CX is busy. It is not present in the current spec. What replaces it? How does software determine that a CX is ready to accept operations vs. busy?
  * status: Open — awaiting spec clarification.

* **`cx_status` CSR or get_status opcode**: Should a mechanism be added to query the status of the currently selected CX instance — either a dedicated `cx_status` CSR or a `get_status` opcode (in custom or non-custom opcode space)?
  * status: Open — not currently in spec; awaiting TG decision.

* **CX initialization procedure and data leakage**: The spec indicates CX initialization is performed according to individual CX specifications, which implies it occurs in userspace. Is that sufficient? What prevents stale state from a prior context from leaking to a new user of the same CX instance?
  * status: Open — userspace-defined initialization may be insufficient to prevent data leakage across CX instance reuse; security implications and enforcement mechanism unresolved.
