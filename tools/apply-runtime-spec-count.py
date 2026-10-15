from pathlib import Path


def replace_between(text, start_marker, end_marker, replacement):
    start = text.index(start_marker)
    end = text.index(end_marker, start)
    return text[:start] + replacement + text[end:]


# parse.c
path = Path("parse.c")
s = path.read_text()
s = s.replace(
    "static Node *ppc_create_spec_expr(Token *tok, Type *ty);\n"
    "static Node *ppc_init_spec_expr(Token *tok, Type *ty, Node *ptr);",
    "static Node *ppc_create_spec_expr(Token *tok, Type *ty, Node *count);\n"
    "static Node *ppc_init_spec_expr(Token *tok, Type *ty, Node *ptr, Node *count);",
    1,
)

primary = '''  if (equal(tok, "create_spec")) {
    tok = skip(tok->next, "(");
    Type *ty = typename(&tok, tok);
    Node *count = NULL;
    if (equal(tok, ","))
      count = assign(&tok, tok->next);
    *rest = skip(tok, ")");
    return ppc_create_spec_expr(start, ty, count);
  }

  if (equal(tok, "init_spec")) {
    tok = skip(tok->next, "(");
    Type *ty = typename(&tok, tok);
    tok = skip(tok, ",");
    Node *ptr = assign(&tok, tok);
    Node *count = NULL;
    if (equal(tok, ","))
      count = assign(&tok, tok->next);
    *rest = skip(tok, ")");
    return ppc_init_spec_expr(start, ty, ptr, count);
  }

'''
s = replace_between(
    s,
    '  if (equal(tok, "create_spec")) {',
    '  if (equal(tok, "get_spec_size")) {',
    primary,
)

helpers = r'''static Type *ppc_object_family_base_ty(Token *tok, Type *ty, char *name) {
  if (ty->kind == TY_ARRAY || ty->kind == TY_VLA)
    error_tok(tok, "%s does not accept an array type; pass the element count as a separate argument", name);

  if (ty->kind != TY_STRUCT || (!ty->is_ppc_generalization && !ty->ppc_base))
    error_tok(tok, "%s expects a PPC generalization or specialization type", name);

  return ty->ppc_base ? ty->ppc_base : ty;
}

static Node *ppc_save_count(Token *tok, Node *count, Obj **count_var, char *name) {
  if (!count)
    count = new_num(1, tok);

  add_type(count);
  if (!is_integer(count->ty))
    error_tok(count->tok, "%s count must have integer type", name);

  *count_var = new_lvar("", ty_long);
  return new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(*count_var, tok),
               new_cast(count, ty_long), tok),
    tok);
}

static Node *ppc_check_count(Token *tok, Obj *count_var, char *name) {
  Node *cond = new_binary(ND_LT, new_var_node(count_var, tok),
                          new_long(0, tok), tok);

  char *fmt = format("PPC %s error: negative count %%ld\n", name);
  Obj *fmt_var = new_string_literal(fmt, array_of(ty_char, strlen(fmt) + 1));

  Node *args = new_num(2, tok);
  args->next = new_cast(new_var_node(fmt_var, tok), pointer_to(ty_char));
  args->next->next = new_var_node(count_var, tok);

  Node *print = new_expr_stmt_node(new_func_call_node(get_dprintf_func(), args, tok), tok);
  Node *abort = new_expr_stmt_node(new_func_call_node(get_abort_func(), NULL, tok), tok);
  print->next = abort;

  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = cond;
  if_node->then = new_block_node(print, tok);
  return if_node;
}

static Node *ppc_check_create_spec_overflow(Token *tok, Obj *count_var, Type *ty) {
  Node *max_count = new_binary(ND_DIV, new_ulong(-1, tok),
                               new_ulong(ty->size, tok), tok);
  Node *actual_count = new_cast(new_var_node(count_var, tok), ty_ulong);
  Node *cond = new_binary(ND_LT, max_count, actual_count, tok);

  char *fmt = format("PPC create_spec error: count %%ld overflows allocation for element size %d\n",
                     ty->size);
  Obj *fmt_var = new_string_literal(fmt, array_of(ty_char, strlen(fmt) + 1));

  Node *args = new_num(2, tok);
  args->next = new_cast(new_var_node(fmt_var, tok), pointer_to(ty_char));
  args->next->next = new_var_node(count_var, tok);

  Node *print = new_expr_stmt_node(new_func_call_node(get_dprintf_func(), args, tok), tok);
  Node *abort = new_expr_stmt_node(new_func_call_node(get_abort_func(), NULL, tok), tok);
  print->next = abort;

  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = cond;
  if_node->then = new_block_node(print, tok);
  return if_node;
}

static Node *ppc_init_spec_tags_loop(Token *tok, Type *ty, Obj *spec_ptr,
                                     Obj *count_var) {
  Obj *i = new_lvar("", ty_long);
  Obj *elem_ptr = new_lvar("", pointer_to(ty));

  Node *init = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(i, tok), new_long(0, tok), tok), tok);
  Node *cond = new_binary(ND_LT, new_var_node(i, tok),
                          new_var_node(count_var, tok), tok);
  Node *inc = new_binary(ND_ASSIGN, new_var_node(i, tok),
                         new_add(new_var_node(i, tok), new_long(1, tok), tok), tok);

  Node *assign_elem = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(elem_ptr, tok),
               new_add(new_var_node(spec_ptr, tok), new_var_node(i, tok), tok), tok),
    tok);

  InitDesg elem_desg = {NULL, 0, NULL, elem_ptr, true};
  Node *assign_tags =
    new_expr_stmt_node(ppc_tag_initializer2(ty, &elem_desg, tok, NULL), tok);
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

static Node *ppc_create_spec_expr(Token *tok, Type *ty, Node *count) {
  if (!current_fn)
    error_tok(tok, "create_spec is not allowed outside a function");

  ppc_object_family_base_ty(tok, ty, "create_spec");

  if (!ppc_type_has_tag(ty, false))
    error_tok(tok, "PPC type has no specialization tag");

  Obj *count_var;
  Node *assign_count = ppc_save_count(tok, count, &count_var, "create_spec");
  Node *check_count = ppc_check_count(tok, count_var, "create_spec");
  Node *check_overflow = ppc_check_create_spec_overflow(tok, count_var, ty);

  Obj *ptr = new_lvar("", pointer_to(ty));
  Node *clear_ptr = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(ptr, tok),
               new_cast(new_num(0, tok), pointer_to(ty)), tok),
    tok);

  Node *arg1 = new_cast(new_var_node(count_var, tok), ty_ulong);
  Node *arg2 = new_ulong(ty->size, tok);
  arg1->next = arg2;

  Node *alloc = new_func_call_node(get_calloc_func(), arg1, tok);
  Node *assign_ptr = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(ptr, tok),
               new_cast(alloc, pointer_to(ty)), tok),
    tok);

  Node *alloc_if = new_node(ND_IF, tok);
  alloc_if->cond = new_binary(ND_NE, new_var_node(count_var, tok),
                              new_long(0, tok), tok);
  alloc_if->then = new_block_node(assign_ptr, tok);

  Node *assign_tags = ppc_init_spec_tags_loop(tok, ty, ptr, count_var);
  Node *ret = new_expr_stmt_node(new_var_node(ptr, tok), tok);

  assign_count->next = check_count;
  check_count->next = check_overflow;
  check_overflow->next = clear_ptr;
  clear_ptr->next = alloc_if;
  alloc_if->next = assign_tags;
  assign_tags->next = ret;

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = assign_count;
  return node;
}

static Node *ppc_init_spec_expr(Token *tok, Type *ty, Node *ptr, Node *count) {
  if (!current_fn)
    error_tok(tok, "init_spec is not allowed outside a function");

  Type *base = ppc_object_family_base_ty(tok, ty, "init_spec");

  add_type(ptr);
  if (ptr->ty->kind != TY_PTR)
    error_tok(tok, "init_spec pointer operand must be a pointer");

  if (ptr->ty->base->kind != TY_VOID) {
    Type *ptr_base = ppc_object_family_base_ty(tok, ptr->ty->base, "init_spec pointer");
    if (ptr_base != base)
      error_tok(tok, "init_spec pointer must point to the same PPC generalization family");
  }

  if (!ppc_type_has_tag(ty, false))
    error_tok(tok, "PPC type has no specialization tag");

  Obj *spec_ptr = new_lvar("", pointer_to(ty));
  Node *assign_ptr = new_expr_stmt_node(
    new_binary(ND_ASSIGN, new_var_node(spec_ptr, tok),
               new_cast(ptr, pointer_to(ty)), tok),
    tok);

  Obj *count_var;
  Node *assign_count = ppc_save_count(tok, count, &count_var, "init_spec");
  Node *check_count = ppc_check_count(tok, count_var, "init_spec");
  Node *assign_tags = ppc_init_spec_tags_loop(tok, ty, spec_ptr, count_var);
  Node *zero = new_expr_stmt_node(new_num(0, tok), tok);

  assign_ptr->next = assign_count;
  assign_count->next = check_count;
  check_count->next = assign_tags;
  assign_tags->next = zero;

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = assign_ptr;
  return new_cast(node, ty_void);
}

'''
s = replace_between(
    s,
    "static Type *ppc_array_element_ty(",
    "static Type *ppc_generalization_arg(",
    helpers,
)
path.write_text(s)


# Migrate old array-count tests and add runtime-expression coverage.
path = Path("test/ppc.c")
s = path.read_text()
for old, new in {
    "create_spec(Shape.color.rect[2])": "create_spec(Shape.color.rect, 2)",
    "create_spec(Shape.color.color.rect[2])": "create_spec(Shape.color.color.rect, 2)",
    "init_spec(Shape.color.rect[2], (void *)u.buf)": "init_spec(Shape.color.rect, (void *)u.buf, 2)",
    "init_spec(Shape.color.color.rect[2], (void *)u.buf)": "init_spec(Shape.color.color.rect, (void *)u.buf, 2)",
}.items():
    if old not in s:
        raise RuntimeError(f"missing old test syntax: {old}")
    s = s.replace(old, new)
path.write_text(s)

Path("test/ppc-spec-count.c").write_text(r'''#include "test.h"

typedef struct CountFigure { int common; } case { int rect; } CountFigure;

int CountKind<CountFigure *f>() { return 0; }
int CountKind<CountFigure.rect *r>() { return 1; }

static int count_calls;

static int runtime_count(int n) {
  count_calls++;
  return n;
}

int main() {
  int n = 3;

  count_calls = 0;
  CountFigure.rect *created = create_spec(CountFigure.rect, runtime_count(n));
  ASSERT(1, count_calls);
  ASSERT(1, CountKind<&created[0]>());
  ASSERT(1, CountKind<&created[1]>());
  ASSERT(1, CountKind<&created[2]>());

  count_calls = 0;
  union {
    CountFigure.rect item[3];
    char buf[sizeof(CountFigure.rect) * 3];
  } storage;
  memset(storage.buf, 0, sizeof(storage.buf));
  init_spec(CountFigure.rect, (void *)storage.buf, runtime_count(n));
  ASSERT(1, count_calls);
  ASSERT(1, CountKind<&storage.item[0]>());
  ASSERT(1, CountKind<&storage.item[1]>());
  ASSERT(1, CountKind<&storage.item[2]>());

  CountFigure.rect *single = create_spec(CountFigure.rect);
  ASSERT(1, CountKind<single>());

  CountFigure.rect one;
  *(int *)&one = 0;
  init_spec(CountFigure.rect, &one);
  ASSERT(1, CountKind<&one>());

  count_calls = 0;
  CountFigure.rect *empty = create_spec(CountFigure.rect, runtime_count(0));
  ASSERT(1, count_calls);
  ASSERT(1, empty == 0);

  count_calls = 0;
  init_spec(CountFigure.rect, (void *)0, runtime_count(0));
  ASSERT(1, count_calls);

  printf("OK\n");
  return 0;
}
''')


# Documentation: replace the two grammar snippets and add semantics notes.
path = Path("docs/ppc-manual-ru.md")
s = path.read_text()
start = s.index("    ИнициализацияСпециализации =")
end = s.index("\n\nФункция позволяет", start)
s = s[:start] + '''    ИнициализацияСпециализации =
        void init_spec \\"(\\" ИмяОбобщения[\\".\\" ПризнакСпециализации]
                               \",\\" УказательНаОбластьПамяти
                               [\\",\\" ЦелочисленноеВыражение] \\")\\".'''+ s[end:]

needle = "Функция является ненадежной, но в целом полностью соответствует концепциям языка программирования C. Она также позволяет формировать массивы, состоящие из специализированных переменных, инициализируя в цикле признаками специализаций соответствующие области памяти."
if needle not in s:
    raise RuntimeError("init_spec documentation paragraph not found")
s = s.replace(needle, needle + "\n\nЕсли третий аргумент отсутствует, инициализируется одна специализация. Если он задан, целочисленное выражение вычисляется ровно один раз во время выполнения и определяет число последовательно размещенных специализаций. Нулевое значение допустимо и не изменяет память, отрицательное значение приводит к диагностике времени выполнения. Прежняя форма с размером массива внутри имени типа, например `init_spec(Figure2.rect[10], p)`, не поддерживается; число элементов задается отдельным аргументом.", 1)

start = s.index("    СозданиеСпециализации =")
end = s.index("\n\nДанная функция", start)
s = s[:start] + '''    СозданиеСпециализации =
        УказательНаОбобщение create_spec
            \\"(\\" ИмяОбобщения[\\".\\" ПризнакСпециализации]
                   [\\",\\" ЦелочисленноеВыражение] \\")\\".'''+ s[end:]

needle = "  Figure2* pfr = create_spec(Figure2.rect);\n  ```"
if needle not in s:
    raise RuntimeError("create_spec example not found")
s = s.replace(needle, needle + "\n\nПри наличии второго аргумента `create_spec` создает последовательность специализаций, число которых задается целочисленным выражением, вычисляемым ровно один раз во время выполнения. Например:\n\n  ```\n  int n = read_count();\n  Figure2.rect *rects = create_spec(Figure2.rect, n);\n  ```\n\nЗначение `n == 0` допустимо, при этом функция возвращает нулевой указатель. Отрицательное значение приводит к диагностике времени выполнения. Перед выделением памяти проверяется переполнение размера. Форма `create_spec(Figure2.rect[10])` больше не поддерживается; количество элементов всегда передается отдельным аргументом.", 1)
path.write_text(s)
