// target/riscv/insn_trans/trans_cx.c.inc

static bool trans_cx_reg(DisasContext *ctx, arg_cx_reg *a) 
{ 
    TCGv dest = dest_gpr(ctx, a->rd);
    TCGv cf_id = tcg_constant_tl(a->cf_id);
    TCGv src1 = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv src2 = get_gpr(ctx, a->rs2, EXT_NONE);

    gen_helper_cx_reg(dest, tcg_env, cf_id, src1, src2);
    gen_set_gpr(ctx, a->rd, dest);
    
    return true;
}