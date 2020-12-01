#include "codegen.h"

#include <assert.h>
#include <limits.h>  // CHAR_BIT
#include <stdlib.h>  // malloc

#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "regalloc.h"
#include "type.h"
#include "util.h"
#include "var.h"

#include "parser.h"  // curfunc

VRegType *to_vtype(const Type *type) {
  VRegType *vtype = malloc(sizeof(*vtype));
  vtype->size = type_size(type);
  vtype->align = align_size(type);

  int flag = 0;
  bool is_unsigned = is_fixnum(type->kind) ? type->fixnum.is_unsigned : true;
#ifndef __NO_FLONUM
  if (is_flonum(type)) {
    flag |= VRTF_FLONUM;
    is_unsigned = false;
  }
#endif
  if (is_unsigned)
    flag |= VRTF_UNSIGNED;
  vtype->flag = flag;

  return vtype;
}

VReg *add_new_reg(const Type *type, int flag) {
  return reg_alloc_spawn(curfunc->ra, to_vtype(type), flag);
}

static enum ConditionKind swap_cond(enum ConditionKind cond) {
  assert(COND_EQ <= cond && cond <= COND_GT);
  if (cond >= COND_LT)
    cond = COND_GT - (cond - COND_LT);
  return cond;
}

static enum ConditionKind gen_compare_expr(enum ExprKind kind, Expr *lhs, Expr *rhs) {
  assert(lhs->type->kind == rhs->type->kind);

  enum ConditionKind cond = kind + (COND_EQ - EX_EQ);
  assert(cond >= COND_EQ && cond < COND_ULT);
  if (is_const(rhs) && !is_const(lhs)) {
    Expr *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
    cond = swap_cond(cond);
  }

  if (cond > COND_NE &&
      ((is_fixnum(lhs->type->kind) && lhs->type->fixnum.is_unsigned) ||
#ifndef __NO_FLONUM
        is_flonum(lhs->type) ||
#endif
       lhs->type->kind == TY_PTR)) {
    // unsigned
    cond += COND_ULT - COND_LT;
  }

  VReg *lhs_reg = gen_expr(lhs);
  if (rhs->kind == EX_FIXNUM && rhs->fixnum == 0 &&
      (cond == COND_EQ || cond == COND_NE)) {
    new_ir_test(lhs_reg);
  } else if (rhs->kind == EX_FIXNUM &&
             ((is_fixnum(lhs->type->kind) && lhs->type->fixnum.kind < FX_LONG) ||
               is_im32(rhs->fixnum))) {
    VReg *num = new_const_vreg(rhs->fixnum, to_vtype(rhs->type));
    new_ir_cmp(lhs_reg, num);
  } else {
    switch (lhs->type->kind) {
    case TY_FIXNUM: case TY_PTR:
#ifndef __NO_FLONUM
    case TY_FLONUM:
#endif
      break;
    default: assert(false); break;
    }

    VReg *rhs_reg = gen_expr(rhs);
    // Allocate new register to avoid comparing spilled registers.
    VReg *tmp = add_new_reg(lhs->type, 0);
    new_ir_mov(tmp, lhs_reg);
    new_ir_cmp(tmp, rhs_reg);
  }

  return cond;
}

void gen_cond_jmp(Expr *cond, bool tf, BB *bb) {
  // Local optimization: if `cond` is compare expression, then
  // jump using flags after CMP directly.
  switch (cond->kind) {
  case EX_FIXNUM:
    if (cond->fixnum == 0)
      tf = !tf;
    if (tf)
      new_ir_jmp(COND_ANY, bb);
    return;

#ifndef __NO_FLONUM
  case EX_FLONUM:
    if (cond->flonum == 0)
      tf = !tf;
    if (tf)
      new_ir_jmp(COND_ANY, bb);
    return;
#endif

  case EX_EQ:
  case EX_NE:
    {
      enum ConditionKind kind = gen_compare_expr(cond->kind, cond->bop.lhs, cond->bop.rhs);
      if (!tf)
        kind = COND_EQ + COND_NE - kind;
      new_ir_jmp(kind, bb);
      return;
    }
  case EX_LT:
  case EX_GT:
  case EX_LE:
  case EX_GE:
    {
      enum ConditionKind kind = gen_compare_expr(cond->kind, cond->bop.lhs, cond->bop.rhs);
      switch (kind) {
      case COND_LT:
      case COND_GE:
        if (!tf)
          kind = COND_LT + COND_GE - kind;
        new_ir_jmp(kind, bb);
        break;
      case COND_GT:
      case COND_LE:
        if (!tf)
          kind = COND_GT + COND_LE - kind;
        new_ir_jmp(kind, bb);
        break;
      case COND_ULT:
      case COND_UGE:
        if (!tf)
          kind = COND_ULT + COND_UGE - kind;
        new_ir_jmp(kind, bb);
        break;
      case COND_UGT:
      case COND_ULE:
        if (!tf)
          kind = COND_UGT + COND_ULE - kind;
        new_ir_jmp(kind, bb);
        break;
      default:  assert(false); break;
      }
    }
    return;
  case EX_NOT:
    gen_cond_jmp(cond->unary.sub, !tf, bb);
    return;
  case EX_LOGAND:
    if (!tf) {
      BB *bb1 = bb_split(curbb);
      BB *bb2 = bb_split(bb1);
      gen_cond_jmp(cond->bop.lhs, false, bb);
      set_curbb(bb1);
      gen_cond_jmp(cond->bop.rhs, false, bb);
      set_curbb(bb2);
    } else {
      BB *bb1 = bb_split(curbb);
      BB *bb2 = bb_split(bb1);
      gen_cond_jmp(cond->bop.lhs, false, bb2);
      set_curbb(bb1);
      gen_cond_jmp(cond->bop.rhs, true, bb);
      set_curbb(bb2);
    }
    return;
  case EX_LOGIOR:
    if (tf) {
      BB *bb1 = bb_split(curbb);
      BB *bb2 = bb_split(bb1);
      gen_cond_jmp(cond->bop.lhs, true, bb);
      set_curbb(bb1);
      gen_cond_jmp(cond->bop.rhs, true, bb);
      set_curbb(bb2);
    } else {
      BB *bb1 = bb_split(curbb);
      BB *bb2 = bb_split(bb1);
      gen_cond_jmp(cond->bop.lhs, true, bb2);
      set_curbb(bb1);
      gen_cond_jmp(cond->bop.rhs, false, bb);
      set_curbb(bb2);
    }
    return;
  default:
    break;
  }

#ifndef __NO_FLONUM
  if (is_flonum(cond->type)) {
    Expr *zero = new_expr_flolit(cond->type, NULL, 0.0);
    Expr *cmp = new_expr_bop(EX_NE, &tyBool, NULL, cond, zero);
    gen_cond_jmp(cmp, tf, bb);
    return;
  }
#endif
  VReg *reg = gen_expr(cond);
  new_ir_test(reg);
  new_ir_jmp(tf ? COND_NE : COND_EQ, bb);
}

static VReg *gen_cast(VReg *reg, const Type *dst_type) {
  if (reg->flag & VRF_CONST) {
#ifndef __NO_FLONUM
    if (reg->vtype->flag & VRTF_FLONUM) {
      assert(!"Not implemented");
    }
#endif
    intptr_t value = reg->fixnum;
    size_t dst_size = type_size(dst_type);
    if (dst_size < (size_t)reg->vtype->size && dst_size < sizeof(intptr_t)) {
      // Assume that integer is represented in Two's complement
      size_t bit = dst_size * CHAR_BIT;
      intptr_t mask = (-1UL) << bit;
      if (dst_type->kind == TY_FIXNUM && !dst_type->fixnum.is_unsigned &&  // signed
          (value & (1 << (bit - 1))))  // negative
        value |= mask;
      else
        value &= ~mask;
    }

#ifndef __NO_FLONUM
    // TODO: Handle when dst is float.
#endif
    VRegType *vtype = to_vtype(dst_type);
    return new_const_vreg(value, vtype);
  }

  int dst_size = type_size(dst_type);
  bool lu = dst_type->kind == TY_FIXNUM ? dst_type->fixnum.is_unsigned : dst_type->kind == TY_PTR;
  bool ru = (reg->vtype->flag & VRTF_UNSIGNED) ? true : false;
  if (dst_size == reg->vtype->size && lu == ru
#ifndef __NO_FLONUM
      && is_flonum(dst_type) == ((reg->vtype->flag & VRTF_FLONUM) != 0)
#endif
  )
    return reg;

  return new_ir_cast(reg, to_vtype(dst_type));
}

static VReg *gen_lval(Expr *expr) {
  switch (expr->kind) {
  case EX_VAR:
    {
      Scope *scope;
      const VarInfo *varinfo = scope_find(expr->var.scope, expr->var.name, &scope);
      assert(varinfo != NULL && scope == expr->var.scope);
      if (is_global_scope(scope)) {
        return new_ir_iofs(expr->var.name, (varinfo->flag & VF_STATIC) == 0);
      } else {
        if (varinfo->flag & VF_STATIC)
          return new_ir_iofs(varinfo->static_.gvar->name, false);
        else if (varinfo->flag & VF_EXTERN)
          return new_ir_iofs(expr->var.name, true);
        else
          return new_ir_bofs(varinfo->local.reg);
      }
    }
  case EX_DEREF:
    return gen_expr(expr->unary.sub);
  case EX_MEMBER:
    {
      const Type *type = expr->member.target->type;
      if (ptr_or_array(type))
        type = type->pa.ptrof;
      assert(type->kind == TY_STRUCT);
      const Vector *members = type->struct_.info->members;
      const VarInfo *member = members->data[expr->member.index];

      VReg *reg;
      if (expr->member.target->type->kind == TY_PTR)
        reg = gen_expr(expr->member.target);
      else
        reg = gen_lval(expr->member.target);
      if (member->struct_member.offset == 0)
        return reg;
      VRegType *vtype = to_vtype(&tySize);
      VReg *imm = new_const_vreg(member->struct_member.offset, vtype);
      VReg *result = new_ir_bop(IR_ADD, reg, imm, vtype);
      return result;
    }
  case EX_COMPLIT:
    {
      Expr *var = expr->complit.var;
      assert(var->var.scope != NULL);
      const VarInfo *varinfo = scope_find(var->var.scope, var->var.name, NULL);
      assert(varinfo != NULL);
      assert(varinfo->local.reg != NULL);
      varinfo->local.reg->flag |= VRF_REF;

      gen_stmts(expr->complit.inits);
      return gen_lval(expr->complit.var);
    }
  default:
    assert(false);
    break;
  }
  return NULL;
}

static VReg *gen_variable(Expr *expr) {
  switch (expr->type->kind) {
  case TY_FIXNUM:
  case TY_PTR:
#ifndef __NO_FLONUM
  case TY_FLONUM:
#endif
    {
      Scope *scope;
      const VarInfo *varinfo = scope_find(expr->var.scope, expr->var.name, &scope);
      assert(varinfo != NULL && scope == expr->var.scope);
      if (!is_global_scope(scope) && !(varinfo->flag & (VF_STATIC | VF_EXTERN))) {
        assert(varinfo->local.reg != NULL);
        return varinfo->local.reg;
      }

      VReg *reg = gen_lval(expr);
      VReg *result = new_ir_unary(IR_LOAD, reg, to_vtype(expr->type));
      return result;
    }
  default:
    assert(false);
    // Fallthrough to suppress compile error.
  case TY_ARRAY:   // Use variable address as a pointer.
  case TY_STRUCT:  // struct value is handled as a pointer.
  case TY_FUNC:
    return gen_lval(expr);
  }
}

static VReg *gen_ternary(Expr *expr) {
  BB *tbb = bb_split(curbb);
  BB *fbb = bb_split(tbb);
  BB *nbb = bb_split(fbb);
  bool no_value = expr->type->kind == TY_VOID;

  VReg *result = add_new_reg(expr->type, 0);
  gen_cond_jmp(expr->ternary.cond, false, fbb);

  set_curbb(tbb);
  VReg *tval = gen_expr(expr->ternary.tval);
  if (!no_value)
    new_ir_mov(result, tval);
  new_ir_jmp(COND_ANY, nbb);

  set_curbb(fbb);
  VReg *fval = gen_expr(expr->ternary.fval);
  if (!no_value)
    new_ir_mov(result, fval);

  set_curbb(nbb);
  return result;
}

bool is_stack_param(const Type *type) {
  return type->kind == TY_STRUCT;
}

typedef struct {
  int reg_index;
  int offset;
  int size;
  bool stack_arg;
#ifndef __NO_FLONUM
  bool is_flonum;
#endif
} ArgInfo;

static VReg *gen_funcall(Expr *expr) {
  Expr *func = expr->funcall.func;
  Vector *args = expr->funcall.args;
  int arg_count = args != NULL ? args->len : 0;

  int offset = 0;

  ArgInfo ret_info;
  ret_info.reg_index = -1;
  ret_info.offset = -1;
  ret_info.size = type_size(expr->type);
  ret_info.stack_arg = is_stack_param(expr->type);
  if (ret_info.stack_arg) {
    ret_info.reg_index = 0;
    ret_info.offset = 0;
    offset += ret_info.size;
  }

  ArgInfo *arg_infos = NULL;
  int stack_arg_count = 0;
  if (args != NULL) {
    bool vaargs = false;
    if (func->kind == EX_VAR && is_global_scope(func->var.scope)) {
      vaargs = func->type->func.vaargs;
    } else {
      // TODO:
    }

    int reg_index = 0;
    if (ret_info.stack_arg)
      ++reg_index;
#ifndef __NO_FLONUM
    int freg_index = 0;
#endif

    // Check stack arguments.
    arg_infos = malloc(sizeof(*arg_infos) * arg_count);
    for (int i = 0; i < arg_count; ++i) {
      ArgInfo *p = &arg_infos[i];
      p->reg_index = -1;
      p->offset = -1;
      Expr *arg = args->data[i];
      assert(arg->type->kind != TY_ARRAY);
      p->size = type_size(arg->type);
#ifndef __NO_FLONUM
      p->is_flonum = is_flonum(arg->type);
#endif
      p->stack_arg = is_stack_param(arg->type);
      bool reg_arg = !p->stack_arg;
      if (reg_arg) {
#ifndef __NO_FLONUM
        if (p->is_flonum)
          reg_arg = freg_index < MAX_FREG_ARGS;
        else
#endif
          reg_arg = reg_index < MAX_REG_ARGS;
      }
      if (!reg_arg) {
        if (reg_index >= MAX_REG_ARGS && vaargs) {
          parse_error(((Expr*)args->data[reg_index])->token,
                      "Param count exceeds %d", MAX_REG_ARGS);
        }

        offset = ALIGN(offset, align_size(arg->type));
        p->offset = offset;
        offset += ALIGN(p->size, WORD_SIZE);
        ++stack_arg_count;
      } else {
#ifndef __NO_FLONUM
        if (p->is_flonum)
          p->reg_index = freg_index++;
        else
#endif
          p->reg_index = reg_index++;
      }
    }
  }
  offset = ALIGN(offset, 8);

  IR *precall = new_ir_precall(arg_count - stack_arg_count, offset);

  int reg_arg_count = 0;
  if (offset > 0)
    new_ir_addsp(-offset);
  unsigned int arg_type_bits = 0;
  if (args != NULL) {
    // Register arguments.
    for (int i = arg_count; --i >= 0; ) {
      Expr *arg = args->data[i];
      VReg *reg = gen_expr(arg);
      const ArgInfo *p = &arg_infos[i];
#ifndef __NO_FLONUM
      if (p->is_flonum)
        arg_type_bits |= 1 << i;
#endif
      if (p->offset < 0) {
        new_ir_pusharg(reg, to_vtype(arg->type));
        ++reg_arg_count;
      } else {
        VRegType offset_type = {.size = 4, .align = 4, .flag = 0};  // TODO:
        VReg *dst = new_ir_sofs(new_const_vreg(p->offset + reg_arg_count * WORD_SIZE,
                                               &offset_type));
        if (p->stack_arg) {
          new_ir_memcpy(dst, reg, type_size(arg->type));
        } else {
          if (reg->flag & VRF_CONST) {
            // Allocate new register to avoid constant register.
            VReg *tmp = add_new_reg(arg->type, 0);
            new_ir_mov(tmp, reg);
            reg = tmp;
          }
          new_ir_store(dst, reg);
        }
      }
    }
  }
  if (ret_info.stack_arg) {
    VRegType offset_type = {.size = 4, .align = 4, .flag = 0};  // TODO:
    VReg *dst = new_ir_sofs(new_const_vreg(ret_info.offset + reg_arg_count * WORD_SIZE,
                                            &offset_type));
    new_ir_pusharg(dst, to_vtype(ptrof(expr->type)));
    ++reg_arg_count;
    arg_type_bits <<= 1;
  }

  bool label_call = false;
  bool global = false;
  if (func->kind == EX_VAR) {
    const VarInfo *varinfo = scope_find(func->var.scope, func->var.name, NULL);
    assert(varinfo != NULL);
    label_call = varinfo->type->kind == TY_FUNC;
    global = !(varinfo->flag & VF_STATIC);
  }

  VReg *result_reg = NULL;
  {
    const Type *type = expr->type;
    if (ret_info.stack_arg)
      type = ptrof(type);
    VRegType *ret_vtype = to_vtype(type);
    if (label_call) {
      result_reg = new_ir_call(func->var.name, global, NULL, reg_arg_count, ret_vtype,
                               precall, arg_type_bits);
    } else {
      VReg *freg = gen_expr(func);
      result_reg = new_ir_call(NULL, false, freg, reg_arg_count, ret_vtype, precall, arg_type_bits);
    }
  }

  free(arg_infos);

  return result_reg;
}

VReg *gen_arith(enum ExprKind kind, const Type *type, VReg *lhs, VReg *rhs) {
  switch (kind) {
  case EX_ADD:
  case EX_SUB:
  case EX_MUL:
#ifndef __NO_FLONUM
    if (is_flonum(type)) {
      return new_ir_bop(kind + (IR_ADD - EX_ADD), lhs, rhs, to_vtype(type));
    }
#endif
    return new_ir_bop(kind + (IR_ADD - EX_ADD), lhs, rhs, to_vtype(type));
  case EX_BITAND:
  case EX_BITOR:
  case EX_BITXOR:
  case EX_LSHIFT:
  case EX_RSHIFT:
    return new_ir_bop(kind + (IR_ADD - EX_ADD), lhs, rhs, to_vtype(type));

  case EX_DIV:
  case EX_MOD:
    assert(is_number(type));
#ifndef __NO_FLONUM
    if (is_flonum(type)) {
      return new_ir_bop(kind + (IR_DIV - EX_DIV), lhs, rhs, to_vtype(type));
    }
#endif
    return new_ir_bop(kind + ((type->fixnum.is_unsigned ? IR_DIVU : IR_DIV) - EX_DIV), lhs, rhs, to_vtype(type));

  default:
    assert(false);
    return NULL;
  }
}

VReg *gen_ptradd(enum ExprKind kind, const Type *type, VReg *lreg, Expr *rhs) {
  size_t scale = type_size(type->pa.ptrof);

  Expr *raw_rhs = rhs;
  while (raw_rhs->kind == EX_CAST)
    raw_rhs = raw_rhs->unary.sub;
  if (is_const(raw_rhs)) {
    intptr_t rval = raw_rhs->fixnum;
    if (kind == EX_PTRSUB)
      rval = -rval;
    return new_ir_ptradd(rval * scale, lreg, NULL, 1, to_vtype(type));
  } else {
    VReg *rreg = gen_expr(rhs);
    if (kind == EX_PTRSUB) {
      rreg = new_ir_unary(IR_NEG, rreg, to_vtype(rhs->type));
#if 1
    } else {  // To avoid both spilled registers, add temporary register.
      VReg *tmp = add_new_reg(rhs->type, 0);
      new_ir_mov(tmp, rreg);
      rreg = tmp;
#endif
    }
    if (scale > 8 || !IS_POWER_OF_2(scale)) {
      VRegType *vtype = to_vtype(rhs->type);
      VReg *sreg = new_const_vreg(scale, vtype);
      rreg = new_ir_bop(IR_MUL, rreg, sreg, vtype);
      scale = 1;
    }
    rreg = new_ir_cast(rreg, to_vtype(&tySize));
    return new_ir_ptradd(0, lreg, rreg, scale, to_vtype(type));
  }
}

VReg *gen_expr(Expr *expr) {
  switch (expr->kind) {
  case EX_FIXNUM:
    assert(expr->type->kind == TY_FIXNUM);
    return new_const_vreg(expr->fixnum, to_vtype(expr->type));
#ifndef __NO_FLONUM
  case EX_FLONUM:
    {
      assert(expr->type->kind == TY_FLONUM);
      Initializer *init = malloc(sizeof(*init));
      init->kind = IK_SINGLE;
      init->single = expr;
      init->token = expr->token;

      assert(curscope != NULL);
      const Type *type = expr->type;
      const Token *ident = alloc_ident(alloc_label(), NULL, NULL);
      VarInfo *varinfo = scope_add(curscope, ident, type, VF_CONST | VF_STATIC);
      varinfo->global.init = init;

      VReg *src = new_ir_iofs(varinfo->name, false);
      return new_ir_unary(IR_LOAD, src, to_vtype(type));
    }
#endif

  case EX_STR:
    {
      Initializer *init = malloc(sizeof(*init));
      init->kind = IK_SINGLE;
      init->single = expr;
      init->token = expr->token;

      const Type* strtype = arrayof(&tyChar, expr->str.size);
      VarInfo *varinfo = str_to_char_array(strtype, init);
      return new_ir_iofs(varinfo->name, false);
    }

  case EX_VAR:
    return gen_variable(expr);

  case EX_REF:
    {
      Expr *sub = expr->unary.sub;
      if (sub->kind == EX_VAR && !is_global_scope(sub->var.scope)) {
        const VarInfo *varinfo = scope_find(sub->var.scope, sub->var.name, NULL);
        assert(varinfo != NULL);
        assert(varinfo->local.reg != NULL);
        varinfo->local.reg->flag |= VRF_REF;
      }
      return gen_lval(sub);
    }

  case EX_DEREF:
    {
      VReg *reg = gen_expr(expr->unary.sub);
      VReg *result;
      switch (expr->type->kind) {
      case TY_FIXNUM:
      case TY_PTR:
#ifndef __NO_FLONUM
      case TY_FLONUM:
#endif
        result = new_ir_unary(IR_LOAD, reg, to_vtype(expr->type));
        return result;

      default:
        assert(false);
        // Fallthrough to suppress compile error.
      case TY_ARRAY:
      case TY_STRUCT:
      case TY_FUNC:
        // array, struct and func values are handled as a pointer.
        return reg;
      }
    }

  case EX_MEMBER:
    {
      VReg *reg = gen_lval(expr);
      VReg *result;
      switch (expr->type->kind) {
      case TY_FIXNUM:
      case TY_PTR:
#ifndef __NO_FLONUM
      case TY_FLONUM:
#endif
        result = new_ir_unary(IR_LOAD, reg, to_vtype(expr->type));
        break;
      default:
        assert(false);
        // Fallthrough to suppress compile error.
      case TY_ARRAY:
      case TY_STRUCT:
        result = reg;
        break;
      }
      return result;
    }

  case EX_COMMA:
    gen_expr(expr->bop.lhs);
    return gen_expr(expr->bop.rhs);

  case EX_TERNARY:
    return gen_ternary(expr);

  case EX_CAST:
    return gen_cast(gen_expr(expr->unary.sub), expr->type);

  case EX_ASSIGN:
    {
      VReg *src = gen_expr(expr->bop.rhs);
      if (expr->bop.lhs->kind == EX_VAR) {
        Expr *lhs = expr->bop.lhs;
        switch (lhs->type->kind) {
        case TY_FIXNUM:
        case TY_PTR:
          {
            Scope *scope;
            const VarInfo *varinfo = scope_find(lhs->var.scope, lhs->var.name, &scope);
            assert(varinfo != NULL);
            if (!is_global_scope(scope) && !(varinfo->flag & (VF_STATIC | VF_EXTERN))) {
              assert(varinfo->local.reg != NULL);
              new_ir_mov(varinfo->local.reg, src);
              return src;
            }
          }
          break;
        default:
          break;
        }
      }

      VReg *dst = gen_lval(expr->bop.lhs);

      switch (expr->type->kind) {
      default:
        assert(false);
        // Fallthrough to suppress compiler error.
      case TY_FIXNUM:
      case TY_PTR:
#ifndef __NO_FLONUM
      case TY_FLONUM:
#endif
#if 0
        new_ir_store(dst, src);
#else
        // To avoid both spilled registers, add temporary register.
        {
          VReg *tmp = add_new_reg(expr->type, 0);
          new_ir_mov(tmp, src);
          new_ir_store(dst, tmp);
        }
#endif
        break;
      case TY_STRUCT:
        {
          VReg *tmp = add_new_reg(&tyVoidPtr, 0);
          new_ir_mov(tmp, src);
          new_ir_memcpy(dst, tmp, expr->type->struct_.info->size);
        }
        break;
      }
      return src;
    }

  case EX_MODIFY:
    {
      Expr *sub = expr->unary.sub;
      switch (sub->kind) {
      case EX_PTRADD:
      case EX_PTRSUB:
        if (sub->bop.lhs->kind == EX_VAR && !is_global_scope(sub->bop.lhs->var.scope)) {
          VReg *lhs = gen_expr(sub->bop.lhs);
          VReg *result = gen_ptradd(sub->kind, sub->type, lhs, sub->bop.rhs);
          new_ir_mov(lhs, result);
          return result;
        } else {
          VReg *lval = gen_lval(sub->bop.lhs);
          VReg *lhs = new_ir_unary(IR_LOAD, lval, to_vtype(sub->bop.lhs->type));
          VReg *result = gen_ptradd(sub->kind, sub->type, lhs, sub->bop.rhs);
          VReg *cast = gen_cast(result, expr->type);
          new_ir_store(lval, cast);
          return result;
        }
      default:
        if (sub->bop.lhs->kind == EX_VAR && !is_global_scope(sub->bop.lhs->var.scope)) {
          VReg *lhs = gen_expr(sub->bop.lhs);
          VReg *rhs = gen_expr(sub->bop.rhs);
          VReg *result = gen_arith(sub->kind, sub->type, lhs, rhs);
          new_ir_mov(lhs, result);
          return result;
        } else {
          VReg *lval = gen_lval(sub->bop.lhs);
          VReg *rhs = gen_expr(sub->bop.rhs);
          VReg *lhs = new_ir_unary(IR_LOAD, lval, to_vtype(sub->bop.lhs->type));
          VReg *result = gen_arith(sub->kind, sub->type, lhs, rhs);
          VReg *cast = gen_cast(result, expr->type);
          new_ir_store(lval, cast);
          return result;
        }
      }
    }

  case EX_PREINC:
  case EX_PREDEC:
    {
      size_t value = 1;
      if (expr->type->kind == TY_PTR)
        value = type_size(expr->type->pa.ptrof);

      VRegType *vtype = to_vtype(expr->type);
      Expr *sub = expr->unary.sub;
      if (sub->kind == EX_VAR && !is_global_scope(sub->var.scope)) {
        const VarInfo *varinfo = scope_find(sub->var.scope, sub->var.name, NULL);
        assert(varinfo != NULL);
        if (!(varinfo->flag & (VF_STATIC | VF_EXTERN))) {
          VReg *num = new_const_vreg(value, vtype);
          VReg *result = new_ir_bop(expr->kind == EX_PREINC ? IR_ADD : IR_SUB,
                                    varinfo->local.reg, num, vtype);
          new_ir_mov(varinfo->local.reg, result);
          return result;
        }
      }

      VReg *lval = gen_lval(sub);
      new_ir_incdec(expr->kind == EX_PREINC ? IR_INC : IR_DEC,
                    lval, type_size(expr->type), value);
      VReg *result = new_ir_unary(IR_LOAD, lval, vtype);
      return result;
    }

  case EX_POSTINC:
  case EX_POSTDEC:
    {
      size_t value = 1;
      if (expr->type->kind == TY_PTR)
        value = type_size(expr->type->pa.ptrof);

      VRegType *vtype = to_vtype(expr->type);
      Expr *sub = expr->unary.sub;
      if (sub->kind == EX_VAR && !is_global_scope(sub->var.scope)) {
        const VarInfo *varinfo = scope_find(sub->var.scope, sub->var.name, NULL);
        assert(varinfo != NULL);
        if (!(varinfo->flag & (VF_STATIC | VF_EXTERN))) {
          VReg *org_val = add_new_reg(sub->type, 0);
          new_ir_mov(org_val, varinfo->local.reg);
          VReg *num = new_const_vreg(value, vtype);
          VReg *result = new_ir_bop(expr->kind == EX_POSTINC ? IR_ADD : IR_SUB,
                                    varinfo->local.reg, num, vtype);
          new_ir_mov(varinfo->local.reg, result);
          return org_val;
        }
      }

      VReg *lval = gen_lval(expr->unary.sub);
      VReg *result = new_ir_unary(IR_LOAD, lval, vtype);
      new_ir_incdec(expr->kind == EX_POSTINC ? IR_INC : IR_DEC,
                    lval, type_size(expr->type), value);
      return result;
    }

  case EX_FUNCALL:
    return gen_funcall(expr);

  case EX_POS:
    return gen_expr(expr->unary.sub);

  case EX_NEG:
    {
      VReg *reg = gen_expr(expr->unary.sub);
#ifndef __NO_FLONUM
      if (is_flonum(expr->type)) {
        VReg *zero = gen_expr(new_expr_flolit(expr->type, NULL, 0.0));
        return gen_arith(EX_SUB, expr->type, zero, reg);
      }
#endif
      VReg *result = new_ir_unary(IR_NEG, reg, to_vtype(expr->type));
      return result;
    }

  case EX_NOT:
    {
      VReg *result;
      switch (expr->unary.sub->type->kind) {
      case TY_FIXNUM: case TY_PTR:
        result = new_ir_unary(IR_NOT, gen_expr(expr->unary.sub), to_vtype(expr->type));
        break;
      default:
        assert(false);
        // Fallthrough to suppress compile error
      case TY_ARRAY: case TY_FUNC:
        // Array is handled as a pointer.
        result = new_ir_unary(IR_NOT, gen_expr(expr->unary.sub), to_vtype(expr->type));
        break;
      }
      return result;
    }

  case EX_BITNOT:
    {
      VReg *reg = gen_expr(expr->unary.sub);
      VReg *result = new_ir_unary(IR_BITNOT, reg, to_vtype(expr->type));
      return result;
    }

  case EX_EQ:
  case EX_NE:
  case EX_LT:
  case EX_GT:
  case EX_LE:
  case EX_GE:
    {
      enum ConditionKind cond = gen_compare_expr(expr->kind, expr->bop.lhs, expr->bop.rhs);
      return new_ir_cond(cond);
    }

  case EX_LOGAND:
    {
      BB *bb1 = bb_split(curbb);
      BB *bb2 = bb_split(bb1);
      BB *false_bb = bb_split(bb2);
      BB *next_bb = bb_split(false_bb);
      gen_cond_jmp(expr->bop.lhs, false, false_bb);
      set_curbb(bb1);
      gen_cond_jmp(expr->bop.rhs, false, false_bb);
      set_curbb(bb2);
      VRegType *vtbool = to_vtype(&tyBool);
      VReg *result = add_new_reg(&tyBool, 0);
      new_ir_mov(result, new_const_vreg(true, vtbool));
      new_ir_jmp(COND_ANY, next_bb);
      set_curbb(false_bb);
      new_ir_mov(result, new_const_vreg(false, vtbool));
      set_curbb(next_bb);
      return result;
    }

  case EX_LOGIOR:
    {
      BB *bb1 = bb_split(curbb);
      BB *bb2 = bb_split(bb1);
      BB *true_bb = bb_split(bb2);
      BB *next_bb = bb_split(true_bb);
      gen_cond_jmp(expr->bop.lhs, true, true_bb);
      set_curbb(bb1);
      gen_cond_jmp(expr->bop.rhs, true, true_bb);
      set_curbb(bb2);
      VRegType *vtbool = to_vtype(&tyBool);
      VReg *result = add_new_reg(&tyBool, 0);
      new_ir_mov(result, new_const_vreg(false, vtbool));
      new_ir_jmp(COND_ANY, next_bb);
      set_curbb(true_bb);
      new_ir_mov(result, new_const_vreg(true, vtbool));
      set_curbb(next_bb);
      return result;
    }

  case EX_ADD:
  case EX_SUB:
  case EX_MUL:
  case EX_DIV:
  case EX_MOD:
  case EX_LSHIFT:
  case EX_RSHIFT:
  case EX_BITAND:
  case EX_BITOR:
  case EX_BITXOR:
    {
      VReg *lhs = gen_expr(expr->bop.lhs);
      VReg *rhs = gen_expr(expr->bop.rhs);
      return gen_arith(expr->kind, expr->type, lhs, rhs);
    }

  case EX_PTRADD:
  case EX_PTRSUB:
    {
      assert(expr->type->kind == TY_PTR);
      VReg *lreg = gen_expr(expr->bop.lhs);
      return gen_ptradd(expr->kind, expr->type, lreg, expr->bop.rhs);
    }

  case EX_COMPLIT:
    gen_stmts(expr->complit.inits);
    return gen_expr(expr->complit.var);

  default:
    fprintf(stderr, "Expr kind=%d, ", expr->kind);
    assert(!"Unhandled in gen_expr");
    break;
  }

  return NULL;
}
