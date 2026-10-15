from pathlib import Path

p = Path("parse.c")
s = p.read_text()

old = "static Node *ppc_tag_initializer(Obj *var, Token *tok);\n"
new = "static Node *ppc_tag_initializer(Obj *var, Token *tok);\nstatic Node *ppc_vla_tag_initializer(Obj *var, Token *tok);\n"
assert old in s
s = s.replace(old, new, 1)

old = """static bool ppc_type_has_tag(Type *ty, bool runtime_only) {\n  if (ty->kind == TY_ARRAY)\n    return ppc_type_has_tag(ty->base, runtime_only);\n"""
new = """static bool ppc_type_has_tag(Type *ty, bool runtime_only) {\n  if (ty->kind == TY_ARRAY || ty->kind == TY_VLA)\n    return ppc_type_has_tag(ty->base, runtime_only);\n"""
assert old in s
s = s.replace(old, new, 1)

marker = """static Node *ppc_tag_initializer(Obj *var, Token *tok) {\n  if (!ppc_type_has_tag(var->ty, false))\n    return NULL;\n\n  InitDesg var_desg = {NULL, 0, NULL, var};\n  return ppc_tag_initializer2(var->ty, &var_desg, tok, NULL);\n}\n"""
addition = r'''

// Generate runtime initialization for PPC tags stored in a variable length
// array. The regular tag initializer can unroll fixed-size arrays at compile
// time; a VLA needs a runtime loop over the storage allocated for it.
static Node *ppc_vla_tag_initializer(Obj *var, Token *tok) {
  Type *ty = var->ty;
  if (ty->kind != TY_VLA || !ppc_type_has_tag(ty, false))
    return NULL;

  // Flatten array dimensions until we reach the fixed-size object whose PPC
  // tags must be initialized. Using the already-computed VLA byte size avoids
  // evaluating the original VLA bound expression a second time.
  Type *elem = ty;
  while (elem->kind == TY_VLA || elem->kind == TY_ARRAY)
    elem = elem->base;

  if (elem->size <= 0)
    error_tok(tok, "PPC VLA element type must have a fixed positive size");

  Obj *i = new_lvar("", ty_ulong);
  Obj *elem_ptr = new_lvar("", pointer_to(elem));

  Node *init = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(i, tok), new_ulong(0, tok), tok), tok);

  Node *count = new_binary(ND_DIV, new_var_node(ty->vla_size, tok),
                           new_ulong(elem->size, tok), tok);
  Node *cond = new_binary(ND_LT, new_var_node(i, tok), count, tok);
  Node *inc = new_binary(ND_ASSIGN, new_var_node(i, tok),
                         new_add(new_var_node(i, tok), new_ulong(1, tok), tok), tok);

  Node *base_ptr = new_cast(new_vla_ptr(var, tok), pointer_to(elem));
  Node *addr = new_add(base_ptr, new_var_node(i, tok), tok);
  Node *assign_elem = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(elem_ptr, tok), addr, tok), tok);

  InitDesg elem_desg = {NULL, 0, NULL, elem_ptr, true};
  Node *assign_tags = new_expr_stmt_node(
    ppc_tag_initializer2(elem, &elem_desg, tok, NULL), tok);
  assign_elem->next = assign_tags;

  Node *loop = new_node(ND_FOR, tok);
  loop->init = init;
  loop->cond = cond;
  loop->inc = inc;
  loop->then = new_block_node(assign_elem, tok);
  loop->brk_label = new_unique_name();
  loop->cont_label = new_unique_name();
  return loop;
}
'''
assert marker in s
s = s.replace(marker, marker + addition, 1)

old = """      Node *expr = new_binary(ND_ASSIGN, new_vla_ptr(var, tok),\n                              new_alloca(new_var_node(ty->vla_size, tok)),\n                              tok);\n\n      cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);\n      continue;\n"""
new = """      Node *expr = new_binary(ND_ASSIGN, new_vla_ptr(var, tok),\n                              new_alloca(new_var_node(ty->vla_size, tok)),\n                              tok);\n\n      cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);\n\n      Node *tag_init = ppc_vla_tag_initializer(var, tok);\n      if (tag_init)\n        cur = cur->next = tag_init;\n      continue;\n"""
assert old in s
s = s.replace(old, new, 1)

p.write_text(s)

test = r'''#include "test.h"

typedef struct VLAWeekDay { int week_number; } enum {
  Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday
} const VLAWeekDay;

static int bound_calls;

static int runtime_bound(int n) {
  bound_calls++;
  return n;
}

int code<VLAWeekDay *w>() { return 0; }
int code<VLAWeekDay.Monday *w>() { return 1; }
int code<VLAWeekDay.Saturday *w>() { return 6; }
int code<VLAWeekDay.Sunday *w>() { return 7; }

int main() {
  int n = 10;
  VLAWeekDay days[n];

  for (int i = 0; i < n; i++) {
    days[i].week_number = i;
    ASSERT(0, get_spec_var_index(&days[i]));
    ASSERT(0, code<&days[i]>());
  }

  int sunday = get_subtype_index(VLAWeekDay.Sunday);
  init_spec(VLAWeekDay.Sunday, days + 1, n - 2);
  ASSERT(0, get_spec_var_index(&days[0]));
  for (int i = 1; i < n - 1; i++) {
    ASSERT(sunday, get_spec_var_index(&days[i]));
    ASSERT(7, code<&days[i]>());
  }
  ASSERT(0, get_spec_var_index(&days[n - 1]));

  int monday = get_subtype_index(VLAWeekDay.Monday);
  VLAWeekDay.Monday mondays[n - 3];
  for (int i = 0; i < n - 3; i++) {
    ASSERT(monday, get_spec_var_index(&mondays[i]));
    ASSERT(1, code<&mondays[i]>());
  }

  int rows = 3;
  int cols = 4;
  VLAWeekDay matrix[rows][cols];
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++)
      ASSERT(0, get_spec_var_index(&matrix[r][c]));

  bound_calls = 0;
  VLAWeekDay.Saturday saturdays[runtime_bound(5)];
  ASSERT(1, bound_calls);
  for (int i = 0; i < 5; i++) {
    ASSERT(get_subtype_index(VLAWeekDay.Saturday),
           get_spec_var_index(&saturdays[i]));
    ASSERT(6, code<&saturdays[i]>());
  }
  ASSERT(1, bound_calls);

  printf("OK\n");
  return 0;
}
'''
Path("test/ppc-vla.c").write_text(test)
