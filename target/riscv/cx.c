/*
 * QEMU RISC-V CX (Composable Extensions)
 *
 * Author: Artur Lojewski lojewski@gmail.com
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

void cxsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxsel_csr_read(env->mhartid, reg_index, val);
}

void cxsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxsel_csr_write(env->mhartid, reg_index, val);
}

void cxsetsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxsetsel_csr_read(env->mhartid, reg_index, val);
}

void cxsetsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxsetsel_csr_write(env->mhartid, reg_index, val);
}

void cxidx_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxidx_csr_read(env->mhartid, reg_index, val);
}

void cxidx_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxidx_csr_write(env->mhartid, reg_index, val);
}

void cxdata_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxdata_csr_read(env->mhartid, reg_index, val);
}

void cxdata_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val)
{
    trace_cxdata_csr_write(env->mhartid, reg_index, val);
}