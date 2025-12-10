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

#ifndef RISCV_CX_H
#define RISCV_CX_H

#include "cpu.h"

void cxsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val);
void cxsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val);

void cxsetsel_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val);
void cxsetsel_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val);

void cxidx_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val);
void cxidx_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val);

void cxdata_csr_read(CPURISCVState *env, uint32_t reg_index, target_ulong val);
void cxdata_csr_write(CPURISCVState *env, uint32_t reg_index, target_ulong val);

#endif /* RISCV_CX_H */