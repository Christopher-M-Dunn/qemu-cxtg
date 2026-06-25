/*
 * RISC-V translation routines for Zcx (Composable Extensions).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

static bool trans_cxsetsel(DisasContext *ctx, arg_r2 *a)
{
    TCGv src, dest;

    if (!ctx->cfg_ptr->ext_zcx) {
        return false;
    }
    src  = get_gpr(ctx, a->rs1, EXT_NONE);
    dest = dest_gpr(ctx, a->rd);
    translator_io_start(&ctx->base);
    gen_helper_cxsetsel(dest, tcg_env, src);
    gen_set_gpr(ctx, a->rd, dest);
    return do_csr_post(ctx);
}
