// qemu/target/riscv/
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"

#include <assert.h>
#include <stdio.h>

#include "../../../zoo/exports.h"

target_ulong HELPER(cx_reg)(CPURISCVState *env, target_ulong cf_id, 
                             target_ulong rs1, target_ulong rs2)
{
    uint32_t OPCODE_ID = cf_id;
    uint32_t OPCODE_ID_SYSTEM_START = 1020;
    int32_t OPA = rs1;
    int32_t OPB = rs2;
    uint32_t CX_ID = GET_CX_ID(env->mcx_selector);
    uint32_t STATE_ID = GET_CX_STATE(env->mcx_selector);
    uint32_t VERSION = GET_CX_VERSION(env->mcx_selector);
    
    cx_status_t cx_status = {.idx = env->cx_status};
   
    int32_t out = -1;

    // not sure if these are the right error values to set it to
    if (env->mcx_selector == CX_INVALID_SELECTOR) {
        cx_status.sel.IV = 1;
        cx_status.sel.IC = 1;
        cx_status.sel.IS = 1;
        env->cx_status = cx_status.idx;
        return (target_ulong)out;
    }

    if (GET_CX_CXE(env->mcx_selector)) {
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
        cx_status = (cx_status_t){.idx = env->cx_status};
    }

    if (VERSION != 1) {
        cx_status.sel.IV = 1;
    }

    else if (CX_ID > MAX_CX_ID - 1) {
        cx_status.sel.IC = 1;
    }
 
    else if (STATE_ID > (num_states[CX_ID] == 0 ? 0 : num_states[CX_ID] - 1) ) {
        cx_status.sel.IS = 1;
    }

    else if (num_states[CX_ID] > 0 && cx_funcs[CX_ID][1023] != NULL &&
        OPCODE_ID != 1022 && OPCODE_ID != 1023) {
        // DC=CX_OFF: state not granted/initialized — permission gate.
        // Distinct from CXE=1 which is save/restore.
        cx_selidx_t sys_sel_of = {.idx = env->mcx_selector};
        cx_stctxs_t stctxs = {.idx = cx_funcs[CX_ID][1023](0, 0, sys_sel_of)};
        if (stctxs.sel.dc == CX_OFF) {
            cx_status.sel.OF = 1;
        }
    }

    // if returning from exception, the mcx_selector values are technically
    // stale. Fine if runtime doesn't reassign to a different state (it shouldn't)
    // unless we implement live resource migration in the future.

    // TODO: CXU functions need access to env to set OP, CU, and IF flags
    // directly (analogous to a CSR write in hardware). Pass env into the
    // function signature so CXUs can set flags themselves.
    else if (OPCODE_ID >= MAX_CF_IDS) {
        // assembler enforces 10-bit CF ID, so this is unreachable from
        // a normal binary — guards against malformed instruction words
        cx_status.sel.IF = 1;
    }

    else if (OPCODE_ID >= num_cfs[CX_ID] &&
            (num_states[CX_ID] == 0 || OPCODE_ID < OPCODE_ID_SYSTEM_START)) {
        // out of user CF range:
        //   stateless: any CF >= num_cfs is invalid (no system CFs)
        //   stateful:  CFs between num_cfs and system range are invalid
        cx_status.sel.IF = 1;
    }

    else if (cx_funcs[CX_ID][OPCODE_ID] == NULL) {
        // NULL slot within valid range — unused CF ID
        cx_status.sel.IF = 1;
    }

    else { // no errors before call — check return value for sentinel flags
        cx_selidx_t sys_sel = {.idx = env->mcx_selector};
        out = cx_funcs[CX_ID][OPCODE_ID](OPA, OPB, sys_sel);

        if (out == FUNC_SENTINEL_OP_POS) {
            cx_status.sel.OP = 1;
            // out remains INT32_MAX — caller sees clipped positive value
        }

        else if (out == FUNC_SENTINEL_OP_NEG) {
            cx_status.sel.OP = 1;
            // out remains INT32_MIN — caller sees clipped negative value
        }

        else if (out == FUNC_SENTINEL_IF_INVALID_RET_0) {
            cx_status.sel.IF = 1;
            out = 0;
        }

        else if (out == FUNC_SENTINEL_IF_INVALID_RET_NEG1) {
            cx_status.sel.IF = 1;
            out = -1;
        }

        else if (out >= FUNC_SENTINEL_CU_CUSTOM_BIT_15 &&
                out <= FUNC_SENTINEL_CU_CUSTOM_BIT_0) {
            cx_status.sel.CU = 1;
            // TODO: write bit into cxs_error CSR (per-state custom error register)
            // cxs_error is not yet implemented in env; when added:
            // int bit = FUNC_SENTINEL_CU_CUSTOM_BIT_0 - out; /* 0..15 */
            // env->cxs_error |= (1u << bit);
            out = -1;
        }

        else if (out >= FUNC_SENTINEL_CU_CUSTOM_BIT_16 &&
                out <= FUNC_SENTINEL_CU_CUSTOM_BIT_31) {
            cx_status.sel.CU = 1;
            // TODO: write bit into cxs_error CSR (per-state custom error register)
            // cxs_error is not yet implemented in env; when added:
            // int bit = out - FUNC_SENTINEL_CU_CUSTOM_BIT_16 + 16; /* 16..31 */
            // env->cxs_error |= (1u << bit);
            out = -1;
        }
    }

    env->cx_status = cx_status.idx;
    return (target_ulong)out;
} 