# Implementation Choices

Numbered, append-only log of project-specific implementation decisions that go beyond what is stated in `Requirements.md` and the Requirements section of `Composable-Extensions.md`. Test for inclusion: starting from those two sources, what additional information would someone need to replicate this project's behavior exactly? That's what belongs here.

Entries are plain lines: `<N> - <text>` for a plain entry, `<N> * <text>` when the entry has a corresponding item in `Composable-Extensions.md`'s Discussion section (the `*` is the only cross-reference this document carries in that direction — `Composable-Extensions.md` cites back by number, e.g. `Implementation: I12`). State the decision only: no rationale, no source citation.

If a choice changes meaningfully, the entry at its old number becomes:

> **Deprecated. See #X.** ~~<original text>~~

and a new entry is appended at the bottom of the list starting with a reference to the entry it supersedes, where `X` is the new entry's own number.

Everything below the divider is not yet incorporated into the project. That is, into `CXTG_QEMU_Action_Plan.md`, either as a change to phases not yet completed, as the addition of a patch in between or attached to phases not yet completed, or as a footnote referenced from every point in the document affected (after the code is fixed). As entries are incorporated, they are dated and moved above the divider.

---

1 - Runtime extension flags: `ext_zcx`, `ext_zcxmulti` (requires `zcx=on` AND `sscsrind=on`). S-mode features (`scxstp`, `scxxs`) check `RVS`; `Smstateen` gating checks `ext_smstateen`; `scxNxs` checks `ext_sscsrind`; hypervisor (`hstateen`) checks `RVH`.
2 - CSR addresses: cxsel = 0xCA0, cxsidx = 0x018, cxsdata = 0x019
3 - `cxsetsel` opcode: `MATCH_CXSETSEL = 0x00004073` (SYSTEM, funct3=4, funct7=0).
4 - `cxdiscard` opcode: TBD (likely SYSTEM, funct3=4, funct7=1)