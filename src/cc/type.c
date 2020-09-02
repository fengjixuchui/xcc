#include "type.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "util.h"

const Type tyChar =          {.kind=TY_NUM, .num={.kind=NUM_CHAR,  .is_unsigned=false}};
const Type tyShort =         {.kind=TY_NUM, .num={.kind=NUM_SHORT, .is_unsigned=false}};
const Type tyInt =           {.kind=TY_NUM, .num={.kind=NUM_INT,   .is_unsigned=false}};
const Type tyLong =          {.kind=TY_NUM, .num={.kind=NUM_LONG,  .is_unsigned=false}};
const Type tyUnsignedChar =  {.kind=TY_NUM, .num={.kind=NUM_CHAR,  .is_unsigned=true}};
const Type tyUnsignedShort = {.kind=TY_NUM, .num={.kind=NUM_SHORT, .is_unsigned=true}};
const Type tyUnsignedInt =   {.kind=TY_NUM, .num={.kind=NUM_INT,   .is_unsigned=true}};
const Type tyUnsignedLong =  {.kind=TY_NUM, .num={.kind=NUM_LONG,  .is_unsigned=true}};
const Type tyEnum =          {.kind=TY_NUM, .num={.kind=NUM_ENUM}};
const Type tyVoid =          {.kind=TY_VOID};
const Type tyVoidPtr =       {.kind=TY_PTR, .pa={.ptrof=&tyVoid}};

bool is_number(enum TypeKind kind) {
  return kind == TY_NUM;
}

bool is_char_type(const Type *type) {
  return type->kind == TY_NUM && type->num.kind == NUM_CHAR;
}

bool is_void_ptr(const Type *type) {
  return type->kind == TY_PTR && type->pa.ptrof->kind == TY_VOID;
}

bool ptr_or_array(const Type *type) {
  return type->kind == TY_PTR || type->kind == TY_ARRAY;
}

bool same_type(const Type *type1, const Type *type2) {
  for (;;) {
    if (type1->kind != type2->kind)
      return false;

    switch (type1->kind) {
    case TY_VOID:
      return true;
    case TY_NUM:
      return type1->num.kind == type2->num.kind;
    case TY_ARRAY:
      if (type1->pa.length != type2->pa.length)
        return false;
      // Fallthrough
    case TY_PTR:
      type1 = type1->pa.ptrof;
      type2 = type2->pa.ptrof;
      continue;
    case TY_FUNC:
      if (!same_type(type1->func.ret, type2->func.ret) || type1->func.vaargs != type2->func.vaargs)
        return false;
      if (type1->func.param_types == NULL && type2->func.param_types == NULL)
        return true;
      if (type1->func.param_types == NULL || type2->func.param_types == NULL ||
          type1->func.param_types->len != type2->func.param_types->len)
        return false;
      for (int i = 0, len = type1->func.param_types->len; i < len; ++i) {
        const Type *t1 = (const Type*)type1->func.param_types->data[i];
        const Type *t2 = (const Type*)type2->func.param_types->data[i];
        if (!same_type(t1, t2))
          return false;
      }
      return true;
    case TY_STRUCT:
      {
        if (type1->struct_.info != NULL) {
          if (type2->struct_.info != NULL)
            return type1->struct_.info == type2->struct_.info;
          const Type *tmp = type1;
          type1 = type2;
          type2 = tmp;
        } else if (type2->struct_.info == NULL) {
          return equal_name(type1->struct_.name, type2->struct_.name);
        }
        // Find type1 from name.
        StructInfo *sinfo = find_struct(type1->struct_.name);
        if (sinfo == NULL)
          return false;
        return sinfo == type2->struct_.info;
      }
    }
  }
}

bool can_cast(const Type *dst, const Type *src, bool zero, bool is_explicit) {
  if (same_type(dst, src))
    return true;

  if (dst->kind == TY_VOID)
    return src->kind == TY_VOID || is_explicit;
  if (src->kind == TY_VOID)
    return false;

  switch (dst->kind) {
  case TY_NUM:
    switch (src->kind) {
    case TY_NUM:
      return true;
    case TY_PTR:
    case TY_ARRAY:
    case TY_FUNC:
      if (is_explicit) {
        // TODO: Check sizeof(long) is same as sizeof(ptr)
        return true;
      }
      break;
    default:
      break;
    }
    break;
  case TY_PTR:
    switch (src->kind) {
    case TY_NUM:
      if (zero)  // Special handling for 0 to pointer.
        return true;
      if (is_explicit)
        return true;
      break;
    case TY_PTR:
      if (is_explicit)
        return true;
      // void* is interchangable with any pointer type.
      if (dst->pa.ptrof->kind == TY_VOID || src->pa.ptrof->kind == TY_VOID)
        return true;
      if (src->pa.ptrof->kind == TY_FUNC)
        return can_cast(dst, src->pa.ptrof, zero, is_explicit);
      break;
    case TY_ARRAY:
      if (is_explicit)
        return true;
      if (same_type(dst->pa.ptrof, src->pa.ptrof) ||
          can_cast(dst, ptrof(src->pa.ptrof), zero, is_explicit))
        return true;
      break;
    case TY_FUNC:
      if (is_explicit)
        return true;
      if (dst->pa.ptrof->kind == TY_FUNC) {
        const Type *ftype = dst->pa.ptrof;
        return (same_type(ftype, src) ||
                (ftype->func.param_types == NULL || src->func.param_types == NULL));
      }
      break;
    default:  break;
    }
    break;
  case TY_ARRAY:
    switch (src->kind) {
    case TY_PTR:
      if (is_explicit && same_type(dst->pa.ptrof, src->pa.ptrof))
        return true;
      // Fallthrough
    case TY_ARRAY:
      if (is_explicit)
        return true;
      break;
    default:  break;
    }
    break;
  default:
    break;
  }
  return false;
}

Type *ptrof(const Type *type) {
  Type *ptr = malloc(sizeof(*ptr));
  ptr->kind = TY_PTR;
  ptr->pa.ptrof = type;
  return ptr;
}

const Type *array_to_ptr(const Type *type) {
  if (type->kind != TY_ARRAY)
    return type;
  return ptrof(type->pa.ptrof);
}

Type *arrayof(const Type *type, size_t length) {
  Type *arr = malloc(sizeof(*arr));
  arr->kind = TY_ARRAY;
  arr->pa.ptrof = type;
  arr->pa.length = length;
  return arr;
}

Type *new_func_type(const Type *ret, Vector *params, Vector *param_types, bool vaargs) {
  Type *f = malloc(sizeof(*f));
  f->kind = TY_FUNC;
  f->func.ret = ret;
  f->func.vaargs = vaargs;
  f->func.params = params;
  f->func.param_types = param_types;
  return f;
}

// Struct

Table struct_table;

StructInfo *find_struct(const Name *name) {
  return table_get(&struct_table, name);
}

StructInfo *create_struct(Vector *members, bool is_union) {
  StructInfo *sinfo = malloc(sizeof(*sinfo));
  sinfo->members = members;
  sinfo->is_union = is_union;
  sinfo->size = -1;
  sinfo->align = 0;
  return sinfo;
}

void define_struct(const Name *name, StructInfo *sinfo) {
  table_put(&struct_table, name, sinfo);
}

// Enum

Table enum_table;
Table enum_value_table;

Type *find_enum(const Name *name) {
  return table_get(&enum_table, name);
}

Type *define_enum(const Name *ident) {
  Type *type = malloc(sizeof(*type));
  type->kind = TY_NUM;
  type->num.kind = NUM_ENUM;
  type->num.is_unsigned = false;
  type->num.enum_.ident = ident;
  type->num.enum_.members = new_vector();

  if (ident != NULL) {
    table_put(&enum_table, ident, type);
  }

  return type;
}

void add_enum_member(Type *type, const Name *ident, int value) {
  assert(type->kind == TY_NUM && type->num.kind == NUM_ENUM);
  EnumMember *member = malloc(sizeof(*member));
  member->ident = ident;
  member->value = value;
  vec_push(type->num.enum_.members, member);

  table_put(&enum_value_table, ident, (void*)(intptr_t)value);
}

bool find_enum_value(const Name *name, intptr_t *output) {
  return table_try_get(&enum_value_table, name, (void**)output);
}

#if 0
void dump_type(FILE *fp, const Type *type) {
  switch (type->kind) {
  case TY_VOID: fprintf(fp, "void"); break;
  case TY_NUM:
    switch (type->num.kind) {
    case NUM_CHAR:  fprintf(fp, "char"); break;
    case NUM_SHORT: fprintf(fp, "short"); break;
    case NUM_INT:   fprintf(fp, "int"); break;
    case NUM_LONG:  fprintf(fp, "long"); break;
    case NUM_ENUM:  fprintf(fp, "enum"); break;
    default: assert(false); break;
    }
    break;
  case TY_PTR: dump_type(fp, type->pa.ptrof); fprintf(fp, "*"); break;
  case TY_ARRAY: dump_type(fp, type->pa.ptrof); fprintf(fp, "[%d]", (int)type->pa.length); break;
  default: assert(false); break;
  }
}
#endif

// Typedef

Table typedef_table;  // <const Name*, Type*>

const Type *find_typedef(const Name *name) {
  return table_get(&typedef_table, name);
}

bool add_typedef(const Name *name, const Type *type) {
  if (table_get(&typedef_table, name) != NULL)
    return false;
  table_put(&typedef_table, name, (void*)type);
  return true;
}
