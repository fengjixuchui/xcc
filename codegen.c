#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "xcc.h"
#include "util.h"

const int FRAME_ALIGN = 8;

#define CURIP(ofs)  (start_address + codesize + ofs)
#define ADD_CODE(...)  do { unsigned char buf[] = {__VA_ARGS__}; add_code(buf, sizeof(buf)); } while (0)
#include "x86_64.h"

#define ALIGN(x, align)  (((x) + (align) - 1) & -(align))  // align must be 2^n

static void calc_struct_size(StructInfo *sinfo, bool is_union);

static int type_size(const Type *type) {
  switch (type->type) {
  case TY_VOID:
    return 1;  // ?
  case TY_CHAR:
    return 1;
  case TY_SHORT:
    return 2;
  case TY_INT:
  case TY_ENUM:
    return 4;
  case TY_LONG:
    return 8;
  case TY_PTR:
  case TY_FUNC:
    return 8;
  case TY_ARRAY:
    return type_size(type->u.pa.ptrof) * type->u.pa.length;
  case TY_STRUCT:
  case TY_UNION:
    if (type->u.struct_.info->size < 0)
      calc_struct_size(type->u.struct_.info, type->type == TY_UNION);
    return type->u.struct_.info->size;
  default:
    assert(false);
    return 1;
  }
}

static int align_size(const Type *type) {
  switch (type->type) {
  case TY_VOID:
    return 1;  // ?
  case TY_CHAR:
    return 1;
  case TY_SHORT:
    return 2;
  case TY_INT:
  case TY_ENUM:
    return 4;
  case TY_LONG:
    return 8;
  case TY_PTR:
  case TY_FUNC:
    return 8;
  case TY_ARRAY:
    return align_size(type->u.pa.ptrof);
  case TY_STRUCT:
  case TY_UNION:
    ensure_struct((Type*)type, NULL);
    if (type->u.struct_.info->size < 0)
      calc_struct_size(type->u.struct_.info, type->type == TY_UNION);
    return type->u.struct_.info->align;
  default:
    assert(false);
    return 1;
  }
}

static void calc_struct_size(StructInfo *sinfo, bool is_union) {
  int size = 0;
  int maxsize = 0;
  int max_align = 1;

  for (int i = 0, len = sinfo->members->len; i < len; ++i) {
    VarInfo *varinfo = (VarInfo*)sinfo->members->data[i];
    int sz = type_size(varinfo->type);
    int align = align_size(varinfo->type);
    size = ALIGN(size, align);
    varinfo->offset = size;
    if (!is_union)
      size += sz;
    else
      if (maxsize < sz)
        maxsize = sz;
    if (max_align < align)
      max_align = align;
  }

  if (is_union)
    size = maxsize;
  size = ALIGN(size, max_align);
  sinfo->size = size;
  sinfo->align = max_align;
}

static void cast(const enum eType ltype, const enum eType rtype) {
  if (ltype == rtype)
    return;

  switch (ltype) {
  case TY_CHAR:
    switch (rtype) {
    case TY_SHORT: return;
    case TY_INT:   return;
    case TY_LONG:  return;
    default: break;
    }
    break;
  case TY_SHORT:
    switch (rtype) {
    case TY_CHAR: MOVSX_AL_AX(); return;
    case TY_INT:  return;
    case TY_LONG: return;
    default: break;
    }
    break;
  case TY_INT:
    switch (rtype) {
    case TY_CHAR:  MOVSX_AL_EAX(); return;
    case TY_SHORT: MOVSX_AX_EAX(); return;
    case TY_ENUM:  return;
    case TY_LONG:  return;
    default: break;
    }
    break;
  case TY_LONG:
    switch (rtype) {
    case TY_CHAR:  MOVSX_AL_RAX(); return;
    case TY_SHORT: MOVSX_AX_RAX(); return;
    case TY_INT:   MOVSX_EAX_RAX(); return;
    case TY_PTR:   return;
    default: break;
    }
    break;
  case TY_ENUM:
    switch (rtype) {
    case TY_INT: return;
    case TY_LONG: return;
    default: break;
    }
    break;
  case TY_PTR:
    switch (rtype) {
    case TY_INT:   MOVSX_EAX_RAX(); return;
    case TY_LONG:  return;
    case TY_ARRAY: return;
    default: break;
    }
    break;
  default:
    break;
  }

  fprintf(stderr, "ltype=%d, rtype=%d\n", ltype, rtype);
  assert(false);
}

static Map *label_map;

enum LocType {
  LOC_REL8,
  LOC_REL32,
  LOC_ABS64,
};

typedef struct {
  enum LocType type;
  uintptr_t ip;
  const char *label;
  union {
    struct {
      uintptr_t base;
    } rel;
  };
} LocInfo;

static Vector *rodata_vector;

void add_rodata(const char *label, const void *data, size_t size) {
  RoData *ro = malloc(sizeof(*ro));
  ro->label = label;
  ro->data = data;
  ro->size = size;
  vec_push(rodata_vector, ro);
}

static uintptr_t start_address;
static unsigned char* code;
static size_t codesize;

void add_code(const unsigned char* buf, size_t size) {
  size_t newsize = codesize + size;
  code = realloc(code, newsize);
  if (code == NULL)
    error("not enough memory");
  memcpy(code + codesize, buf, size);
  codesize = newsize;
}

// Put label at the current.
void add_label(const char *label) {
  map_put(label_map, label, (void*)CURIP(0));
}

uintptr_t label_adr(const char *label) {
  void *adr = map_get(label_map, label);
  return adr != NULL ? (uintptr_t)adr : (uintptr_t)-1;
}

static char *alloc_label(void) {
  static int label_no;
  ++label_no;
  char buf[sizeof(int) * 3 + 1];
  snprintf(buf, sizeof(buf), ".L%d", label_no);
  return strdup_(buf);
}

Vector *loc_vector;

static LocInfo *new_loc(enum LocType type, uintptr_t ip, const char *label) {
  LocInfo *loc = malloc(sizeof(*loc));
  loc->type = type;
  loc->ip = ip;
  loc->label = label;
  vec_push(loc_vector, loc);
  return loc;
}

void add_loc_rel8(const char *label, int ofs, int baseofs) {
  uintptr_t ip = codesize + ofs;
  uintptr_t base = CURIP(baseofs);
  LocInfo *loc = new_loc(LOC_REL8, ip, label);
  loc->rel.base = base;
}

void add_loc_rel32(const char *label, int ofs, int baseofs) {
  uintptr_t ip = codesize + ofs;
  uintptr_t base = CURIP(baseofs);
  LocInfo *loc = new_loc(LOC_REL32, ip, label);
  loc->rel.base = base;
}

void add_loc_abs64(const char *label, uintptr_t pos) {
  new_loc(LOC_ABS64, pos, label);
}

// Put RoData into code.
static void put_rodata(void) {
  for (int i = 0, len = rodata_vector->len; i < len; ++i) {
    const RoData *ro = (const RoData*)rodata_vector->data[i];
    add_label(ro->label);
    add_code(ro->data, ro->size);
  }
}

void construct_initial_value(unsigned char *buf, const Type *type, Initializer *init, Vector **pptrinits) {
  switch (type->type) {
  case TY_CHAR:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
    {
      if (init->type != vSingle)
        error("initializer type error");
      assert(init->u.single->type == ND_INT);
      intptr_t value = init->u.single->u.value;
      int size = type_size(type);
      for (int i = 0; i < size; ++i)
        buf[i] = value >> (i * 8);  // Little endian
    }
    break;
  case TY_PTR:
    {
      if (init->type != vSingle)
        error("initializer type error");
      assert(init->u.single->type == ND_REF);
      Node *value = init->u.single->u.unary.sub;
      if (!(value->type == ND_VARREF && value->u.varref.global))  // TODO: Initialize with array variable.
        error("Allowed global reference only");

      memset(buf, 0, type_size(type));  // Just in case.
      void **init = malloc(sizeof(void*) * 2);
      init[0] = buf;
      init[1] = (void*)value->u.varref.ident;
      if (*pptrinits == NULL)
        *pptrinits = new_vector();
      vec_push(*pptrinits, init);
    }
    break;
  case TY_STRUCT:
    {
      if (init->type != vMulti)
        error("initializer type error");

      ensure_struct((Type*)type, NULL);
      memset(buf, 0x00, type_size(type));
      const StructInfo *sinfo = type->u.struct_.info;
      int n = sinfo->members->len;
      int m = init->u.multi->len;
      if (n <= 0) {
        if (m > 0)
          parse_error(NULL, "Initializer for empty struct");
        break;
      }
      Initializer **values = malloc(sizeof(Initializer*) * n);
      for (int i = 0; i < n; ++i)
        values[i] = NULL;

      int dst = -1;
      for (int i = 0; i < m; ++i) {
        Initializer *value = init->u.multi->data[i];
        if (value->type == vDot) {
          int idx = var_find(sinfo->members, value->u.dot.name);
          if (idx < 0)
            parse_error(NULL, "`%s' is not member of struct", value->u.dot.name);
          values[idx] = value->u.dot.value;
          dst = idx;
          continue;
        }
        if (++dst >= n)
          break;  // TODO: Check extra.
        values[dst] = value;
      }

      for (int i = 0; i < n; ++i) {
        VarInfo* varinfo = sinfo->members->data[i];
        if (values[i] != NULL) {
          construct_initial_value(buf + varinfo->offset, varinfo->type, values[i], pptrinits);
        }
      }
    }
    break;
  default:
    fprintf(stderr, "Global initial value for type %d not implemented (yet)\n", type->type);
    assert(false);
    break;
  }
}

// Put global with initial value (RwData).
static void put_rwdata(void) {
  unsigned char *buf = NULL;
  size_t bufsize = 0;
  for (int i = 0, len = map_count(global); i < len; ++i) {
    const char *name = (const char *)global->keys->data[i];
    const GlobalVarInfo *varinfo = (const GlobalVarInfo*)global->vals->data[i];
    if (varinfo->type->type == TY_FUNC || varinfo->init == NULL ||
        (varinfo->flag & VF_EXTERN) != 0 ||
        varinfo->type->type == TY_ENUM)
      continue;

    int align = align_size(varinfo->type);
    codesize = ALIGN(codesize, align);
    int size = type_size(varinfo->type);
    if (bufsize < (size_t)size) {
      buf = realloc(buf, size);
      bufsize = size;
    }

    Vector *ptrinits = NULL;
    construct_initial_value(buf, varinfo->type, varinfo->init, &ptrinits);

    if (ptrinits != NULL) {
      for (int i = 0; i < ptrinits->len; ++i) {
        void **pp = (void**)ptrinits->data[i];
        add_loc_abs64((char*)pp[1], (unsigned char*)pp[0] - buf + codesize);
      }
    }

    add_label(name);
    add_code(buf, size);
  }
}

// Put global without initial value (bss).
static void put_bss(void) {
  unsigned char *buf = NULL;
  size_t bufsize = 0;
  for (int i = 0, len = map_count(global); i < len; ++i) {
    const char *name = (const char *)global->keys->data[i];
    const GlobalVarInfo *varinfo = (const GlobalVarInfo*)global->vals->data[i];
    if (varinfo->type->type == TY_FUNC || varinfo->init != NULL ||
        (varinfo->flag & VF_EXTERN) != 0)
      continue;
    int align = align_size(varinfo->type);
    codesize = ALIGN(codesize, align);
    int size = type_size(varinfo->type);
    if (size < 1)
      size = 1;
    if (bufsize < (size_t)size) {
      buf = realloc(buf, size);
      memset(buf + bufsize, 0x00, size - bufsize);
      bufsize = size;
    }
    add_label(name);
    add_code(buf, size);
  }
}

// Resolve label locations.
static void resolve_label_locations(void) {
  for (int i = 0; i < loc_vector->len; ++i) {
    LocInfo *loc = loc_vector->data[i];
    void *val = map_get(label_map, loc->label);
    if (val == NULL) {
      error("Cannot find label: `%s'", loc->label);
      continue;
    }

    intptr_t v = (intptr_t)val;
    switch (loc->type) {
    case LOC_REL8:
      {
        intptr_t d = v - loc->rel.base;
        // TODO: Check out of range
        code[loc->ip] = d;
      }
      break;
    case LOC_REL32:
      {
        intptr_t d = v - loc->rel.base;
        // TODO: Check out of range
        for (int i = 0; i < 4; ++i)
          code[loc->ip + i] = d >> (i * 8);
      }
      break;
    case LOC_ABS64:
      for (int i = 0; i < 8; ++i)
        code[loc->ip + i] = v >> (i * 8);
      break;
    default:
      assert(false);
      break;
    }
  }
}

size_t fixup_locations(size_t *pmemsz) {
  put_rodata();
  put_rwdata();

  size_t filesize = codesize;

  put_bss();

  resolve_label_locations();

  *pmemsz = codesize;
  return filesize;
}

//

typedef struct LoopInfo {
  struct LoopInfo *outer;
  const char *l_break;
  const char *l_continue;
} LoopInfo;

static Defun *curfunc;
static Scope *curscope;
static const char *s_break_label;
static const char *s_continue_label;

static const char *push_break_label(const char **save) {
  *save = s_break_label;
  return s_break_label = alloc_label();
}

static void pop_break_label(const char *save) {
  s_break_label = save;
}

static const char *push_continue_label(const char **save) {
  *save = s_continue_label;
  return s_continue_label = alloc_label();
}

static void pop_continue_label(const char *save) {
  s_continue_label = save;
}

static void gen_lval(Node *node);

static void gen_rval(Node *node) {
  gen(node);  // ?
}

static void gen_ref(Node *node) {
  gen_lval(node);
}

static void gen_lval(Node *node) {
  switch (node->type) {
  case ND_VARREF:
    if (node->u.varref.global) {
      LEA_OFS32_RIP_RAX(node->u.varref.ident);
    } else {
      VarInfo *varinfo = scope_find(curscope, node->u.varref.ident);
      assert(varinfo != NULL);
      int offset = varinfo->offset;
      MOV_RBP_RAX();
      ADD_IM32_RAX(offset);
    }
    break;
  case ND_DEREF:
    gen_rval(node->u.unary.sub);
    break;
  case ND_MEMBER:
    {
      const Type *type = node->u.member.target->expType;
      if (type->type == TY_PTR || type->type == TY_ARRAY)
        type = type->u.pa.ptrof;
      assert(type->type == TY_STRUCT || type->type == TY_UNION);
      calc_struct_size(type->u.struct_.info, type->type == TY_UNION);
      Vector *members = type->u.struct_.info->members;
      int varidx = var_find(members, node->u.member.name);
      assert(varidx >= 0);
      VarInfo *varinfo = (VarInfo*)members->data[varidx];

      if (node->u.member.target->expType->type == TY_PTR)
        gen(node->u.member.target);
      else
        gen_ref(node->u.member.target);
      if (varinfo->offset != 0)
        ADD_IM32_RAX(varinfo->offset);
    }
    break;
  default:
    error("No lvalue: %d", node->type);
    break;
  }
}

static void gen_cond_jmp(Node *cond, bool tf, const char *label) {
  gen(cond);

  switch (cond->expType->type) {
  case TY_CHAR: CMP_IM8_AL(0); break;
  case TY_INT:  CMP_IM8_EAX(0); break;
  case TY_PTR:  CMP_IM8_RAX(0); break;
  default: assert(false); break;
  }

  if (tf)
    JNE32(label);
  else
    JE32(label);
}

static void gen_varref(Node *node) {
  gen_lval(node);
  switch (node->expType->type) {
  case TY_CHAR:  MOV_IND_RAX_AL(); break;
  case TY_SHORT: MOV_IND_RAX_AX(); break;
  case TY_INT:   MOV_IND_RAX_EAX(); break;
  case TY_LONG:  MOV_IND_RAX_RAX(); break;
  case TY_ENUM:  MOV_IND_RAX_EAX(); break;
  case TY_PTR:   MOV_IND_RAX_RAX(); break;
  case TY_ARRAY: break;  // Use variable address as a pointer.
  default: assert(false); break;
  }
}

static void gen_defun(Node *node) {
  Defun *defun = node->u.defun;
  add_label(defun->name);
  if (defun->stmts == NULL) {
    RET();
    return;
  }

  curfunc = defun;
  curscope = defun->top_scope;
  defun->ret_label = alloc_label();

  // Calc local variable offsets.
  // Map parameters from the bottom (to reduce offsets).
  int frame_size = 0;
  for (int i = 0; i < defun->all_scopes->len; ++i) {
    Scope *scope = (Scope*)defun->all_scopes->data[i];
    if (scope->vars == NULL)
      continue;
    int scope_size = scope->parent != NULL ? scope->parent->size : 0;
    for (int j = 0; j < scope->vars->len; ++j) {
      VarInfo *varinfo = (VarInfo*)scope->vars->data[j];
      int size = type_size(varinfo->type);
      int align = align_size(varinfo->type);
      if (size < 1)
        size = 1;
      scope_size = ALIGN(scope_size + size, align);
      varinfo->offset = -scope_size;
    }
    scope->size = scope_size;
    if (frame_size < scope_size)
      frame_size = scope_size;
  }
  frame_size = ALIGN(frame_size, FRAME_ALIGN);

  // Prologue
  // Allocate variable bufer.
  PUSH_RBP();
  MOV_RSP_RBP();
  if (frame_size > 0)
    SUB_IM32_RSP(frame_size);

  // Store arguments into local frame.
  int len = len = defun->params != NULL ? defun->params->len : 0;
  if (len > 6)
    error("Parameter count exceeds 6 (%d)", len);
  for (int i = 0; i < len; ++i) {
    const VarInfo *varinfo = (const VarInfo*)defun->params->data[i];
    int offset = varinfo->offset;
    switch (varinfo->type->type) {
    case TY_CHAR:  // 1
      switch (i) {
      case 0:  MOV_DIL_IND8_RBP(offset); break;
      case 1:  MOV_SIL_IND8_RBP(offset); break;
      case 2:  MOV_DL_IND8_RBP(offset); break;
      case 3:  MOV_CL_IND8_RBP(offset); break;
      case 4:  MOV_R8B_IND8_RBP(offset); break;
      case 5:  MOV_R9B_IND8_RBP(offset); break;
      default: break;
      }
      break;
    case TY_INT:
    case TY_ENUM:
      // 4
      switch (i) {
      case 0:  MOV_EDI_IND8_RBP(offset); break;
      case 1:  MOV_ESI_IND8_RBP(offset); break;
      case 2:  MOV_EDX_IND8_RBP(offset); break;
      case 3:  MOV_ECX_IND8_RBP(offset); break;
      case 4:  MOV_R8D_IND8_RBP(offset); break;
      case 5:  MOV_R9D_IND8_RBP(offset); break;
      default: break;
      }
      break;
    case TY_LONG:
    case TY_PTR:
      // 8
      switch (i) {
      case 0:  MOV_RDI_IND8_RBP(offset); break;
      case 1:  MOV_RSI_IND8_RBP(offset); break;
      case 2:  MOV_RDX_IND8_RBP(offset); break;
      case 3:  MOV_RCX_IND8_RBP(offset); break;
      case 4:  MOV_R8_IND8_RBP(offset); break;
      case 5:  MOV_R9_IND8_RBP(offset); break;
      default: break;
      }
      break;
    default:
      assert(false);
      break;
    }
  }

  // Statements
  for (int i = 0; i < defun->stmts->len; ++i) {
    gen((Node*)defun->stmts->data[i]);
  }

  // Epilogue
  add_label(defun->ret_label);
  MOV_RBP_RSP();
  POP_RBP();
  RET();
  curfunc = NULL;
  curscope = NULL;
}

static void gen_return(Node *node) {
  if (node->u.return_.val != NULL)
    gen(node->u.return_.val);
  assert(curfunc != NULL);
  JMP32(curfunc->ret_label);
}

static void gen_funcall(Node *node) {
  Vector *args = node->u.funcall.args;
  if (args != NULL) {
    int len = args->len;
    if (len > 6)
      error("Param count exceeds 6 (%d)", len);

    for (int i = 0; i < len; ++i) {
      gen((Node*)args->data[i]);
      PUSH_RAX();
    }

    switch (len) {
    case 6:  POP_R9();  // Fallthrough
    case 5:  POP_R8();  // Fallthrough
    case 4:  POP_RCX();  // Fallthrough
    case 3:  POP_RDX();  // Fallthrough
    case 2:  POP_RSI();  // Fallthrough
    case 1:  POP_RDI();  // Fallthrough
    default: break;
    }
  }
  Node *func = node->u.funcall.func;
  if (func->type == ND_VARREF && func->u.varref.global) {
    CALL(func->u.varref.ident);
  } else {
    gen(func);
    CALL_IND_RAX();
  }
}

static void gen_if(Node *node) {
  const char *flabel = alloc_label();
  gen_cond_jmp(node->u.if_.cond, false, flabel);
  gen(node->u.if_.tblock);
  if (node->u.if_.fblock == NULL) {
    add_label(flabel);
  } else {
    const char *nlabel = alloc_label();
    JMP32(nlabel);
    add_label(flabel);
    gen(node->u.if_.fblock);
    add_label(nlabel);
  }
}

static Vector *cur_case_values;
static Vector *cur_case_labels;

static void gen_switch(Node *node) {
  Vector *save_case_values = cur_case_values;
  Vector *save_case_labels = cur_case_labels;
  const char *save_break;
  const char *l_break = push_break_label(&save_break);

  Vector *labels = new_vector();
  Vector *case_values = node->u.switch_.case_values;
  int len = case_values->len;
  for (int i = 0; i < len; ++i) {
    const char *label = alloc_label();
    vec_push(labels, label);
  }
  vec_push(labels, alloc_label());  // len+0: Extra label for default.
  vec_push(labels, l_break);  // len+1: Extra label for break.

  Node *value = node->u.switch_.value;
  gen(value);

  enum eType valtype = value->expType->type;
  for (int i = 0; i < len; ++i) {
    intptr_t x = (intptr_t)case_values->data[i];
    switch (valtype) {
    case TY_INT:  CMP_IM32_EAX(x); break;
    case TY_CHAR: CMP_IM8_AL(x); break;
    case TY_LONG: MOV_IM64_RDI(x); CMP_RDI_RAX(); break;
    default: assert(false); break;
    }
    JE32(labels->data[i]);
  }
  JMP32(labels->data[len]);

  cur_case_values = case_values;
  cur_case_labels = labels;

  gen(node->u.switch_.body);

  if (!node->u.switch_.has_default)
    add_label(labels->data[len]);  // No default: Locate at the end of switch statement.
  add_label(l_break);

  cur_case_values = save_case_values;
  cur_case_labels = save_case_labels;
  pop_break_label(save_break);
}

static void gen_label(Node *node) {
  switch (node->u.label.type) {
  case lCASE:
    {
      assert(cur_case_values != NULL);
      assert(cur_case_labels != NULL);
      intptr_t x = node->u.label.u.case_value;
      int i, len = cur_case_values->len;
      for (i = 0; i < len; ++i) {
        if ((intptr_t)cur_case_values->data[i] == x)
          break;
      }
      assert(i < len);
      assert(i < cur_case_labels->len);
      add_label(cur_case_labels->data[i]);
    }
    break;
  case lDEFAULT:
    {
      assert(cur_case_values != NULL);
      assert(cur_case_labels != NULL);
      int i = cur_case_values->len;  // Label for default is stored at the size of values.
      assert(i < cur_case_labels->len);
      add_label(cur_case_labels->data[i]);
    }
    break;
  default: assert(false); break;
  }
}

static void gen_while(Node *node) {
  const char *save_break, *save_cont;
  const char *l_cond = push_continue_label(&save_cont);
  const char *l_break = push_break_label(&save_break);
  const char *l_loop = alloc_label();
  JMP32(l_cond);
  add_label(l_loop);
  gen(node->u.while_.body);
  add_label(l_cond);
  gen_cond_jmp(node->u.while_.cond, true, l_loop);
  add_label(l_break);
  pop_continue_label(save_cont);
  pop_break_label(save_break);
}

static void gen_do_while(Node *node) {
  const char *save_break, *save_cont;
  const char *l_cond = push_continue_label(&save_cont);
  const char *l_break = push_break_label(&save_break);
  const char * l_loop = alloc_label();
  add_label(l_loop);
  gen(node->u.do_while.body);
  add_label(l_cond);
  gen_cond_jmp(node->u.do_while.cond, true, l_loop);
  add_label(l_break);
  pop_continue_label(save_cont);
  pop_break_label(save_break);
}

static void gen_for(Node *node) {
  const char *save_break, *save_cont;
  const char *l_continue = push_continue_label(&save_cont);
  const char *l_break = push_break_label(&save_break);
  const char * l_cond = alloc_label();
  if (node->u.for_.pre != NULL)
    gen(node->u.for_.pre);
  add_label(l_cond);
  if (node->u.for_.cond != NULL) {
    gen_cond_jmp(node->u.for_.cond, false, l_break);
  }
  gen(node->u.for_.body);
  add_label(l_continue);
  if (node->u.for_.post != NULL)
    gen(node->u.for_.post);
  JMP32(l_cond);
  add_label(l_break);
  pop_continue_label(save_cont);
  pop_break_label(save_break);
}

static void gen_break(void) {
  assert(s_break_label != NULL);
  JMP32(s_break_label);
}

static void gen_continue(void) {
  assert(s_continue_label != NULL);
  JMP32(s_continue_label);
}

static void gen_arith(enum NodeType nodeType, enum eType expType, enum eType rhsType) {
  // lhs=rax, rhs=rdi, result=rax

  switch (nodeType) {
  case ND_ADD:
    switch (expType) {
    case TY_CHAR: ADD_DIL_AL(); break;
    case TY_INT:  ADD_EDI_EAX(); break;
    case TY_LONG: ADD_RDI_RAX(); break;
    default: assert(false); break;
    }
    break;

  case ND_SUB:
    switch (expType) {
    case TY_CHAR: SUB_DIL_AL(); break;
    case TY_INT:  SUB_EDI_EAX(); break;
    case TY_LONG: SUB_RDI_RAX(); break;
    default: assert(false); break;
    }
    break;

  case ND_MUL:
    switch (expType) {
    case TY_CHAR: MUL_DIL(); break;
    case TY_INT:  MUL_EDI(); break;
    case TY_LONG: MUL_RDI(); break;
    default: assert(false); break;
    }

    break;

  case ND_DIV:
    MOV_IM32_RDX(0);
    switch (expType) {
    case TY_CHAR: DIV_DIL(); break;
    case TY_INT:  DIV_EDI(); break;
    case TY_LONG: DIV_RDI(); break;
    default: assert(false); break;
    }
    break;

  case ND_MOD:
    MOV_IM32_RDX(0);
    switch (expType) {
    case TY_CHAR: DIV_DIL(); MOV_DL_AL(); break;
    case TY_INT:  DIV_EDI(); MOV_EDX_EAX(); break;
    case TY_LONG: DIV_RDI(); MOV_RDX_RAX(); break;
    default: assert(false); break;
    }
    break;

  case ND_BITAND:
    switch (expType) {
    case TY_CHAR: AND_DIL_AL(); break;
    case TY_INT:  AND_EDI_EAX(); break;
    case TY_LONG: AND_RDI_RAX(); break;
    default: assert(false); break;
    }
    break;

  case ND_BITOR:
    switch (expType) {
    case TY_CHAR: OR_DIL_AL(); break;
    case TY_INT:  OR_EDI_EAX(); break;
    case TY_LONG: OR_RDI_RAX(); break;
    default: assert(false); break;
    }
    break;

  case ND_BITXOR:
    switch (expType) {
    case TY_CHAR: XOR_DIL_AL(); break;
    case TY_INT:  XOR_EDI_EAX(); break;
    case TY_LONG: XOR_RDI_RAX(); break;
    default: assert(false); break;
    }
    break;

  case ND_LSHIFT:
  case ND_RSHIFT:
    switch (rhsType) {
    case TY_CHAR: MOV_DIL_CL(); break;
    case TY_INT:  MOV_EDI_ECX(); break;
    case TY_LONG: MOV_RDI_RCX(); break;
    default: assert(false); break;
    }
    if (nodeType == ND_LSHIFT) {
      switch (expType) {
      case TY_CHAR: SHL_CL_AL(); break;
      case TY_INT:  SHL_CL_EAX(); break;
      case TY_LONG: SHL_CL_RAX(); break;
      default: assert(false); break;
      }
    } else {
      switch (expType) {
      case TY_CHAR: SHR_CL_AL(); break;
      case TY_INT:  SHR_CL_EAX(); break;
      case TY_LONG: SHR_CL_RAX(); break;
      default: assert(false); break;
      }
    }
    break;

  default:
    assert(false);
    break;
  }
}

void gen(Node *node) {
  switch (node->type) {
  case ND_INT:
    MOV_IM32_EAX(node->u.value);
    return;

  case ND_CHAR:
    MOV_IM8_AL(node->u.value);
    return;

  case ND_LONG:
    if (node->u.value < 0x7fffffffL && node->u.value >= -0x80000000L)
      MOV_IM32_RAX(node->u.value);
    else
      MOV_IM64_RAX(node->u.value);
    return;

  case ND_SIZEOF:
    {
      size_t size = type_size(node->u.sizeof_.type);
      if (size < 0x7fffffffL)
        MOV_IM32_RAX(size);
      else
        MOV_IM64_RAX(size);
    }
    return;

  case ND_STR:
    {
      const char * label = alloc_label();
      add_rodata(label, node->u.str.buf, node->u.str.len);
      LEA_OFS32_RIP_RAX(label);
    }
    return;

  case ND_VARREF:
    gen_varref(node);
    return;

  case ND_REF:
    gen_ref(node->u.unary.sub);
    return;

  case ND_DEREF:
    gen_rval(node->u.unary.sub);
    switch (node->expType->type) {
    case TY_CHAR:  MOV_IND_RAX_AL(); break;
    case TY_INT:   MOV_IND_RAX_EAX(); break;
    case TY_PTR:   MOV_IND_RAX_RAX(); break;
    case TY_ARRAY: break;
    default: assert(false); break;
    }
    return;

  case ND_MEMBER:
    gen_lval(node);
    switch (node->expType->type) {
    case TY_CHAR:
      MOV_IND_RAX_AL();
      break;
    case TY_INT:
      MOV_IND_RAX_EAX();
      break;
    case TY_PTR:
      MOV_IND_RAX_RAX();
      break;
    case TY_ARRAY:
      break;
    default:
      assert(false);
      break;
    }
    return;

  case ND_CAST:
    gen(node->u.cast.sub);
    cast(node->expType->type, node->u.cast.sub->expType->type);
    break;

  case ND_ASSIGN:
    gen_lval(node->u.bop.lhs);
    PUSH_RAX();
    gen(node->u.bop.rhs);

    POP_RDI();
    switch (node->u.bop.lhs->expType->type) {
    case TY_CHAR:  MOV_AL_IND_RDI(); break;
    case TY_INT:   MOV_EAX_IND_RDI(); break;
    default:
    case TY_PTR:   MOV_RAX_IND_RDI(); break;
    }
    return;

  case ND_ASSIGN_WITH:
    {
      Node *sub = node->u.unary.sub;
      gen(sub->u.bop.rhs);
      PUSH_RAX();
      gen_lval(sub->u.bop.lhs);
      MOV_RAX_RSI();  // Save lhs address to %rsi.

      // Move lhs to %?ax
      switch (node->u.bop.lhs->expType->type) {
      case TY_CHAR:  MOV_IND_RAX_AL(); break;
      case TY_INT:   MOV_IND_RAX_EAX(); break;
      default:
      case TY_PTR:   MOV_IND_RAX_RAX(); break;
      }

      POP_RDI();  // %rdi=rhs
      gen_arith(sub->type, sub->expType->type, sub->u.bop.rhs->expType->type);
      cast(node->expType->type, sub->expType->type);

      switch (node->expType->type) {
      case TY_CHAR:  MOV_AL_IND_RSI(); break;
      case TY_INT:   MOV_EAX_IND_RSI(); break;
      default:
      case TY_PTR:   MOV_RAX_IND_RSI(); break;
      }
    }
    return;

  case ND_PREINC:
  case ND_PREDEC:
    gen_lval(node->u.unary.sub);
    switch (node->expType->type) {
    case TY_CHAR:
      if (node->type == ND_PREINC)  INCB_IND_RAX();
      else                          DECB_IND_RAX();
      MOV_IND_RAX_RAX();
      break;
    case TY_INT:
      if (node->type == ND_PREINC)  INCL_IND_RAX();
      else                          DECL_IND_RAX();
      MOV_IND_RAX_RAX();
      break;
    case TY_PTR:
      {
        MOV_RAX_RDI();
        int size = type_size(node->expType->u.pa.ptrof);
        MOV_IM32_RAX(node->type == ND_PREINC ? size : -size);
        ADD_IND_RDI_RAX();
        MOV_RAX_IND_RDI();
      }
      break;
    default:
      assert(false);
      break;
    }
    return;

  case ND_POSTINC:
  case ND_POSTDEC:
    gen_lval(node->u.unary.sub);
    MOV_IND_RAX_RDI();
    switch (node->expType->type) {
    case TY_CHAR:
      if (node->type == ND_POSTINC)  INCB_IND_RAX();
      else                           DECB_IND_RAX();
      break;
    case TY_INT:
      if (node->type == ND_POSTINC)  INCL_IND_RAX();
      else                           DECL_IND_RAX();
      break;
    case TY_PTR:
      {
        int size = type_size(node->expType->u.pa.ptrof);
        if (node->type == ND_POSTINC)  ADD_IM32_RAX(size);
        else                           SUB_IM32_RAX(size);
      }
      break;
    default:
      assert(false);
      break;
    }
    MOV_RDI_RAX();
    return;

  case ND_DEFUN:
    gen_defun(node);
    return;

  case ND_RETURN:
    gen_return(node);
    return;

  case ND_FUNCALL:
    gen_funcall(node);
    return;

  case ND_BLOCK:
    if (node->u.block.nodes != NULL) {
      if (node->u.block.scope != NULL) {
        assert(curscope == node->u.block.scope->parent);
        curscope = node->u.block.scope;
      }
      for (int i = 0, len = node->u.block.nodes->len; i < len; ++i)
        gen((Node*)node->u.block.nodes->data[i]);
      if (node->u.block.scope != NULL)
        curscope = curscope->parent;
    }
    break;

  case ND_IF:
    gen_if(node);
    break;

  case ND_SWITCH:
    gen_switch(node);
    break;

  case ND_LABEL:
    gen_label(node);
    break;

  case ND_WHILE:
    gen_while(node);
    break;

  case ND_DO_WHILE:
    gen_do_while(node);
    break;

  case ND_FOR:
    gen_for(node);
    break;

  case ND_BREAK:
    gen_break();
    break;

  case ND_CONTINUE:
    gen_continue();
    break;

  case ND_NEG:
    gen(node->u.unary.sub);
    switch (node->expType->type) {
    case TY_CHAR: NEG_AL(); break;
    case TY_INT:  NEG_EAX(); break;
    case TY_LONG: NEG_RAX(); break;
    default:  assert(false); break;
    }
    break;

  case ND_NOT:
    gen(node->u.unary.sub);
    switch (node->expType->type) {
    case TY_INT:  CMP_IM8_EAX(0); break;
    case TY_CHAR: CMP_IM8_AL(0); break;
    case TY_PTR:  CMP_IM8_RAX(0); break;
    default:  assert(false); break;
    }
    SETE_AL();
    MOVZX_AL_EAX();
    break;

  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_GT:
  case ND_LE:
  case ND_GE:
    {
      enum NodeType type = node->type;
      Node *lhs = node->u.bop.lhs;
      Node *rhs = node->u.bop.rhs;
      if (type == ND_LE || type == ND_GT) {
        Node *tmp = lhs; lhs = rhs; rhs = tmp;
        type = type == ND_LE ? ND_GE : ND_LT;
      }

      gen(lhs);
      PUSH_RAX();
      gen(rhs);

      POP_RDI();
      switch (lhs->expType->type) {
      case TY_CHAR: CMP_AL_DIL(); break;
      case TY_INT:  CMP_EAX_EDI(); break;
      case TY_LONG: CMP_RAX_RDI(); break;
      case TY_PTR:  CMP_RAX_RDI(); break;
      default: assert(false); break;
      }

      switch (type) {
      case ND_EQ:  SETE_AL(); break;
      case ND_NE:  SETNE_AL(); break;
      case ND_LT:  SETS_AL(); break;
      case ND_GE:  SETNS_AL(); break;
      default: assert(false); break;
      }
    }
    MOVZX_AL_EAX();
    return;

  case ND_LOGAND:
    {
      const char * l_false = alloc_label();
      const char * l_true = alloc_label();
      const char * l_next = alloc_label();
      gen_cond_jmp(node->u.bop.lhs, false, l_false);
      gen_cond_jmp(node->u.bop.rhs, true, l_true);
      add_label(l_false);
      MOV_IM32_EAX(0);
      JMP8(l_next);
      add_label(l_true);
      MOV_IM32_EAX(1);
      add_label(l_next);
    }
    return;

  case ND_LOGIOR:
    {
      const char * l_false = alloc_label();
      const char * l_true = alloc_label();
      const char * l_next = alloc_label();
      gen_cond_jmp(node->u.bop.lhs, true, l_true);
      gen_cond_jmp(node->u.bop.rhs, false, l_false);
      add_label(l_true);
      MOV_IM32_EAX(1);
      JMP8(l_next);
      add_label(l_false);
      MOV_IM32_EAX(0);
      add_label(l_next);
    }
    return;

  case ND_PTRADD:
    {
      Node *lhs = node->u.bop.lhs, *rhs = node->u.bop.rhs;
      gen(rhs);
      cast(TY_INT, rhs->expType->type);  // TODO: Fix
      long size = type_size(lhs->expType->u.pa.ptrof);
      if (size != 1) {
        MOV_IM32_EDI(size);
        MUL_EDI();
      }
      PUSH_RAX();
      gen(lhs);
      POP_RDI();
      ADD_RDI_RAX();
      break;
    }
    return;

  case ND_PTRSUB:
    {
      Node *lhs = node->u.bop.lhs, *rhs = node->u.bop.rhs;
      gen(rhs);
      cast(TY_INT, rhs->expType->type);  // TODO: Fix
      int size = type_size(node->u.bop.lhs->expType->u.pa.ptrof);
      if (size != 1) {
        MOV_IM64_RDI((long)size);
        MUL_RDI();
      }
      PUSH_RAX();
      gen(lhs);
      POP_RDI();
      SUB_RDI_RAX();
    }
    return;

  case ND_PTRDIFF:
    {
      gen(node->u.bop.rhs);
      PUSH_RAX();
      gen(node->u.bop.lhs);
      POP_RDI();
      SUB_RDI_RAX();

      int size = type_size(node->u.bop.lhs->expType->u.pa.ptrof);
      switch (size) {
      case 1:  break;
      case 2:  SAR_RAX(); break;
      case 4:  SAR_IM8_RAX(2); break;
      case 8:  SAR_IM8_RAX(3); break;
      default:
        MOV_IM64_RDI((long)size);
        MOV_IM32_RDX(0);
        DIV_RDI();
        break;
      }
    }
    return;

  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_LSHIFT:
  case ND_RSHIFT:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
    gen(node->u.bop.rhs);
    PUSH_RAX();
    gen(node->u.bop.lhs);

    POP_RDI();

    gen_arith(node->type, node->expType->type, node->u.bop.rhs->expType->type);
    return;

  default:
    error("Unhandled node: %d", node->type);
    break;
  }
}

void init_gen(uintptr_t start_address_) {
  start_address = start_address_;
  label_map = new_map();
  rodata_vector = new_vector();
}

void output_code(FILE* fp, size_t filesize) {
  fwrite(code, filesize, 1, fp);
}
