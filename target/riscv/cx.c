/*
 * QEMU RISC-V CX (Composable Extensions)
 *
 * Authors: Artur Lojewski lojewski@gmail.com
 *          Christopher Dunn christopher.m.dunn@gmail.com
 *
 * This provides a RISC-V Composable Extensions (CX) interface
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "trace.h"

void cxsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong *val)
{
    *val = env->cxsel;
    trace_cxsel_csr_read(env->mhartid, reg_index, *val);
}

void cxsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    /* cxsel is URO — direct writes trap via hardware encoding; */
    trace_cxsel_csr_write(env->mhartid, reg_index, val);
}

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

/*
 * cxsdata_csr_op — combined atomic handler registered as csr_ops[].op.
 *
 * Using .op instead of separate .read/.write ensures cxsidx increments
 * exactly once per instruction for all CSR variants (csrr, csrw, csrrw,
 * csrrs, csrrc).  csrrw is the dominant pattern (context spill/fill).
 *
 * write_mask encodes the instruction variant:
 *   csrr/csrrs x0/csrrc x0  write_mask=0   read only
 *   csrw/csrrs/csrrc         write_mask=rs1 partial write (set/clear bits)
 *   csrrw                    write_mask=-1  full replace
 */
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