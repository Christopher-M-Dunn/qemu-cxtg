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
    uint32_t CX_CXE = GET_CX_CXE(env->mcx_selector);
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
    }

    else if (CX_CXE == 1) {
        riscv_raise_exception(env, RISCV_EXCP_ILLEGAL_INST, GETPC());
    }

    else if (VERSION != 1) {
        cx_status.sel.IV = 1;
    }

    else if (CX_ID > MAX_CX_ID - 1) {
        cx_status.sel.IC = 1;
    }
 
    else if (STATE_ID > (num_states[CX_ID] == 0 ? 0 : num_states[CX_ID] - 1) ) {
        cx_status.sel.IS = 1;
    }

    //    else if (){
    //        //TODO: OF flag
    //    }

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

    else { // no errors
        cx_selidx_t sys_sel = {.idx = env->mcx_selector};
        out = cx_funcs[CX_ID][OPCODE_ID](OPA, OPB, sys_sel);

        //    if (){
        //        //TODO: OP flag
        //    }

        //    else if (){
        //        //TODO: CU flag
        //    }
    }

    env->cx_status = cx_status.idx;
    return (target_ulong)out;
} 