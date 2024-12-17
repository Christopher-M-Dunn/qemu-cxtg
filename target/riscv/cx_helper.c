// qemu/target/riscv/
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"

#include <assert.h>
#include <stdio.h>

#include "../../../../../zoo/exports.h"

target_ulong HELPER(cx_reg)(CPURISCVState *env, target_ulong cf_id, 
                             target_ulong rs1, target_ulong rs2)
{
    
    // not sure if these are the right error values to set it to
    if (env->mcx_selector == CX_INVALID_SELECTOR) {
        cx_status_t cx_status = {.idx = env->cx_status};
        cx_status.sel.IV = 1;
        cx_status.sel.IC = 1;
        cx_status.sel.IS = 1;
        env->cx_status = cx_status.idx;
        return (target_ulong)-1;
    }

    if (GET_CX_CXE(env->mcx_selector) == 1) {
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
    }

    uint32_t OPCODE_ID = cf_id;
    int32_t OPA = rs1;
    int32_t OPB = rs2;
    uint32_t CX_ID = GET_CX_ID(env->mcx_selector);
    uint32_t STATE_ID = GET_CX_STATE(env->mcx_selector);
    uint32_t VERSION = GET_CX_VERSION(env->mcx_selector);

    if (VERSION != 1) {
        cx_status_t cx_status = {.idx = env->cx_status};
        cx_status.sel.IV = 1;
        env->cx_status = cx_status.idx;
    }

    // stateless 
    if (STATE_ID > 0 && num_states[CX_ID] == 0) {
        cx_status_t cx_status = {.idx = env->cx_status};
        cx_status.sel.IS = 1;
        env->cx_status = cx_status.idx;
    }
    // stateful
    else if (STATE_ID > num_states[CX_ID] - 1) {
        cx_status_t cx_status = {.idx = env->cx_status};
        cx_status.sel.IS = 1;
        env->cx_status = cx_status.idx;
    }

    if (OPCODE_ID > num_cfs[CX_ID] - 1) {
        cx_status_t cx_status = {.idx = env->cx_status};
        cx_status.sel.IF = 1;
        env->cx_status = cx_status.idx;
    }
    
    assert( CX_ID < MAX_CX_ID); // Possibly redundant

    cx_selidx_t sys_sel = {.idx = env->mcx_selector};
    int32_t out = cx_funcs[CX_ID][OPCODE_ID](OPA, OPB, sys_sel);

    return (target_ulong)out;
} 