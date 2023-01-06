#include <assert.h>
#include <inttypes.h>  // PRId64
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "table.h"
#include "type.h"
#include "util.h"

static int error_count;

extern void dump_expr(FILE *fp, Expr *expr);

static bool same_expr(const Expr *expr1, const Expr *expr2) {
  if (expr1->kind != expr2->kind)
    return false;

  switch (expr1->kind) {
  case EX_FIXNUM:
    return expr1->fixnum == expr2->fixnum;
  case EX_STR:
    return expr1->str.size == expr2->str.size && memcmp(expr1->str.buf, expr2->str.buf, expr1->str.size) == 0;
  default: assert(false); break;
  }
}

static bool same_init(const Initializer *init1, const Initializer *init2) {
  if (init1 == NULL)
    return init2 == NULL;
  if (init2 == NULL)
    return false;

  if (init1->kind != init2->kind)
    return false;

  switch (init1->kind) {
  case IK_SINGLE:
    return same_expr(init1->single, init2->single);
  case IK_MULTI:
    {
      Vector *m1 = init1->multi, *m2 = init2->multi;
      if (m1->len != m2->len)
        return false;
      for (int i = 0; i < m1->len; ++i) {
        if (!same_init(m1->data[i], m2->data[i]))
          return false;
      }
    }
    return true;
  case IK_ARR:
    return same_expr(init1->arr.index, init2->arr.index) && same_init(init1->arr.value, init2->arr.value);
  default:
    return false;
  }
}

static Initializer *parse_init(const char *source) {
  const char fn[] = "*test*";
  set_source_file(NULL, fn);
  set_source_string(source, fn, 1);
  return parse_initializer();
}

void dump_init(FILE *fp, const Initializer *init) {
  if (init == NULL) {
    fprintf(fp, "NULL");
    return;
  }

  switch (init->kind) {
  case IK_SINGLE:
    dump_expr(fp, init->single);
    break;
  case IK_MULTI:
    {
      Vector *multi = init->multi;
      fprintf(fp, "{#%d:", multi->len);
      for (int i = 0; i < multi->len; ++i) {
        if (i != 0)
          fprintf(fp, ", ");
        dump_init(fp, multi->data[i]);
      }
      fprintf(fp, "}");
    }
    break;
  case IK_ARR:
    assert(init->arr.index->kind == EX_FIXNUM);
    fprintf(fp, "[%" PRId64 "]=", init->arr.index->fixnum);
    dump_init(fp, init->arr.value);
    break;
  default: assert(false); break;
  }
}

void expect(Initializer *expected, const char *input_str, Type *type) {
  printf("%s => ", input_str);

  Initializer *init = parse_init(input_str);
  Initializer *actual = flatten_initializer(type, init);

  // Compare initializer
  if (same_init(expected, actual)) {
    printf("OK\n");
    return;
  }

  fflush(stdout);
  fprintf(stderr, "Fail, expected[");
  dump_init(stderr, expected);
  fprintf(stderr, "], actual[");
  dump_init(stderr, actual);
  fprintf(stderr, "]\n");
  ++error_count;
}

void expect2(const char *expected_str, const char *input_str, Type *type) {
  Initializer *expected = parse_init(expected_str);
  expect(expected, input_str, type);
}

Initializer *new_init_single(Expr *expr) {
  Initializer *init = calloc(1, sizeof(*init));
  init->kind = IK_SINGLE;
  init->single = expr;
  return init;
}

Initializer *new_init_multi(int count, ...) {
  va_list ap;
  va_start(ap, count);
  Vector *elems = new_vector();
  for (int i = 0; i < count; ++i) {
    Initializer *elem = va_arg(ap, Initializer*);
    vec_push(elems, elem);
  }
  va_end(ap);

  Initializer *init = calloc(1, sizeof(*init));
  init->kind = IK_MULTI;
  init->multi = elems;
  return init;
}

Initializer *new_init_arr(int index, Initializer *value) {
  Initializer *init = calloc(1, sizeof(*init));
  init->kind = IK_ARR;
  init->arr.index = new_expr_fixlit(&tyInt, NULL, index);
  init->arr.value = value;
  return init;
}

void test_flatten(void) {
  expect2("1234", "1234", &tyInt);
  expect2("\"str\"", "\"str\"", ptrof(&tyChar));
  expect2("\"array\"", "\"array\"", arrayof(&tyChar, 4));
  expect2("{1, 2, 3}", "{1, 2, 3}", arrayof(&tyInt, -1));
  expect2("{\"str\"}", "{\"str\"}", arrayof(ptrof(&tyChar), -1));

  {  // Struct initializer shortage.
    Vector *members = new_vector();
    add_struct_member(members, alloc_name("x", NULL, false), &tyChar);
    add_struct_member(members, alloc_name("y", NULL, false), get_fixnum_type(FX_SHORT, false, 0));
    add_struct_member(members, alloc_name("z", NULL, false), get_fixnum_type(FX_LONG, true, 0));
    StructInfo *sinfo = create_struct_info(members, false);
    Type *type = create_struct_type(sinfo, NULL, 0);

    Initializer *expected = new_init_multi(3,
        new_init_single(new_expr_fixlit(&tyInt, NULL, 11)),
        new_init_single(new_expr_fixlit(&tyInt, NULL, 22)),
        NULL);
    expect(expected, "{11, 22}", type);
  }

  // Braced string for char pointer.
  expect2("\"hello\"", "{\"hello\"}", ptrof(&tyChar));
  expect2("\"array\"", "{\"array\"}", arrayof(&tyChar, 4));


  {  // String for char array in struct.
    Vector *members = new_vector();
    add_struct_member(members, alloc_name("str", NULL, false), arrayof(&tyChar, 4));
    StructInfo *sinfo = create_struct_info(members, false);
    Type *type = create_struct_type(sinfo, NULL, 0);
    expect2("{\"abcd\"}", "{\"abcd\"}", type);
  }

  {  // Array index initializer.
    Initializer *expected = new_init_multi(2,
        new_init_arr(1, new_init_single(new_expr_fixlit(&tyInt, NULL, 11))),
        new_init_arr(3, new_init_single(new_expr_fixlit(&tyInt, NULL, 33))),
        NULL);
    expect(expected, "{[3] = 33, [1] = 11}", arrayof(&tyInt, -1));
  }

  {  // Dotted initializer.
    Vector *members = new_vector();
    add_struct_member(members, alloc_name("x", NULL, false), &tyInt);
    add_struct_member(members, alloc_name("y", NULL, false), &tyInt);
    add_struct_member(members, alloc_name("z", NULL, false), &tyInt);
    StructInfo *sinfo = create_struct_info(members, false);
    Type *type = create_struct_type(sinfo, NULL, 0);
    expect2("{7, 8, 9}", "{.z = 9, .y = 8, .x = 7}", type);
  }

  {  // 2D array.
    Initializer *expected = new_init_multi(2,
        new_init_multi(3,
            new_init_single(new_expr_fixlit(&tyInt, NULL, 2)),
            new_init_single(new_expr_fixlit(&tyInt, NULL, 4)),
            new_init_single(new_expr_fixlit(&tyInt, NULL, 6))),
        new_init_multi(2,
            new_init_single(new_expr_fixlit(&tyInt, NULL, 9)),
            new_init_single(new_expr_fixlit(&tyInt, NULL, 11))));
    expect(expected, "{{2, 4, 6}, {9, 11}}", arrayof(arrayof(&tyInt, 3), 2));
  }

  // 2D array without brace.
  expect2("{{2, 4, 6}, {9, 11}}", "{2, 4, 6, 9, 11}", arrayof(arrayof(&tyInt, 3), 2));
  expect2("{{3, 1}, {4, 1}, {5, 9}}", "{{3, 1}, 4, 1, {5, 9}}", arrayof(arrayof(&tyInt, 2), -1));

  // Array of struct without brace.
  {
    Vector *members = new_vector();
    add_struct_member(members, alloc_name("x", NULL, false), &tyChar);
    add_struct_member(members, alloc_name("y", NULL, false), get_fixnum_type(FX_SHORT, false, 0));
    StructInfo *sinfo = create_struct_info(members, false);
    Type *type = create_struct_type(sinfo, NULL, 0);

    expect2("{{11, 12}, {21, 22}}", "{11, 12, 21, 22}", arrayof(type, -1));
    expect2("{{11, 12}, {21, 22}}", "{{11, 12}, 21, 22}", arrayof(type, 2));
  }
}

int main(void) {
  test_flatten();
  return MIN(error_count, 255);
}
