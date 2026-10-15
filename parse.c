// This file contains a recursive descent parser for C.
//
// Most functions in this file are named after the symbols they are
// supposed to read from an input token list. For example, stmt() is
// responsible for reading a statement from a token list. The function
// then construct an AST node representing a statement.
//
// Each function conceptually returns two values, an AST node and
// remaining part of the input tokens. Since C doesn't support
// multiple return values, the remaining tokens are returned to the
// caller via a pointer argument.
//
// Input tokens are represented by a linked list. Unlike many recursive
// descent parsers, we don't have the notion of the "input token stream".
// Most parsing functions don't change the global state of the parser.
// So it is very easy to lookahead arbitrary number of tokens in this
// parser.

#include "chibicc.h"

// Scope for local variables, global variables, typedefs
// or enum constants
typedef struct {
  Obj *var;
  Type *type_def;
  Type *enum_ty;
  int enum_val;
} VarScope;

// Represents a block scope.
typedef struct Scope Scope;
struct Scope {
  Scope *next;

  // C has two block scopes; one is for variables/typedefs and
  // the other is for struct/union/enum tags.
  HashMap vars;
  HashMap tags;
};

// Variable attributes such as typedef or extern.
typedef struct {
  bool is_typedef;
  bool is_static;
  bool is_extern;
  bool is_inline;
  bool is_tls;
  int align;
} VarAttr;

// Procedural-parametric functions are represented as a small registry
// keyed by the source-level name and the PPC generalization parameters.
typedef struct PpcFunc PpcFunc;
typedef struct PpcHandler PpcHandler;
typedef struct PpcGen PpcGen;
typedef struct PpcNestedType PpcNestedType;

struct PpcHandler {
  PpcHandler *next;
  int *indices;
  bool *wildcards;
  Type *param_tys;
  Obj *fn;
  bool is_abstract;
  bool is_default;
};

struct PpcFunc {
  PpcFunc *next;
  char *name;
  Token *name_tok;
  Type *return_ty;
  Type **base_tys;
  int ppc_param_count;
  Type *regular_params;
  bool is_variadic;
  Type *generic_func_ty;
  Type *fnptr_ty;
  Obj *table_var;
  Obj *default_fn;
  PpcHandler *handlers;
};

struct PpcGen {
  PpcGen *next;
  Type *ty;
  Token *tok;
};

struct PpcNestedType {
  PpcNestedType *next;
  Type *outer;
  Token *name;
  Type *ty;
};

// This struct represents a variable initializer. Since initializers
// can be nested (e.g. `int x[2][2] = {{1, 2}, {3, 4}}`), this struct
// is a tree data structure.
typedef struct Initializer Initializer;
struct Initializer {
  Initializer *next;
  Type *ty;
  Token *tok;
  bool is_flexible;
  Member *ppc_enum_feature;

  // If it's not an aggregate type and has an initializer,
  // `expr` has an initialization expression.
  Node *expr;

  // If it's an initializer for an aggregate type (e.g. array or struct),
  // `children` has initializers for its children.
  Initializer **children;

  // Only one member can be initialized for a union.
  // `mem` is used to clarify which member is initialized.
  Member *mem;
};

// For local variable initializer.
typedef struct InitDesg InitDesg;
struct InitDesg {
  InitDesg *next;
  int idx;
  Member *member;
  Obj *var;
  bool is_deref;
};

// All local variable instances created during parsing are
// accumulated to this list.
static Obj *locals;

// Likewise, global variables are accumulated to this list.
static Obj *globals;

// Likewise, procedural-parametric functions are accumulated to this list.
static PpcFunc *ppc_funcs;

// Likewise, procedural-parametric generalizations are accumulated to this list.
static PpcGen *ppc_gens;

// Likewise, generated PPC chain specialization types are cached here.
static PpcNestedType *ppc_nested_types;

static Scope *scope = &(Scope){};

// Points to the function object the parser is currently parsing.
static Obj *current_fn;

// Lists of all goto statements and labels in the curent function.
static Node *gotos;
static Node *labels;

// Current "goto" and "continue" jump targets.
static char *brk_label;
static char *cont_label;

// Points to a node representing a switch if we are parsing
// a switch statement. Otherwise, NULL.
static Node *current_switch;

static Obj *builtin_alloca;

static bool is_typename(Token *tok);
static Type *declspec(Token **rest, Token *tok, VarAttr *attr);
static Type *typename(Token **rest, Token *tok);
static Type *enum_specifier(Token **rest, Token *tok);
static Type *typeof_specifier(Token **rest, Token *tok);
static Type *type_suffix(Token **rest, Token *tok, Type *ty);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr);
static void array_initializer2(Token **rest, Token *tok, Initializer *init, int i);
static void struct_initializer2(Token **rest, Token *tok, Initializer *init, Member *mem);
static void initializer2(Token **rest, Token *tok, Initializer *init);
static Initializer *initializer(Token **rest, Token *tok, Type *ty, Type **new_ty);
static Node *lvar_initializer(Token **rest, Token *tok, Obj *var);
static Initializer *gvar_initializer(Token **rest, Token *tok, Obj *var);
static Node *ppc_tag_value(Member *mem, Token *tok);
static Node *ppc_tag_initializer(Obj *var, Token *tok);
static void ppc_gvar_tag_initializer(Obj *var, Initializer *init, Token *tok);
static bool ppc_initializer_has_enum_feature(Initializer *init);
static Member *ppc_read_enum_feature(Token **rest, Token *tok, Type *ty);
static void ppc_check_enum_feature_target(Type *ty, Token *tok);
static Node *ppc_enum_feature_assignment(Node *lhs, Member *feature, Token *tok);
static Member *find_ppc_spec(Type *base, Token *name);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static int64_t eval(Node *node);
static int64_t eval2(Node *node, char ***label);
static int64_t eval_rval(Node *node, char ***label);
static bool is_const_expr(Node *node);
static Node *assign(Token **rest, Token *tok);
static Node *logor(Token **rest, Token *tok);
static double eval_double(Node *node);
static Node *conditional(Token **rest, Token *tok);
static Node *logand(Token **rest, Token *tok);
static Node *bitor(Token **rest, Token *tok);
static Node *bitxor(Token **rest, Token *tok);
static Node *bitand(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *shift(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *new_add(Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(Node *lhs, Node *rhs, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *cast(Token **rest, Token *tok);
static Member *get_struct_member(Type *ty, Token *tok);
static Type *struct_decl(Token **rest, Token *tok);
static Type *union_decl(Token **rest, Token *tok);
static Type *ppc_specialization_type(Type *base, Token *name);
static Type *ppc_type_suffix(Token **rest, Type *ty, Token *tok);
static Token *ppc_extend_generalization(Token *tok, Type *base);
static Member *next_user_member(Member *mem);
static Member *ppc_tag_member(Type *ty);
static PpcFunc *find_ppc_func(Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *funcall(Token **rest, Token *tok, Node *node);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);
static Node *new_expr_stmt_node(Node *expr, Token *tok);
static Node *ppc_tag_expr(Obj *var, Type *base_ty, Token *tok);
static Node *ppc_extent_expr(Type *base, Token *tok);
static Node *ppc_create_spec_expr(Token *tok, Type *ty, Node *count);
static Node *ppc_init_spec_expr(Token *tok, Type *ty, Node *ptr, Node *count);
static Node *ppc_get_spec_size_expr(Token *tok, Type *ty);
static Node *ppc_get_spec_ptr_expr(Token *tok, Type *ty, Node *idx);
static Node *ppc_spec_index_cmp_expr(Token *tok, Node *lhs, Node *rhs);
static Node *ppc_funcall_expr(Token **rest, Token *tok);
static Token *parse_typedef(Token *tok, Type *basety);
static bool is_ppc_function(Token *tok);
static Token *ppc_function(Token *tok, Type *basety, VarAttr *attr);
static bool is_function(Token *tok);
static Token *function(Token *tok, Type *basety, VarAttr *attr);
static Token *global_variable(Token *tok, Type *basety, VarAttr *attr);

static int align_down(int n, int align) {
  return align_to(n - align + 1, align);
}

static void enter_scope(void) {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->next = scope;
  scope = sc;
}

static void leave_scope(void) {
  scope = scope->next;
}

// Find a variable by name.
static VarScope *find_var(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    VarScope *sc2 = hashmap_get2(&sc->vars, tok->loc, tok->len);
    if (sc2)
      return sc2;
  }
  return NULL;
}

static Type *find_tag(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    Type *ty = hashmap_get2(&sc->tags, tok->loc, tok->len);
    if (ty)
      return ty;
  }
  return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

static Node *new_num(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node *new_long(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_long;
  return node;
}

static Node *new_ulong(long val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_ulong;
  return node;
}

static Node *new_var_node(Obj *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static Node *new_vla_ptr(Obj *var, Token *tok) {
  Node *node = new_node(ND_VLA_PTR, tok);
  node->var = var;
  return node;
}

Node *new_cast(Node *expr, Type *ty) {
  add_type(expr);

  if (ty->kind == TY_NULLPTR) {
    if (expr->ty->kind != TY_NULLPTR && !is_null_pointer_constant(expr))
      error_tok(expr->tok, "conversion to nullptr_t requires a null pointer constant");
  } else if (expr->ty->kind == TY_NULLPTR && ty->kind != TY_VOID &&
             ty->kind != TY_BOOL && ty->kind != TY_PTR) {
    error_tok(expr->tok, "nullptr_t converts only to void, _Bool or a pointer");
  }

  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_CAST;
  node->tok = expr->tok;
  node->lhs = expr;
  node->ty = copy_type(ty);
  return node;
}

static VarScope *push_scope(char *name) {
  VarScope *sc = calloc(1, sizeof(VarScope));
  hashmap_put(&scope->vars, name, sc);
  return sc;
}

static Initializer *new_initializer(Type *ty, bool is_flexible) {
  Initializer *init = calloc(1, sizeof(Initializer));
  init->ty = ty;

  if (ty->kind == TY_ARRAY) {
    if (is_flexible && ty->size < 0) {
      init->is_flexible = true;
      return init;
    }

    init->children = calloc(ty->array_len, sizeof(Initializer *));
    for (int i = 0; i < ty->array_len; i++)
      init->children[i] = new_initializer(ty->base, false);
    return init;
  }

  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    // Count the number of struct members.
    int len = 0;
    for (Member *mem = ty->members; mem; mem = mem->next)
      len++;

    init->children = calloc(len, sizeof(Initializer *));

    for (Member *mem = ty->members; mem; mem = mem->next) {
      if (is_flexible && ty->is_flexible && !mem->next) {
        Initializer *child = calloc(1, sizeof(Initializer));
        child->ty = mem->ty;
        child->is_flexible = true;
        init->children[mem->idx] = child;
      } else {
        init->children[mem->idx] = new_initializer(mem->ty, false);
      }
    }
    return init;
  }

  return init;
}

static Obj *new_var(char *name, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  var->align = ty->align;
  push_scope(name)->var = var;
  return var;
}

static Obj *new_lvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->is_local = true;
  var->next = locals;
  locals = var;
  return var;
}

static Obj *new_gvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->next = globals;
  var->is_static = true;
  var->is_definition = true;
  globals = var;
  return var;
}

static char *new_unique_name(void) {
  static int id = 0;
  return format(".L..%d", id++);
}

static Obj *new_anon_gvar(Type *ty) {
  return new_gvar(new_unique_name(), ty);
}

static Obj *new_string_literal(char *p, Type *ty) {
  Obj *var = new_anon_gvar(ty);
  var->init_data = p;
  return var;
}

static char *get_ident(Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected an identifier");
  return strndup(tok->loc, tok->len);
}

static Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    if (sc)
      return sc->type_def;
  }
  return NULL;
}

static void push_tag_scope(Token *tok, Type *ty) {
  hashmap_put2(&scope->tags, tok->loc, tok->len, ty);
}

// declspec = ("void" | "_Bool" | "char" | "short" | "int" | "long"
//             | "typedef" | "static" | "extern" | "inline"
//             | "_Thread_local" | "__thread"
//             | "signed" | "unsigned"
//             | struct-decl | union-decl | typedef-name
//             | enum-specifier | typeof-specifier
//             | "const" | "volatile" | "auto" | "register" | "restrict"
//             | "__restrict" | "__restrict__" | "_Noreturn")+
//
// The order of typenames in a type-specifier doesn't matter. For
// example, `int long static` means the same as `static long int`.
// That can also be written as `static long` because you can omit
// `int` if `long` or `short` are specified. However, something like
// `char int` is not a valid type specifier. We have to accept only a
// limited combinations of the typenames.
//
// In this function, we count the number of occurrences of each typename
// while keeping the "current" type object that the typenames up
// until that point represent. When we reach a non-typename token,
// we returns the current type object.
static Type *declspec(Token **rest, Token *tok, VarAttr *attr) {
  // We use a single integer as counters for all typenames.
  // For example, bits 0 and 1 represents how many times we saw the
  // keyword "void" so far. With this, we can use a switch statement
  // as you can see below.
  enum {
    VOID     = 1 << 0,
    BOOL     = 1 << 2,
    CHAR     = 1 << 4,
    SHORT    = 1 << 6,
    INT      = 1 << 8,
    LONG     = 1 << 10,
    FLOAT    = 1 << 12,
    DOUBLE   = 1 << 14,
    OTHER    = 1 << 16,
    SIGNED   = 1 << 17,
    UNSIGNED = 1 << 18,
  };

  Type *ty = ty_int;
  int counter = 0;
  bool is_atomic = false;

  while (is_typename(tok)) {
    // Handle storage class specifiers.
    if (equal(tok, "typedef") || equal(tok, "static") || equal(tok, "extern") ||
        equal(tok, "inline") || equal(tok, "_Thread_local") || equal(tok, "__thread")) {
      if (!attr)
        error_tok(tok, "storage class specifier is not allowed in this context");

      if (equal(tok, "typedef"))
        attr->is_typedef = true;
      else if (equal(tok, "static"))
        attr->is_static = true;
      else if (equal(tok, "extern"))
        attr->is_extern = true;
      else if (equal(tok, "inline"))
        attr->is_inline = true;
      else
        attr->is_tls = true;

      if (attr->is_typedef &&
          attr->is_static + attr->is_extern + attr->is_inline + attr->is_tls > 1)
        error_tok(tok, "typedef may not be used together with static,"
                  " extern, inline, __thread or _Thread_local");
      tok = tok->next;
      continue;
    }

    // These keywords are recognized but ignored.
    if (consume(&tok, tok, "const") || consume(&tok, tok, "volatile") ||
        consume(&tok, tok, "auto") || consume(&tok, tok, "register") ||
        consume(&tok, tok, "restrict") || consume(&tok, tok, "__restrict") ||
        consume(&tok, tok, "__restrict__") || consume(&tok, tok, "_Noreturn"))
      continue;

    if (equal(tok, "_Atomic")) {
      tok = tok->next;
      if (equal(tok , "(")) {
        ty = typename(&tok, tok->next);
        tok = skip(tok, ")");
      }
      is_atomic = true;
      continue;
    }

    if (equal(tok, "_Alignas")) {
      if (!attr)
        error_tok(tok, "_Alignas is not allowed in this context");
      tok = skip(tok->next, "(");

      if (is_typename(tok))
        attr->align = typename(&tok, tok)->align;
      else
        attr->align = const_expr(&tok, tok);
      tok = skip(tok, ")");
      continue;
    }

    // Handle user-defined types.
    Type *ty2 = find_typedef(tok);
    if (equal(tok, "struct") || equal(tok, "union") || equal(tok, "enum") ||
        equal(tok, "typeof") || ty2) {
      if (counter)
        break;

      if (equal(tok, "struct")) {
        ty = struct_decl(&tok, tok->next);
      } else if (equal(tok, "union")) {
        ty = union_decl(&tok, tok->next);
      } else if (equal(tok, "enum")) {
        ty = enum_specifier(&tok, tok->next);
      } else if (equal(tok, "typeof")) {
        ty = typeof_specifier(&tok, tok->next);
      } else {
        ty = ty2;
        tok = tok->next;
        ty = ppc_type_suffix(&tok, ty, tok);
      }

      counter += OTHER;
      continue;
    }

    // Handle built-in types.
    if (equal(tok, "void"))
      counter += VOID;
    else if (equal(tok, "_Bool"))
      counter += BOOL;
    else if (equal(tok, "char"))
      counter += CHAR;
    else if (equal(tok, "short"))
      counter += SHORT;
    else if (equal(tok, "int"))
      counter += INT;
    else if (equal(tok, "long"))
      counter += LONG;
    else if (equal(tok, "float"))
      counter += FLOAT;
    else if (equal(tok, "double"))
      counter += DOUBLE;
    else if (equal(tok, "signed"))
      counter |= SIGNED;
    else if (equal(tok, "unsigned"))
      counter |= UNSIGNED;
    else
      unreachable();

    switch (counter) {
    case VOID:
      ty = ty_void;
      break;
    case BOOL:
      ty = ty_bool;
      break;
    case CHAR:
    case SIGNED + CHAR:
      ty = ty_char;
      break;
    case UNSIGNED + CHAR:
      ty = ty_uchar;
      break;
    case SHORT:
    case SHORT + INT:
    case SIGNED + SHORT:
    case SIGNED + SHORT + INT:
      ty = ty_short;
      break;
    case UNSIGNED + SHORT:
    case UNSIGNED + SHORT + INT:
      ty = ty_ushort;
      break;
    case INT:
    case SIGNED:
    case SIGNED + INT:
      ty = ty_int;
      break;
    case UNSIGNED:
    case UNSIGNED + INT:
      ty = ty_uint;
      break;
    case LONG:
    case LONG + INT:
    case LONG + LONG:
    case LONG + LONG + INT:
    case SIGNED + LONG:
    case SIGNED + LONG + INT:
    case SIGNED + LONG + LONG:
    case SIGNED + LONG + LONG + INT:
      ty = ty_long;
      break;
    case UNSIGNED + LONG:
    case UNSIGNED + LONG + INT:
    case UNSIGNED + LONG + LONG:
    case UNSIGNED + LONG + LONG + INT:
      ty = ty_ulong;
      break;
    case FLOAT:
      ty = ty_float;
      break;
    case DOUBLE:
      ty = ty_double;
      break;
    case LONG + DOUBLE:
      ty = ty_ldouble;
      break;
    default:
      error_tok(tok, "invalid type");
    }

    tok = tok->next;
  }

  if (is_atomic) {
    ty = copy_type(ty);
    ty->is_atomic = true;
  }

  *rest = tok;
  return ty;
}

// func-params = ("void" | param ("," param)* ("," "...")?)? ")"
// param       = declspec declarator
static Type *func_params(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "void") && equal(tok->next, ")")) {
    *rest = tok->next->next;
    return func_type(ty);
  }

  Type head = {};
  Type *cur = &head;
  bool is_variadic = false;

  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");

    if (equal(tok, "...")) {
      is_variadic = true;
      tok = tok->next;
      skip(tok, ")");
      break;
    }

    Type *ty2 = declspec(&tok, tok, NULL);
    ty2 = declarator(&tok, tok, ty2);

    Token *name = ty2->name;

    if (ty2->kind == TY_ARRAY) {
      // "array of T" is converted to "pointer to T" only in the parameter
      // context. For example, *argv[] is converted to **argv by this.
      ty2 = pointer_to(ty2->base);
      ty2->name = name;
    } else if (ty2->kind == TY_FUNC) {
      // Likewise, a function is converted to a pointer to a function
      // only in the parameter context.
      ty2 = pointer_to(ty2);
      ty2->name = name;
    }

    cur = cur->next = copy_type(ty2);
  }

  if (cur == &head)
    is_variadic = true;

  ty = func_type(ty);
  ty->params = head.next;
  ty->is_variadic = is_variadic;
  *rest = tok->next;
  return ty;
}

// array-dimensions = ("static" | "restrict")* const-expr? "]" type-suffix
static Type *array_dimensions(Token **rest, Token *tok, Type *ty) {
  while (equal(tok, "static") || equal(tok, "restrict"))
    tok = tok->next;

  if (equal(tok, "]")) {
    ty = type_suffix(rest, tok->next, ty);
    return array_of(ty, -1);
  }

  Node *expr = conditional(&tok, tok);
  add_type(expr);
  if (expr->ty->kind == TY_NULLPTR)
    error_tok(expr->tok, "array size must have integer type");
  tok = skip(tok, "]");
  ty = type_suffix(rest, tok, ty);

  if (ty->kind == TY_VLA || !is_const_expr(expr))
    return vla_of(ty, expr);
  return array_of(ty, eval(expr));
}

// type-suffix = "(" func-params
//             | "[" array-dimensions
//             | ε
static Type *type_suffix(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "("))
    return func_params(rest, tok->next, ty);

  if (equal(tok, "["))
    return array_dimensions(rest, tok->next, ty);

  *rest = tok;
  return ty;
}

// pointers = ("*" ("const" | "volatile" | "restrict")*)*
static Type *pointers(Token **rest, Token *tok, Type *ty) {
  while (consume(&tok, tok, "*")) {
    ty = pointer_to(ty);
    while (equal(tok, "const") || equal(tok, "volatile") || equal(tok, "restrict") ||
           equal(tok, "__restrict") || equal(tok, "__restrict__"))
      tok = tok->next;
  }
  *rest = tok;
  return ty;
}

// declarator = pointers ("(" ident ")" | "(" declarator ")" | ident) type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty) {
  ty = pointers(&tok, tok, ty);

  if (equal(tok, "(")) {
    Token *start = tok;
    Type dummy = {};
    declarator(&tok, start->next, &dummy);
    tok = skip(tok, ")");
    ty = type_suffix(rest, tok, ty);
    return declarator(&tok, start->next, ty);
  }

  Token *name = NULL;
  Token *name_pos = tok;

  if (tok->kind == TK_IDENT) {
    name = tok;
    tok = tok->next;
  }

  ty = type_suffix(rest, tok, ty);
  ty->name = name;
  ty->name_pos = name_pos;
  return ty;
}

// abstract-declarator = pointers ("(" abstract-declarator ")")? type-suffix
static Type *abstract_declarator(Token **rest, Token *tok, Type *ty) {
  ty = pointers(&tok, tok, ty);

  if (equal(tok, "(")) {
    Token *start = tok;
    Type dummy = {};
    abstract_declarator(&tok, start->next, &dummy);
    tok = skip(tok, ")");
    ty = type_suffix(rest, tok, ty);
    return abstract_declarator(&tok, start->next, ty);
  }

  return type_suffix(rest, tok, ty);
}

// type-name = declspec abstract-declarator
static Type *typename(Token **rest, Token *tok) {
  Type *ty = declspec(&tok, tok, NULL);
  return abstract_declarator(rest, tok, ty);
}

static bool is_end(Token *tok) {
  return equal(tok, "}") || (equal(tok, ",") && equal(tok->next, "}"));
}

static bool consume_end(Token **rest, Token *tok) {
  if (equal(tok, "}")) {
    *rest = tok->next;
    return true;
  }

  if (equal(tok, ",") && equal(tok->next, "}")) {
    *rest = tok->next->next;
    return true;
  }

  return false;
}

// enum-specifier = ident? "{" enum-list? "}"
//                | ident ("{" enum-list? "}")?
//
// enum-list      = ident ("=" num)? ("," ident ("=" num)?)* ","?
static Type *enum_specifier(Token **rest, Token *tok) {
  Type *ty = enum_type();

  // Read a struct tag.
  Token *tag = NULL;
  if (tok->kind == TK_IDENT) {
    tag = tok;
    tok = tok->next;
  }

  if (tag && !equal(tok, "{")) {
    Type *ty = find_tag(tag);
    if (!ty)
      error_tok(tag, "unknown enum type");
    if (ty->kind != TY_ENUM)
      error_tok(tag, "not an enum tag");
    *rest = tok;
    return ty;
  }

  tok = skip(tok, "{");

  // Read an enum-list.
  int i = 0;
  int val = 0;
  while (!consume_end(rest, tok)) {
    if (i++ > 0)
      tok = skip(tok, ",");

    char *name = get_ident(tok);
    tok = tok->next;

    if (equal(tok, "="))
      val = const_expr(&tok, tok->next);

    VarScope *sc = push_scope(name);
    sc->enum_ty = ty;
    sc->enum_val = val++;
  }

  if (tag)
    push_tag_scope(tag, ty);
  return ty;
}

// typeof-specifier = "(" (expr | typename) ")"
static Type *typeof_specifier(Token **rest, Token *tok) {
  tok = skip(tok, "(");

  Type *ty;
  if (is_typename(tok)) {
    ty = typename(&tok, tok);
  } else {
    Node *node = expr(&tok, tok);
    add_type(node);
    ty = node->ty;
  }
  *rest = skip(tok, ")");
  return ty;
}

// Generate code for computing a VLA size.
static Node *compute_vla_size(Type *ty, Token *tok) {
  Node *node = new_node(ND_NULL_EXPR, tok);
  if (ty->base)
    node = new_binary(ND_COMMA, node, compute_vla_size(ty->base, tok), tok);

  if (ty->kind != TY_VLA)
    return node;

  Node *base_sz;
  if (ty->base->kind == TY_VLA)
    base_sz = new_var_node(ty->base->vla_size, tok);
  else
    base_sz = new_num(ty->base->size, tok);

  ty->vla_size = new_lvar("", ty_ulong);
  Node *expr = new_binary(ND_ASSIGN, new_var_node(ty->vla_size, tok),
                          new_binary(ND_MUL, ty->vla_len, base_sz, tok),
                          tok);
  return new_binary(ND_COMMA, node, expr, tok);
}

static Node *new_alloca(Node *sz) {
  Node *node = new_unary(ND_FUNCALL, new_var_node(builtin_alloca, sz->tok), sz->tok);
  node->func_ty = builtin_alloca->ty;
  node->ty = builtin_alloca->ty->return_ty;
  node->args = sz;
  add_type(sz);
  return node;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr) {
  Node head = {};
  Node *cur = &head;
  int i = 0;

  while (!equal(tok, ";")) {
    if (i++ > 0)
      tok = skip(tok, ",");

    Type *ty = declarator(&tok, tok, basety);
    if (ty->kind == TY_VOID)
      error_tok(tok, "variable declared void");
    if (!ty->name)
      error_tok(ty->name_pos, "variable name omitted");

    if (attr && attr->is_static) {
      // static local variable
      Obj *var = new_anon_gvar(ty);
      push_scope(get_ident(ty->name))->var = var;
      Initializer *init = NULL;
      if (equal(tok, "="))
        init = gvar_initializer(&tok, tok->next, var);
      if (attr->is_tls && ppc_initializer_has_enum_feature(init))
        error_tok(tok, "PPC enum feature initialization of thread-local storage is not supported");
      ppc_gvar_tag_initializer(var, init, tok);
      continue;
    }

    // Generate code for computing a VLA size. We need to do this
    // even if ty is not VLA because ty may be a pointer to VLA
    // (e.g. int (*foo)[n][m] where n and m are variables.)
    cur = cur->next = new_unary(ND_EXPR_STMT, compute_vla_size(ty, tok), tok);

    if (ty->kind == TY_VLA) {
      if (equal(tok, "="))
        error_tok(tok, "variable-sized object may not be initialized");

      // Variable length arrays (VLAs) are translated to alloca() calls.
      // For example, `int x[n+2]` is translated to `tmp = n + 2,
      // x = alloca(tmp)`.
      Obj *var = new_lvar(get_ident(ty->name), ty);
      Token *tok = ty->name;
      Node *expr = new_binary(ND_ASSIGN, new_vla_ptr(var, tok),
                              new_alloca(new_var_node(ty->vla_size, tok)),
                              tok);

      cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
      continue;
    }

    Obj *var = new_lvar(get_ident(ty->name), ty);
    if (attr && attr->align)
      var->align = attr->align;

    if (equal(tok, "=")) {
      Node *expr = lvar_initializer(&tok, tok->next, var);
      cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
    } else {
      Node *expr = ppc_tag_initializer(var, tok);
      if (expr)
        cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
    }

    if (var->ty->size < 0)
      error_tok(ty->name, "variable has incomplete type");
    if (var->ty->kind == TY_VOID)
      error_tok(ty->name, "variable declared void");
  }

  Node *node = new_node(ND_BLOCK, tok);
  node->body = head.next;
  *rest = tok->next;
  return node;
}

static Token *skip_excess_element(Token *tok) {
  if (equal(tok, "{")) {
    tok = skip_excess_element(tok->next);
    return skip(tok, "}");
  }

  assign(&tok, tok);
  return tok;
}

// string-initializer = string-literal
static void string_initializer(Token **rest, Token *tok, Initializer *init) {
  if (init->is_flexible)
    *init = *new_initializer(array_of(init->ty->base, tok->ty->array_len), false);

  int len = MIN(init->ty->array_len, tok->ty->array_len);

  switch (init->ty->base->size) {
  case 1: {
    char *str = tok->str;
    for (int i = 0; i < len; i++)
      init->children[i]->expr = new_num(str[i], tok);
    break;
  }
  case 2: {
    uint16_t *str = (uint16_t *)tok->str;
    for (int i = 0; i < len; i++)
      init->children[i]->expr = new_num(str[i], tok);
    break;
  }
  case 4: {
    uint32_t *str = (uint32_t *)tok->str;
    for (int i = 0; i < len; i++)
      init->children[i]->expr = new_num(str[i], tok);
    break;
  }
  default:
    unreachable();
  }

  *rest = tok->next;
}

// array-designator = "[" const-expr "]"
//
// C99 added the designated initializer to the language, which allows
// programmers to move the "cursor" of an initializer to any element.
// The syntax looks like this:
//
//   int x[10] = { 1, 2, [5]=3, 4, 5, 6, 7 };
//
// `[5]` moves the cursor to the 5th element, so the 5th element of x
// is set to 3. Initialization then continues forward in order, so
// 6th, 7th, 8th and 9th elements are initialized with 4, 5, 6 and 7,
// respectively. Unspecified elements (in this case, 3rd and 4th
// elements) are initialized with zero.
//
// Nesting is allowed, so the following initializer is valid:
//
//   int x[5][10] = { [5][8]=1, 2, 3 };
//
// It sets x[5][8], x[5][9] and x[6][0] to 1, 2 and 3, respectively.
//
// Use `.fieldname` to move the cursor for a struct initializer. E.g.
//
//   struct { int a, b, c; } x = { .c=5 };
//
// The above initializer sets x.c to 5.
static void array_designator(Token **rest, Token *tok, Type *ty, int *begin, int *end) {
  *begin = const_expr(&tok, tok->next);
  if (*begin >= ty->array_len)
    error_tok(tok, "array designator index exceeds array bounds");

  if (equal(tok, "...")) {
    *end = const_expr(&tok, tok->next);
    if (*end >= ty->array_len)
      error_tok(tok, "array designator index exceeds array bounds");
    if (*end < *begin)
      error_tok(tok, "array designator range [%d, %d] is empty", *begin, *end);
  } else {
    *end = *begin;
  }

  *rest = skip(tok, "]");
}

// struct-designator = "." ident
static Member *struct_designator(Token **rest, Token *tok, Type *ty) {
  Token *start = tok;
  tok = skip(tok, ".");
  Member *zero = equal(tok, "void") ? get_struct_member(ty, tok) : NULL;
  if (tok->kind != TK_IDENT && !(zero && zero->is_ppc_pointer_zero_member))
    error_tok(tok, "expected a field designator");

  for (Member *mem = ty->members; mem; mem = mem->next) {
    if (mem->is_ppc_tag)
      continue;

    // Anonymous struct member
    if (mem->ty->kind == TY_STRUCT && !mem->name) {
      if (get_struct_member(mem->ty, tok)) {
        *rest = start;
        return mem;
      }
      continue;
    }

    // Regular struct member
    if (mem->name && mem->name->len == tok->len && !strncmp(mem->name->loc, tok->loc, tok->len)) {
      *rest = tok->next;
      return mem;
    }
  }

  error_tok(tok, "struct has no such member");
}

// designation = ("[" const-expr "]" | "." ident)* "="? initializer
static void designation(Token **rest, Token *tok, Initializer *init) {
  if (equal(tok, "[")) {
    if (init->ty->kind != TY_ARRAY)
      error_tok(tok, "array index in non-array initializer");

    int begin, end;
    array_designator(&tok, tok, init->ty, &begin, &end);

    Token *tok2;
    for (int i = begin; i <= end; i++)
      designation(&tok2, tok, init->children[i]);
    array_initializer2(rest, tok2, init, begin + 1);
    return;
  }

  if (equal(tok, ".") && init->ty->kind == TY_STRUCT) {
    Member *mem = struct_designator(&tok, tok, init->ty);
    designation(&tok, tok, init->children[mem->idx]);
    init->expr = NULL;
    struct_initializer2(rest, tok, init, next_user_member(mem->next));
    return;
  }

  if (equal(tok, ".") && init->ty->kind == TY_UNION) {
    Member *mem = struct_designator(&tok, tok, init->ty);
    init->mem = mem;
    designation(rest, tok, init->children[mem->idx]);
    return;
  }

  if (equal(tok, "."))
    error_tok(tok, "field name not in struct or union initializer");

  if (equal(tok, "="))
    tok = tok->next;
  initializer2(rest, tok, init);
}

// An array length can be omitted if an array has an initializer
// (e.g. `int x[] = {1,2,3}`). If it's omitted, count the number
// of initializer elements.
static int count_array_init_elements(Token *tok, Type *ty) {
  bool first = true;
  Initializer *dummy = new_initializer(ty->base, true);

  int i = 0, max = 0;

  while (!consume_end(&tok, tok)) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    if (equal(tok, "[")) {
      i = const_expr(&tok, tok->next);
      if (equal(tok, "..."))
        i = const_expr(&tok, tok->next);
      tok = skip(tok, "]");
      designation(&tok, tok, dummy);
    } else {
      initializer2(&tok, tok, dummy);
    }

    i++;
    max = MAX(max, i);
  }
  return max;
}

// array-initializer1 = "{" initializer ("," initializer)* ","? "}"
static void array_initializer1(Token **rest, Token *tok, Initializer *init) {
  tok = skip(tok, "{");

  if (init->is_flexible) {
    int len = count_array_init_elements(tok, init->ty);
    *init = *new_initializer(array_of(init->ty->base, len), false);
  }

  bool first = true;

  if (init->is_flexible) {
    int len = count_array_init_elements(tok, init->ty);
    *init = *new_initializer(array_of(init->ty->base, len), false);
  }

  for (int i = 0; !consume_end(rest, tok); i++) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    if (equal(tok, "[")) {
      int begin, end;
      array_designator(&tok, tok, init->ty, &begin, &end);

      Token *tok2;
      for (int j = begin; j <= end; j++)
        designation(&tok2, tok, init->children[j]);
      tok = tok2;
      i = end;
      continue;
    }

    if (i < init->ty->array_len)
      initializer2(&tok, tok, init->children[i]);
    else
      tok = skip_excess_element(tok);
  }
}

// array-initializer2 = initializer ("," initializer)*
static void array_initializer2(Token **rest, Token *tok, Initializer *init, int i) {
  if (init->is_flexible) {
    int len = count_array_init_elements(tok, init->ty);
    *init = *new_initializer(array_of(init->ty->base, len), false);
  }

  for (; i < init->ty->array_len && !is_end(tok); i++) {
    Token *start = tok;
    if (i > 0)
      tok = skip(tok, ",");

    if (equal(tok, "[") || equal(tok, ".")) {
      *rest = start;
      return;
    }

    initializer2(&tok, tok, init->children[i]);
  }
  *rest = tok;
}

// struct-initializer1 = "{" initializer ("," initializer)* ","? "}"
static void struct_initializer1(Token **rest, Token *tok, Initializer *init) {
  tok = skip(tok, "{");

  Member *mem = next_user_member(init->ty->members);
  bool first = true;

  while (!consume_end(rest, tok)) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    if (equal(tok, ".")) {
      mem = struct_designator(&tok, tok, init->ty);
      designation(&tok, tok, init->children[mem->idx]);
      mem = next_user_member(mem->next);
      continue;
    }

    if (mem) {
      initializer2(&tok, tok, init->children[mem->idx]);
      mem = next_user_member(mem->next);
    } else {
      tok = skip_excess_element(tok);
    }
  }
}

// struct-initializer2 = initializer ("," initializer)*
static void struct_initializer2(Token **rest, Token *tok, Initializer *init, Member *mem) {
  bool first = true;

  for (mem = next_user_member(mem); mem && !is_end(tok); mem = next_user_member(mem->next)) {
    Token *start = tok;

    if (!first) {
      tok = skip(tok, ",");
    } else if (equal(tok, ",")) {
      if (equal(tok->next, "[") || equal(tok->next, ".") || is_end(tok->next)) {
        *rest = start;
        return;
      }
      tok = tok->next;
    }
    first = false;

    if (equal(tok, "[") || equal(tok, ".")) {
      *rest = start;
      return;
    }

    initializer2(&tok, tok, init->children[mem->idx]);
  }
  *rest = tok;
}

static void union_initializer(Token **rest, Token *tok, Initializer *init) {
  // Unlike structs, union initializers take only one initializer,
  // and that initializes the first union member by default.
  // You can initialize other member using a designated initializer.
  if (equal(tok, "{") && equal(tok->next, ".")) {
    Member *mem = struct_designator(&tok, tok->next, init->ty);
    init->mem = mem;
    designation(&tok, tok, init->children[mem->idx]);
    *rest = skip(tok, "}");
    return;
  }

  init->mem = init->ty->members;

  if (equal(tok, "{")) {
    initializer2(&tok, tok->next, init->children[0]);
    consume(&tok, tok, ",");
    *rest = skip(tok, "}");
  } else {
    initializer2(rest, tok, init->children[0]);
  }
}

// initializer = string-initializer | array-initializer
//             | struct-initializer | union-initializer
//             | assign
static void initializer2(Token **rest, Token *tok, Initializer *init) {
  if (init->ty->kind == TY_ARRAY && tok->kind == TK_STR) {
    string_initializer(rest, tok, init);
    return;
  }

  if (init->ty->kind == TY_ARRAY) {
    if (equal(tok, "{"))
      array_initializer1(rest, tok, init);
    else
      array_initializer2(rest, tok, init, 0);
    return;
  }

  if (init->ty->kind == TY_STRUCT) {
    Token *end;
    bool braced = equal(tok, "{");
    Member *feature = ppc_read_enum_feature(&end, braced ? tok->next : tok, init->ty);
    if (feature) {
      if (braced && equal(end, ","))
        end = end->next;
      if (!braced || equal(end, "}")) {
        ppc_check_enum_feature_target(init->ty, tok);
        init->ppc_enum_feature = feature;
        *rest = braced ? end->next : end;
        return;
      }
    }
    if (equal(tok, "{")) {
      struct_initializer1(rest, tok, init);
      return;
    }

    // A struct can be initialized with another struct. E.g.
    // `struct T x = y;` where y is a variable of type `struct T`.
    // Handle that case first.
    Node *expr = assign(rest, tok);
    add_type(expr);
    if (expr->ty->kind == TY_STRUCT) {
      if (!is_assignable(init->ty, expr->ty))
        error_tok(expr->tok, "incompatible struct initializer");
      init->expr = expr;
      return;
    }

    struct_initializer2(rest, tok, init, next_user_member(init->ty->members));
    return;
  }

  if (init->ty->kind == TY_UNION) {
    union_initializer(rest, tok, init);
    return;
  }

  if (equal(tok, "{")) {
    // An initializer for a scalar variable can be surrounded by
    // braces. E.g. `int x = {3};`. Handle that case.
    if (init->ty->kind == TY_NULLPTR && equal(tok->next, "}")) {
      init->expr = new_cast(new_num(0, tok), ty_nullptr);
      *rest = tok->next->next;
      return;
    }
    initializer2(&tok, tok->next, init);
    *rest = skip(tok, "}");
    return;
  }

  init->expr = assign(rest, tok);
  add_type(init->expr);
  if (init->ty->kind == TY_NULLPTR || init->expr->ty->kind == TY_NULLPTR)
    init->expr = new_cast(init->expr, init->ty);
}

static Type *copy_struct_type(Type *ty) {
  ty = copy_type(ty);

  Member head = {};
  Member *cur = &head;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    Member *m = calloc(1, sizeof(Member));
    *m = *mem;
    cur = cur->next = m;
  }

  ty->members = head.next;
  return ty;
}

static Initializer *initializer(Token **rest, Token *tok, Type *ty, Type **new_ty) {
  Initializer *init = new_initializer(ty, true);
  initializer2(rest, tok, init);

  if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) && ty->is_flexible) {
    ty = copy_struct_type(ty);

    Member *mem = ty->members;
    while (mem->next)
      mem = mem->next;
    mem->ty = init->children[mem->idx]->ty;
    ty->size += mem->ty->size;

    *new_ty = ty;
    return init;
  }

  *new_ty = init->ty;
  return init;
}

static Node *init_desg_expr(InitDesg *desg, Token *tok) {
  if (desg->var) {
    Node *node = new_var_node(desg->var, tok);
    if (desg->is_deref)
      node = new_unary(ND_DEREF, node, tok);
    return node;
  }

  if (desg->member) {
    Node *node = new_unary(ND_MEMBER, init_desg_expr(desg->next, tok), tok);
    node->member = desg->member;
    return node;
  }

  Node *lhs = init_desg_expr(desg->next, tok);
  Node *rhs = new_num(desg->idx, tok);
  return new_unary(ND_DEREF, new_add(lhs, rhs, tok), tok);
}

static Node *create_lvar_init(Initializer *init, Type *ty, InitDesg *desg, Token *tok) {
  if (ty->kind == TY_ARRAY) {
    Node *node = new_node(ND_NULL_EXPR, tok);
    for (int i = 0; i < ty->array_len; i++) {
      InitDesg desg2 = {desg, i};
      Node *rhs = create_lvar_init(init->children[i], ty->base, &desg2, tok);
      node = new_binary(ND_COMMA, node, rhs, tok);
    }
    return node;
  }

  if (ty->kind == TY_STRUCT && !init->expr) {
    Node *node = new_node(ND_NULL_EXPR, tok);

    for (Member *mem = ty->members; mem; mem = mem->next) {
      InitDesg desg2 = {desg, 0, mem};

      if (mem->is_ppc_tag) {
        Node *lhs = init_desg_expr(&desg2, tok);
        Member *feature = init->ppc_enum_feature;
        Node *rhs = ppc_tag_value(feature ? feature : mem, tok);
        node = new_binary(ND_COMMA, node, new_binary(ND_ASSIGN, lhs, rhs, tok), tok);
        continue;
      }

      Node *rhs = create_lvar_init(init->children[mem->idx], mem->ty, &desg2, tok);
      node = new_binary(ND_COMMA, node, rhs, tok);
    }
    return node;
  }

  if (ty->kind == TY_UNION) {
    Member *mem = init->mem ? init->mem : ty->members;
    InitDesg desg2 = {desg, 0, mem};
    return create_lvar_init(init->children[mem->idx], mem->ty, &desg2, tok);
  }

  if (!init->expr)
    return new_node(ND_NULL_EXPR, tok);

  Node *lhs = init_desg_expr(desg, tok);
  return new_binary(ND_ASSIGN, lhs, init->expr, tok);
}

// A variable definition with an initializer is a shorthand notation
// for a variable definition followed by assignments. This function
// generates assignment expressions for an initializer. For example,
// `int x[2][2] = {{6, 7}, {8, 9}}` is converted to the following
// expressions:
//
//   x[0][0] = 6;
//   x[0][1] = 7;
//   x[1][0] = 8;
//   x[1][1] = 9;
static Node *lvar_initializer(Token **rest, Token *tok, Obj *var) {
  Initializer *init = initializer(rest, tok, var->ty, &var->ty);
  InitDesg desg = {NULL, 0, NULL, var};

  // If a partial initializer list is given, the standard requires
  // that unspecified elements are set to 0. Here, we simply
  // zero-initialize the entire memory region of a variable before
  // initializing it with user-supplied values.
  Node *lhs = new_node(ND_MEMZERO, tok);
  lhs->var = var;

  Node *rhs = create_lvar_init(init, var->ty, &desg, tok);
  return new_binary(ND_COMMA, lhs, rhs, tok);
}

static Obj *new_generated_lvar(Obj **list, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = "";
  var->ty = ty;
  var->align = ty->align;
  var->is_local = true;
  var->next = *list;
  *list = var;
  return var;
}

static Obj *new_generated_func(char *name, Type *ty, int priority) {
  Obj *fn = new_gvar(name, ty);
  fn->is_function = true;
  fn->is_definition = true;
  fn->is_static = true;
  fn->is_root = true;
  fn->is_live = true;
  fn->init_priority = priority;
  return fn;
}

static Node *new_block_node(Node *body, Token *tok) {
  Node *node = new_node(ND_BLOCK, tok);
  node->body = body;
  return node;
}

static Node *ppc_tag_value(Member *mem, Token *tok) {
  if (mem->ppc_tag_var)
    return new_var_node(mem->ppc_tag_var, tok);
  return new_num(0, tok);
}

static Type *ppc_enum_family(Type *ty) {
  while (ty->origin)
    ty = ty->origin;
  Type *base = ty->ppc_base ? ty->ppc_base : ty;
  return base->is_ppc_enum_generalization ? base : NULL;
}

// Feature values use Generalization.feature, separately from ordinary C names.
// The same spelling in a type context still denotes a specialization type.
static Member *ppc_read_enum_feature(Token **rest, Token *tok, Type *ty) {
  Type *base = ppc_enum_family(ty);
  if (!base)
    return NULL;
  if (equal(tok, "(")) {
    Token *end;
    Member *feature = ppc_read_enum_feature(&end, tok->next, ty);
    if (feature && equal(end, ")")) {
      *rest = end->next;
      return feature;
    }
    return NULL;
  }
  Type *qualifier = NULL;
  if (equal(tok, "struct") && tok->next->kind == TK_IDENT) {
    qualifier = find_tag(tok->next);
    tok = tok->next->next;
  } else if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    qualifier = sc ? sc->type_def : find_tag(tok);
    tok = tok->next;
  }
  if (!qualifier || !equal(tok, "."))
    return NULL;
  while (qualifier->origin)
    qualifier = qualifier->origin;
  if (!qualifier->is_ppc_enum_generalization)
    return NULL;
  if (qualifier != base)
    error_tok(tok, "PPC enum feature belongs to a different generalization");
  Member *feature = find_ppc_spec(base, tok->next);
  if (!feature)
    error_tok(tok->next, "unknown PPC enum feature");
  *rest = tok->next->next;
  return feature;
}

static void ppc_check_enum_feature_target(Type *ty, Token *tok) {
  while (ty->origin)
    ty = ty->origin;
  Type *base = ppc_enum_family(ty);
  Member *tag = ppc_tag_member(ty);
  if (!base || ty->size != base->size || ty->align != base->align ||
      (ty != base && (!tag || tag->ppc_index != 0)))
    error_tok(tok, "PPC enum feature requires a zero-specialization target");
}

static Node *ppc_enum_feature_assignment(Node *lhs, Member *feature, Token *tok) {
  ppc_check_enum_feature_target(lhs->ty, tok);
  if (!current_fn)
    error_tok(tok, "PPC enum feature assignment is not a constant initializer");

  // Keep the address so array indices and other lvalue side effects run once.
  Obj *ptr = new_lvar("", pointer_to(lhs->ty));
  Node *save = new_binary(ND_ASSIGN, new_var_node(ptr, tok),
                         new_unary(ND_ADDR, lhs, tok), tok);
  Node *tag = new_unary(ND_MEMBER,
                       new_unary(ND_DEREF, new_var_node(ptr, tok), tok), tok);
  tag->member = ppc_tag_member(lhs->ty);
  Node *set = new_binary(ND_ASSIGN, tag, ppc_tag_value(feature, tok), tok);
  Node *value = new_unary(ND_DEREF, new_var_node(ptr, tok), tok);
  return new_binary(ND_COMMA, save, new_binary(ND_COMMA, set, value, tok), tok);
}

static bool ppc_type_has_tag(Type *ty, bool runtime_only) {
  if (ty->kind == TY_ARRAY)
    return ppc_type_has_tag(ty->base, runtime_only);

  if (ty->kind != TY_STRUCT)
    return false;

  for (Member *mem = ty->members; mem; mem = mem->next) {
    if (mem->is_ppc_tag) {
      if (!runtime_only || mem->ppc_tag_var)
        return true;
      continue;
    }

    if (ppc_type_has_tag(mem->ty, runtime_only))
      return true;
  }

  return false;
}

static Node *ppc_tag_initializer2(Type *ty, InitDesg *desg, Token *tok, Initializer *init) {
  Node *node = new_node(ND_NULL_EXPR, tok);

  if (ty->kind == TY_ARRAY) {
    for (int i = 0; i < ty->array_len; i++) {
      InitDesg desg2 = {desg, i};
      Node *rhs = ppc_tag_initializer2(ty->base, &desg2, tok, init ? init->children[i] : NULL);
      node = new_binary(ND_COMMA, node, rhs, tok);
    }
    return node;
  }

  if (ty->kind == TY_UNION && init && init->mem) {
    Member *mem = init->mem;
    InitDesg desg2 = {desg, 0, mem};
    return ppc_tag_initializer2(mem->ty, &desg2, tok, init->children[mem->idx]);
  }

  if (ty->kind != TY_STRUCT)
    return node;

  for (Member *mem = ty->members; mem; mem = mem->next) {
    InitDesg desg2 = {desg, 0, mem};

    if (mem->is_ppc_tag) {
      Node *lhs = init_desg_expr(&desg2, tok);
      Member *feature = init ? init->ppc_enum_feature : NULL;
      Node *rhs = ppc_tag_value(feature ? feature : mem, tok);
      node = new_binary(ND_COMMA, node, new_binary(ND_ASSIGN, lhs, rhs, tok), tok);
      continue;
    }

    Node *rhs = ppc_tag_initializer2(mem->ty, &desg2, tok,
                                     init ? init->children[mem->idx] : NULL);
    node = new_binary(ND_COMMA, node, rhs, tok);
  }

  return node;
}

static Node *ppc_tag_initializer(Obj *var, Token *tok) {
  if (!ppc_type_has_tag(var->ty, false))
    return NULL;

  InitDesg var_desg = {NULL, 0, NULL, var};
  return ppc_tag_initializer2(var->ty, &var_desg, tok, NULL);
}

static bool ppc_initializer_has_enum_feature(Initializer *init) {
  if (!init)
    return false;
  if (init->ppc_enum_feature)
    return true;
  if (init->ty->kind == TY_ARRAY) {
    for (int i = 0; i < init->ty->array_len; i++)
      if (ppc_initializer_has_enum_feature(init->children[i]))
        return true;
  } else if (init->ty->kind == TY_STRUCT || init->ty->kind == TY_UNION) {
    for (Member *mem = init->ty->members; mem; mem = mem->next)
      if (ppc_initializer_has_enum_feature(init->children[mem->idx]))
        return true;
  }
  return false;
}

static void ppc_gvar_tag_initializer(Obj *var, Initializer *init, Token *tok) {
  if (!ppc_type_has_tag(var->ty, true) && !ppc_initializer_has_enum_feature(init))
    return;

  static int id = 0;
  Obj *fn = new_generated_func(format("__ppc_ctor_401_gvar_%d", id++),
                               func_type(ty_void), 401);

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));

  InitDesg var_desg = {NULL, 0, NULL, var};
  Node *stmt = new_expr_stmt_node(ppc_tag_initializer2(var->ty, &var_desg, tok, init), tok);

  fn->body = new_block_node(stmt, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static uint64_t read_buf(char *buf, int sz) {
  if (sz == 1)
    return *buf;
  if (sz == 2)
    return *(uint16_t *)buf;
  if (sz == 4)
    return *(uint32_t *)buf;
  if (sz == 8)
    return *(uint64_t *)buf;
  unreachable();
}

static void write_buf(char *buf, uint64_t val, int sz) {
  if (sz == 1)
    *buf = val;
  else if (sz == 2)
    *(uint16_t *)buf = val;
  else if (sz == 4)
    *(uint32_t *)buf = val;
  else if (sz == 8)
    *(uint64_t *)buf = val;
  else
    unreachable();
}

static Relocation *
write_gvar_data(Relocation *cur, Initializer *init, Type *ty, char *buf, int offset) {
  if (ty->kind == TY_ARRAY) {
    int sz = ty->base->size;
    for (int i = 0; i < ty->array_len; i++)
      cur = write_gvar_data(cur, init->children[i], ty->base, buf, offset + sz * i);
    return cur;
  }

  if (ty->kind == TY_STRUCT) {
    for (Member *mem = ty->members; mem; mem = mem->next) {
      if (mem->is_ppc_tag) {
        write_buf(buf + offset + mem->offset, 0, mem->ty->size);
        continue;
      }

      if (mem->is_bitfield) {
        Node *expr = init->children[mem->idx]->expr;
        if (!expr)
          break;

        char *loc = buf + offset + mem->offset;
        uint64_t oldval = read_buf(loc, mem->ty->size);
        uint64_t newval = eval(expr);
        uint64_t mask = (1L << mem->bit_width) - 1;
        uint64_t combined = oldval | ((newval & mask) << mem->bit_offset);
        write_buf(loc, combined, mem->ty->size);
      } else {
        cur = write_gvar_data(cur, init->children[mem->idx], mem->ty, buf,
                              offset + mem->offset);
      }
    }
    return cur;
  }

  if (ty->kind == TY_UNION) {
    if (!init->mem)
      return cur;
    return write_gvar_data(cur, init->children[init->mem->idx],
                           init->mem->ty, buf, offset);
  }

  if (!init->expr)
    return cur;

  if (ty->kind == TY_FLOAT) {
    *(float *)(buf + offset) = eval_double(init->expr);
    return cur;
  }

  if (ty->kind == TY_DOUBLE) {
    *(double *)(buf + offset) = eval_double(init->expr);
    return cur;
  }

  char **label = NULL;
  uint64_t val = eval2(init->expr, &label);

  if (!label) {
    write_buf(buf + offset, val, ty->size);
    return cur;
  }

  Relocation *rel = calloc(1, sizeof(Relocation));
  rel->offset = offset;
  rel->label = label;
  rel->addend = val;
  cur->next = rel;
  return cur->next;
}

// Initializers for global variables are evaluated at compile-time and
// embedded to .data section. This function serializes Initializer
// objects to a flat byte array. It is a compile error if an
// initializer list contains a non-constant expression.
static Initializer *gvar_initializer(Token **rest, Token *tok, Obj *var) {
  Initializer *init = initializer(rest, tok, var->ty, &var->ty);
  if (var->is_tls && ppc_initializer_has_enum_feature(init))
    error_tok(tok, "PPC enum feature initialization of thread-local storage is not supported");

  Relocation head = {};
  char *buf = calloc(1, var->ty->size);
  write_gvar_data(&head, init, var->ty, buf, 0);
  var->init_data = buf;
  var->rel = head.next;
  return init;
}

// Returns true if a given token represents a type.
static bool is_typename(Token *tok) {
  static HashMap map;

  if (map.capacity == 0) {
    static char *kw[] = {
      "void", "_Bool", "char", "short", "int", "long", "struct", "union",
      "typedef", "enum", "static", "extern", "_Alignas", "signed", "unsigned",
      "const", "volatile", "auto", "register", "restrict", "__restrict",
      "__restrict__", "_Noreturn", "float", "double", "typeof", "inline",
      "_Thread_local", "__thread", "_Atomic",
    };

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++)
      hashmap_put(&map, kw[i], (void *)1);
  }

  return hashmap_get2(&map, tok->loc, tok->len) || find_typedef(tok);
}

// asm-stmt = "asm" ("volatile" | "inline")* "(" string-literal ")"
static Node *asm_stmt(Token **rest, Token *tok) {
  Node *node = new_node(ND_ASM, tok);
  tok = tok->next;

  while (equal(tok, "volatile") || equal(tok, "inline"))
    tok = tok->next;

  tok = skip(tok, "(");
  if (tok->kind != TK_STR || tok->ty->base->kind != TY_CHAR)
    error_tok(tok, "expected string literal");
  node->asm_str = tok->str;
  *rest = skip(tok->next, ")");
  return node;
}

// stmt = "return" expr? ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "switch" "(" expr ")" stmt
//      | "case" const-expr ("..." const-expr)? ":" stmt
//      | "default" ":" stmt
//      | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//      | "while" "(" expr ")" stmt
//      | "do" stmt "while" "(" expr ")" ";"
//      | "asm" asm-stmt
//      | "goto" (ident | "*" expr) ";"
//      | "break" ";"
//      | "continue" ";"
//      | ident ":" stmt
//      | "{" compound-stmt
//      | expr-stmt
static Node *stmt(Token **rest, Token *tok) {
  if (equal(tok, "return")) {
    Node *node = new_node(ND_RETURN, tok);
    if (consume(rest, tok->next, ";"))
      return node;

    Node *exp = expr(&tok, tok->next);
    *rest = skip(tok, ";");

    add_type(exp);
    Type *ty = current_fn->ty->return_ty;
    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
      if (!is_compatible(ty, exp->ty))
        error_tok(exp->tok, "incompatible return type");
    } else {
      exp = new_cast(exp, current_fn->ty->return_ty);
    }

    node->lhs = exp;
    return node;
  }

  if (equal(tok, "if")) {
    Node *node = new_node(ND_IF, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);
    if (equal(tok, "else"))
      node->els = stmt(&tok, tok->next);
    *rest = tok;
    return node;
  }

  if (equal(tok, "switch")) {
    Node *node = new_node(ND_SWITCH, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    add_type(node->cond);
    if (node->cond->ty->kind == TY_NULLPTR)
      error_tok(node->cond->tok, "switch condition must have integer type");
    tok = skip(tok, ")");

    Node *sw = current_switch;
    current_switch = node;

    char *brk = brk_label;
    brk_label = node->brk_label = new_unique_name();

    node->then = stmt(rest, tok);

    current_switch = sw;
    brk_label = brk;
    return node;
  }

  if (equal(tok, "case")) {
    if (!current_switch)
      error_tok(tok, "stray case");

    Node *node = new_node(ND_CASE, tok);
    int begin = const_expr(&tok, tok->next);
    int end;

    if (equal(tok, "...")) {
      // [GNU] Case ranges, e.g. "case 1 ... 5:"
      end = const_expr(&tok, tok->next);
      if (end < begin)
        error_tok(tok, "empty case range specified");
    } else {
      end = begin;
    }

    tok = skip(tok, ":");
    node->label = new_unique_name();
    node->lhs = stmt(rest, tok);
    node->begin = begin;
    node->end = end;
    node->case_next = current_switch->case_next;
    current_switch->case_next = node;
    return node;
  }

  if (equal(tok, "default")) {
    if (!current_switch)
      error_tok(tok, "stray default");

    Node *node = new_node(ND_CASE, tok);
    tok = skip(tok->next, ":");
    node->label = new_unique_name();
    node->lhs = stmt(rest, tok);
    current_switch->default_case = node;
    return node;
  }

  if (equal(tok, "for")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");

    enter_scope();

    char *brk = brk_label;
    char *cont = cont_label;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();

    if (is_typename(tok)) {
      Type *basety = declspec(&tok, tok, NULL);
      node->init = declaration(&tok, tok, basety, NULL);
    } else {
      node->init = expr_stmt(&tok, tok);
    }

    if (!equal(tok, ";"))
      node->cond = expr(&tok, tok);
    tok = skip(tok, ";");

    if (!equal(tok, ")"))
      node->inc = expr(&tok, tok);
    tok = skip(tok, ")");

    node->then = stmt(rest, tok);

    leave_scope();
    brk_label = brk;
    cont_label = cont;
    return node;
  }

  if (equal(tok, "while")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");

    char *brk = brk_label;
    char *cont = cont_label;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();

    node->then = stmt(rest, tok);

    brk_label = brk;
    cont_label = cont;
    return node;
  }

  if (equal(tok, "do")) {
    Node *node = new_node(ND_DO, tok);

    char *brk = brk_label;
    char *cont = cont_label;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();

    node->then = stmt(&tok, tok->next);

    brk_label = brk;
    cont_label = cont;

    tok = skip(tok, "while");
    tok = skip(tok, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    *rest = skip(tok, ";");
    return node;
  }

  if (equal(tok, "asm"))
    return asm_stmt(rest, tok);

  if (equal(tok, "goto")) {
    if (equal(tok->next, "*")) {
      // [GNU] `goto *ptr` jumps to the address specified by `ptr`.
      Node *node = new_node(ND_GOTO_EXPR, tok);
      node->lhs = expr(&tok, tok->next->next);
      *rest = skip(tok, ";");
      return node;
    }

    Node *node = new_node(ND_GOTO, tok);
    node->label = get_ident(tok->next);
    node->goto_next = gotos;
    gotos = node;
    *rest = skip(tok->next->next, ";");
    return node;
  }

  if (equal(tok, "break")) {
    if (!brk_label)
      error_tok(tok, "stray break");
    Node *node = new_node(ND_GOTO, tok);
    node->unique_label = brk_label;
    *rest = skip(tok->next, ";");
    return node;
  }

  if (equal(tok, "continue")) {
    if (!cont_label)
      error_tok(tok, "stray continue");
    Node *node = new_node(ND_GOTO, tok);
    node->unique_label = cont_label;
    *rest = skip(tok->next, ";");
    return node;
  }

  if (tok->kind == TK_IDENT && equal(tok->next, ":")) {
    Node *node = new_node(ND_LABEL, tok);
    node->label = strndup(tok->loc, tok->len);
    node->unique_label = new_unique_name();
    node->lhs = stmt(rest, tok->next->next);
    node->goto_next = labels;
    labels = node;
    return node;
  }

  if (equal(tok, "{"))
    return compound_stmt(rest, tok->next);

  return expr_stmt(rest, tok);
}

// compound-stmt = (typedef | declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok) {
  Node *node = new_node(ND_BLOCK, tok);
  Node head = {};
  Node *cur = &head;

  enter_scope();

  while (!equal(tok, "}")) {
    if (is_typename(tok) && !equal(tok->next, ":")) {
      VarAttr attr = {};
      Type *basety = declspec(&tok, tok, &attr);

      if (attr.is_typedef) {
        tok = parse_typedef(tok, basety);
        continue;
      }

      if (is_ppc_function(tok)) {
        tok = ppc_function(tok, basety, &attr);
        continue;
      }

      if (is_function(tok)) {
        tok = function(tok, basety, &attr);
        continue;
      }

      if (attr.is_extern) {
        tok = global_variable(tok, basety, &attr);
        continue;
      }

      cur = cur->next = declaration(&tok, tok, basety, &attr);
    } else {
      cur = cur->next = stmt(&tok, tok);
    }
    add_type(cur);
  }

  leave_scope();

  node->body = head.next;
  *rest = tok->next;
  return node;
}

// expr-stmt = expr? ";"
static Node *expr_stmt(Token **rest, Token *tok) {
  if (equal(tok, ";")) {
    *rest = tok->next;
    return new_node(ND_BLOCK, tok);
  }

  Node *node = new_node(ND_EXPR_STMT, tok);
  node->lhs = expr(&tok, tok);
  *rest = skip(tok, ";");
  return node;
}

// expr = assign ("," expr)?
static Node *expr(Token **rest, Token *tok) {
  Node *node = assign(&tok, tok);

  if (equal(tok, ","))
    return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);

  *rest = tok;
  return node;
}

static int64_t eval(Node *node) {
  return eval2(node, NULL);
}

// Evaluate a given node as a constant expression.
//
// A constant expression is either just a number or ptr+n where ptr
// is a pointer to a global variable and n is a postiive/negative
// number. The latter form is accepted only as an initialization
// expression for a global variable.
static int64_t eval2(Node *node, char ***label) {
  add_type(node);

  if (is_flonum(node->ty))
    return eval_double(node);

  switch (node->kind) {
  case ND_ADD:
    return eval2(node->lhs, label) + eval(node->rhs);
  case ND_SUB:
    return eval2(node->lhs, label) - eval(node->rhs);
  case ND_MUL:
    return eval(node->lhs) * eval(node->rhs);
  case ND_DIV:
    if (node->ty->is_unsigned)
      return (uint64_t)eval(node->lhs) / eval(node->rhs);
    return eval(node->lhs) / eval(node->rhs);
  case ND_NEG:
    return -eval(node->lhs);
  case ND_MOD:
    if (node->ty->is_unsigned)
      return (uint64_t)eval(node->lhs) % eval(node->rhs);
    return eval(node->lhs) % eval(node->rhs);
  case ND_BITAND:
    return eval(node->lhs) & eval(node->rhs);
  case ND_BITOR:
    return eval(node->lhs) | eval(node->rhs);
  case ND_BITXOR:
    return eval(node->lhs) ^ eval(node->rhs);
  case ND_SHL:
    return eval(node->lhs) << eval(node->rhs);
  case ND_SHR:
    if (node->ty->is_unsigned && node->ty->size == 8)
      return (uint64_t)eval(node->lhs) >> eval(node->rhs);
    return eval(node->lhs) >> eval(node->rhs);
  case ND_EQ:
    return eval(node->lhs) == eval(node->rhs);
  case ND_NE:
    return eval(node->lhs) != eval(node->rhs);
  case ND_LT:
    if (node->lhs->ty->is_unsigned)
      return (uint64_t)eval(node->lhs) < eval(node->rhs);
    return eval(node->lhs) < eval(node->rhs);
  case ND_LE:
    if (node->lhs->ty->is_unsigned)
      return (uint64_t)eval(node->lhs) <= eval(node->rhs);
    return eval(node->lhs) <= eval(node->rhs);
  case ND_COND:
    return eval(node->cond) ? eval2(node->then, label) : eval2(node->els, label);
  case ND_COMMA:
    return eval2(node->rhs, label);
  case ND_NOT:
    return !eval(node->lhs);
  case ND_BITNOT:
    return ~eval(node->lhs);
  case ND_LOGAND:
    return eval(node->lhs) && eval(node->rhs);
  case ND_LOGOR:
    return eval(node->lhs) || eval(node->rhs);
  case ND_CAST: {
    if (node->ty->kind == TY_BOOL && is_flonum(node->lhs->ty))
      return eval_double(node->lhs) != 0;
    int64_t val = eval2(node->lhs, label);
    if (node->ty->kind == TY_BOOL)
      return val != 0;
    if (is_integer(node->ty)) {
      switch (node->ty->size) {
      case 1: return node->ty->is_unsigned ? (uint8_t)val : (int8_t)val;
      case 2: return node->ty->is_unsigned ? (uint16_t)val : (int16_t)val;
      case 4: return node->ty->is_unsigned ? (uint32_t)val : (int32_t)val;
      }
    }
    return val;
  }
  case ND_ADDR:
    return eval_rval(node->lhs, label);
  case ND_LABEL_VAL:
    *label = &node->unique_label;
    return 0;
  case ND_MEMBER:
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    if (node->ty->kind != TY_ARRAY)
      error_tok(node->tok, "invalid initializer");
    return eval_rval(node->lhs, label) + node->member->offset;
  case ND_VAR:
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    if (node->var->ty->kind != TY_ARRAY && node->var->ty->kind != TY_FUNC)
      error_tok(node->tok, "invalid initializer");
    *label = &node->var->name;
    return 0;
  case ND_NUM:
    return node->val;
  }

  error_tok(node->tok, "not a compile-time constant");
}

static int64_t eval_rval(Node *node, char ***label) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local)
      error_tok(node->tok, "not a compile-time constant");
    *label = &node->var->name;
    return 0;
  case ND_DEREF:
    return eval2(node->lhs, label);
  case ND_MEMBER:
    return eval_rval(node->lhs, label) + node->member->offset;
  }

  error_tok(node->tok, "invalid initializer");
}

static bool is_const_expr(Node *node) {
  add_type(node);

  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
  case ND_SHL:
  case ND_SHR:
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
  case ND_LOGAND:
  case ND_LOGOR:
    return is_const_expr(node->lhs) && is_const_expr(node->rhs);
  case ND_COND:
    if (!is_const_expr(node->cond))
      return false;
    return is_const_expr(eval(node->cond) ? node->then : node->els);
  case ND_COMMA:
    return is_const_expr(node->rhs);
  case ND_NEG:
  case ND_NOT:
  case ND_BITNOT:
  case ND_CAST:
    return is_const_expr(node->lhs);
  case ND_NUM:
    return true;
  }

  return false;
}

// This is deliberately narrower than is_const_expr: a folded pointer,
// variable, or comma expression is not an integer null pointer constant.
static bool is_integer_constant_expr(Node *node, bool unevaluated) {
  if (!is_integer(node->ty))
    return false;

  switch (node->kind) {
  case ND_NUM:
    return true;
  case ND_NEG:
  case ND_NOT:
  case ND_BITNOT:
    return is_integer_constant_expr(node->lhs, unevaluated);
  case ND_CAST:
    return is_integer_constant_expr(node->lhs, unevaluated) ||
           (node->lhs->kind == ND_NUM && is_flonum(node->lhs->ty));
  case ND_COND: {
    if (!is_integer_constant_expr(node->cond, unevaluated))
      return false;
    bool cond = !unevaluated && eval(node->cond);
    return is_integer_constant_expr(node->then, unevaluated || !cond) &&
           is_integer_constant_expr(node->els, unevaluated || cond);
  }
  case ND_LOGAND:
  case ND_LOGOR: {
    if (!is_integer_constant_expr(node->lhs, unevaluated))
      return false;
    bool skip = unevaluated || (node->kind == ND_LOGAND ?
                               !eval(node->lhs) : eval(node->lhs));
    return is_integer_constant_expr(node->rhs, skip);
  }
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
  case ND_SHL:
  case ND_SHR:
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    if (!is_integer_constant_expr(node->lhs, unevaluated) ||
        !is_integer_constant_expr(node->rhs, unevaluated))
      return false;
    if (!unevaluated) {
      if ((node->kind == ND_DIV || node->kind == ND_MOD) &&
          (!eval(node->rhs) || (!node->ty->is_unsigned &&
           eval(node->lhs) == INT64_MIN && eval(node->rhs) == -1)))
        return false;
      if ((node->kind == ND_SHL || node->kind == ND_SHR) &&
          (eval(node->rhs) < 0 || eval(node->rhs) >= node->lhs->ty->size * 8))
        return false;
    }
    return true;
  }
  return false;
}

bool is_null_pointer_constant(Node *node) {
  add_type(node);
  if (node->kind == ND_NUM && node->ty->kind == TY_NULLPTR)
    return true;
  if (node->kind == ND_CAST && node->ty->kind == TY_PTR &&
      node->ty->base->kind == TY_VOID)
    return is_integer_constant_expr(node->lhs, false) && eval(node->lhs) == 0;
  return is_integer_constant_expr(node, false) && eval(node) == 0;
}

int64_t const_expr(Token **rest, Token *tok) {
  Node *node = conditional(rest, tok);
  add_type(node);
  if (node->ty->kind == TY_NULLPTR)
    error_tok(node->tok, "integer constant expression required");
  return eval(node);
}

static double eval_double(Node *node) {
  add_type(node);

  if (is_integer(node->ty)) {
    if (node->ty->is_unsigned)
      return (unsigned long)eval(node);
    return eval(node);
  }

  switch (node->kind) {
  case ND_ADD:
    return eval_double(node->lhs) + eval_double(node->rhs);
  case ND_SUB:
    return eval_double(node->lhs) - eval_double(node->rhs);
  case ND_MUL:
    return eval_double(node->lhs) * eval_double(node->rhs);
  case ND_DIV:
    return eval_double(node->lhs) / eval_double(node->rhs);
  case ND_NEG:
    return -eval_double(node->lhs);
  case ND_COND:
    return eval_double(node->cond) ? eval_double(node->then) : eval_double(node->els);
  case ND_COMMA:
    return eval_double(node->rhs);
  case ND_CAST:
    if (is_flonum(node->lhs->ty))
      return eval_double(node->lhs);
    return eval(node->lhs);
  case ND_NUM:
    return node->fval;
  }

  error_tok(node->tok, "not a compile-time constant");
}

// Convert op= operators to expressions containing an assignment.
//
// In general, `A op= C` is converted to ``tmp = &A, *tmp = *tmp op B`.
// However, if a given expression is of form `A.x op= C`, the input is
// converted to `tmp = &A, (*tmp).x = (*tmp).x op C` to handle assignments
// to bitfields.
static Node *to_assign(Node *binary) {
  add_type(binary->lhs);
  add_type(binary->rhs);
  Token *tok = binary->tok;

  // Convert `A.x op= C` to `tmp = &A, (*tmp).x = (*tmp).x op C`.
  if (binary->lhs->kind == ND_MEMBER) {
    Obj *var = new_lvar("", pointer_to(binary->lhs->lhs->ty));

    Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok),
                             new_unary(ND_ADDR, binary->lhs->lhs, tok), tok);

    Node *expr2 = new_unary(ND_MEMBER,
                            new_unary(ND_DEREF, new_var_node(var, tok), tok),
                            tok);
    expr2->member = binary->lhs->member;

    Node *expr3 = new_unary(ND_MEMBER,
                            new_unary(ND_DEREF, new_var_node(var, tok), tok),
                            tok);
    expr3->member = binary->lhs->member;

    Node *expr4 = new_binary(ND_ASSIGN, expr2,
                             new_binary(binary->kind, expr3, binary->rhs, tok),
                             tok);

    return new_binary(ND_COMMA, expr1, expr4, tok);
  }

  // If A is an atomic type, Convert `A op= B` to
  //
  // ({
  //   T1 *addr = &A; T2 val = (B); T1 old = *addr; T1 new;
  //   do {
  //    new = old op val;
  //   } while (!atomic_compare_exchange_strong(addr, &old, new));
  //   new;
  // })
  if (binary->lhs->ty->is_atomic) {
    Node head = {};
    Node *cur = &head;

    Obj *addr = new_lvar("", pointer_to(binary->lhs->ty));
    Obj *val = new_lvar("", binary->rhs->ty);
    Obj *old = new_lvar("", binary->lhs->ty);
    Obj *new = new_lvar("", binary->lhs->ty);

    cur = cur->next =
      new_unary(ND_EXPR_STMT,
                new_binary(ND_ASSIGN, new_var_node(addr, tok),
                           new_unary(ND_ADDR, binary->lhs, tok), tok),
                tok);

    cur = cur->next =
      new_unary(ND_EXPR_STMT,
                new_binary(ND_ASSIGN, new_var_node(val, tok), binary->rhs, tok),
                tok);

    cur = cur->next =
      new_unary(ND_EXPR_STMT,
                new_binary(ND_ASSIGN, new_var_node(old, tok),
                           new_unary(ND_DEREF, new_var_node(addr, tok), tok), tok),
                tok);

    Node *loop = new_node(ND_DO, tok);
    loop->brk_label = new_unique_name();
    loop->cont_label = new_unique_name();

    Node *body = new_binary(ND_ASSIGN,
                            new_var_node(new, tok),
                            new_binary(binary->kind, new_var_node(old, tok),
                                       new_var_node(val, tok), tok),
                            tok);

    loop->then = new_node(ND_BLOCK, tok);
    loop->then->body = new_unary(ND_EXPR_STMT, body, tok);

    Node *cas = new_node(ND_CAS, tok);
    cas->cas_addr = new_var_node(addr, tok);
    cas->cas_old = new_unary(ND_ADDR, new_var_node(old, tok), tok);
    cas->cas_new = new_var_node(new, tok);
    loop->cond = new_unary(ND_NOT, cas, tok);

    cur = cur->next = loop;
    cur = cur->next = new_unary(ND_EXPR_STMT, new_var_node(new, tok), tok);

    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = head.next;
    return node;
  }

  // Convert `A op= B` to ``tmp = &A, *tmp = *tmp op B`.
  Obj *var = new_lvar("", pointer_to(binary->lhs->ty));

  Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok),
                           new_unary(ND_ADDR, binary->lhs, tok), tok);

  Node *expr2 =
    new_binary(ND_ASSIGN,
               new_unary(ND_DEREF, new_var_node(var, tok), tok),
               new_binary(binary->kind,
                          new_unary(ND_DEREF, new_var_node(var, tok), tok),
                          binary->rhs,
                          tok),
               tok);

  return new_binary(ND_COMMA, expr1, expr2, tok);
}

// assign    = conditional (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^="
//           | "<<=" | ">>="
static Node *assign(Token **rest, Token *tok) {
  Node *node = conditional(&tok, tok);

  if (equal(tok, "=")) {
    add_type(node);
    Member *feature = ppc_read_enum_feature(rest, tok->next, node->ty);
    if (feature)
      return ppc_enum_feature_assignment(node, feature, tok);
    return new_binary(ND_ASSIGN, node, assign(rest, tok->next), tok);
  }

  if (equal(tok, "+="))
    return to_assign(new_add(node, assign(rest, tok->next), tok));

  if (equal(tok, "-="))
    return to_assign(new_sub(node, assign(rest, tok->next), tok));

  if (equal(tok, "*="))
    return to_assign(new_binary(ND_MUL, node, assign(rest, tok->next), tok));

  if (equal(tok, "/="))
    return to_assign(new_binary(ND_DIV, node, assign(rest, tok->next), tok));

  if (equal(tok, "%="))
    return to_assign(new_binary(ND_MOD, node, assign(rest, tok->next), tok));

  if (equal(tok, "&="))
    return to_assign(new_binary(ND_BITAND, node, assign(rest, tok->next), tok));

  if (equal(tok, "|="))
    return to_assign(new_binary(ND_BITOR, node, assign(rest, tok->next), tok));

  if (equal(tok, "^="))
    return to_assign(new_binary(ND_BITXOR, node, assign(rest, tok->next), tok));

  if (equal(tok, "<<="))
    return to_assign(new_binary(ND_SHL, node, assign(rest, tok->next), tok));

  if (equal(tok, ">>="))
    return to_assign(new_binary(ND_SHR, node, assign(rest, tok->next), tok));

  *rest = tok;
  return node;
}

// conditional = logor ("?" expr? ":" conditional)?
static Node *conditional(Token **rest, Token *tok) {
  Node *cond = logor(&tok, tok);

  if (!equal(tok, "?")) {
    *rest = tok;
    return cond;
  }

  if (equal(tok->next, ":")) {
    // [GNU] Compile `a ?: b` as `tmp = a, tmp ? tmp : b`.
    add_type(cond);
    Obj *var = new_lvar("", cond->ty);
    Node *lhs = new_binary(ND_ASSIGN, new_var_node(var, tok), cond, tok);
    Node *rhs = new_node(ND_COND, tok);
    rhs->cond = new_var_node(var, tok);
    rhs->then = new_var_node(var, tok);
    rhs->els = conditional(rest, tok->next->next);
    return new_binary(ND_COMMA, lhs, rhs, tok);
  }

  Node *node = new_node(ND_COND, tok);
  node->cond = cond;
  node->then = expr(&tok, tok->next);
  tok = skip(tok, ":");
  node->els = conditional(rest, tok);
  return node;
}

// logor = logand ("||" logand)*
static Node *logor(Token **rest, Token *tok) {
  Node *node = logand(&tok, tok);
  while (equal(tok, "||")) {
    Token *start = tok;
    node = new_binary(ND_LOGOR, node, logand(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

// logand = bitor ("&&" bitor)*
static Node *logand(Token **rest, Token *tok) {
  Node *node = bitor(&tok, tok);
  while (equal(tok, "&&")) {
    Token *start = tok;
    node = new_binary(ND_LOGAND, node, bitor(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

// bitor = bitxor ("|" bitxor)*
static Node *bitor(Token **rest, Token *tok) {
  Node *node = bitxor(&tok, tok);
  while (equal(tok, "|")) {
    Token *start = tok;
    node = new_binary(ND_BITOR, node, bitxor(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

// bitxor = bitand ("^" bitand)*
static Node *bitxor(Token **rest, Token *tok) {
  Node *node = bitand(&tok, tok);
  while (equal(tok, "^")) {
    Token *start = tok;
    node = new_binary(ND_BITXOR, node, bitand(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

// bitand = equality ("&" equality)*
static Node *bitand(Token **rest, Token *tok) {
  Node *node = equality(&tok, tok);
  while (equal(tok, "&")) {
    Token *start = tok;
    node = new_binary(ND_BITAND, node, equality(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok) {
  Node *node = relational(&tok, tok);

  for (;;) {
    Token *start = tok;

    if (equal(tok, "==")) {
      node = new_binary(ND_EQ, node, relational(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, "!=")) {
      node = new_binary(ND_NE, node, relational(&tok, tok->next), start);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
static Node *relational(Token **rest, Token *tok) {
  Node *node = shift(&tok, tok);

  for (;;) {
    Token *start = tok;

    if (equal(tok, "<")) {
      node = new_binary(ND_LT, node, shift(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, "<=")) {
      node = new_binary(ND_LE, node, shift(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, ">")) {
      node = new_binary(ND_LT, shift(&tok, tok->next), node, start);
      continue;
    }

    if (equal(tok, ">=")) {
      node = new_binary(ND_LE, shift(&tok, tok->next), node, start);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// shift = add ("<<" add | ">>" add)*
static Node *shift(Token **rest, Token *tok) {
  Node *node = add(&tok, tok);

  for (;;) {
    Token *start = tok;

    if (equal(tok, "<<")) {
      node = new_binary(ND_SHL, node, add(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, ">>")) {
      node = new_binary(ND_SHR, node, add(&tok, tok->next), start);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// In C, `+` operator is overloaded to perform the pointer arithmetic.
// If p is a pointer, p+n adds not n but sizeof(*p)*n to the value of p,
// so that p+n points to the location n elements (not bytes) ahead of p.
// In other words, we need to scale an integer value before adding to a
// pointer value. This function takes care of the scaling.
static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (lhs->ty->kind == TY_NULLPTR || rhs->ty->kind == TY_NULLPTR)
    error_tok(tok, "invalid nullptr_t operands");

  // num + num
  if (is_numeric(lhs->ty) && is_numeric(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs, tok);

  if (lhs->ty->base && rhs->ty->base)
    error_tok(tok, "invalid operands");

  // Canonicalize `num + ptr` to `ptr + num`.
  if (!lhs->ty->base && rhs->ty->base) {
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // VLA + num
  if (lhs->ty->base->kind == TY_VLA) {
    rhs = new_binary(ND_MUL, rhs, new_var_node(lhs->ty->base->vla_size, tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
  }

  // ptr + num
  rhs = new_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, tok), tok);
  return new_binary(ND_ADD, lhs, rhs, tok);
}

// Like `+`, `-` is overloaded for the pointer type.
static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (lhs->ty->kind == TY_NULLPTR || rhs->ty->kind == TY_NULLPTR)
    error_tok(tok, "invalid nullptr_t operands");

  // num - num
  if (is_numeric(lhs->ty) && is_numeric(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs, tok);

  // VLA + num
  if (lhs->ty->base->kind == TY_VLA) {
    rhs = new_binary(ND_MUL, rhs, new_var_node(lhs->ty->base->vla_size, tok), tok);
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = lhs->ty;
    return node;
  }

  // ptr - num
  if (lhs->ty->base && is_integer(rhs->ty)) {
    rhs = new_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, tok), tok);
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = lhs->ty;
    return node;
  }

  // ptr - ptr, which returns how many elements are between the two.
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = ty_long;
    return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
  }

  error_tok(tok, "invalid operands");
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok) {
  Node *node = mul(&tok, tok);

  for (;;) {
    Token *start = tok;

    if (equal(tok, "+")) {
      node = new_add(node, mul(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, "-")) {
      node = new_sub(node, mul(&tok, tok->next), start);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// mul = cast ("*" cast | "/" cast | "%" cast)*
static Node *mul(Token **rest, Token *tok) {
  Node *node = cast(&tok, tok);

  for (;;) {
    Token *start = tok;

    if (equal(tok, "*")) {
      node = new_binary(ND_MUL, node, cast(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, "/")) {
      node = new_binary(ND_DIV, node, cast(&tok, tok->next), start);
      continue;
    }

    if (equal(tok, "%")) {
      node = new_binary(ND_MOD, node, cast(&tok, tok->next), start);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// cast = "(" type-name ")" cast | unary
static Node *cast(Token **rest, Token *tok) {
  if (equal(tok, "(") && is_typename(tok->next)) {
    Token *start = tok;
    Type *ty = typename(&tok, tok->next);
    tok = skip(tok, ")");

    // compound literal
    if (equal(tok, "{"))
      return unary(rest, start);

    // type cast
    Node *node = new_cast(cast(rest, tok), ty);
    node->tok = start;
    return node;
  }

  return unary(rest, tok);
}

// unary = ("+" | "-" | "*" | "&" | "!" | "~") cast
//       | ("++" | "--") unary
//       | "&&" ident
//       | postfix
static Node *unary(Token **rest, Token *tok) {
  if (equal(tok, "+")) {
    Node *node = cast(rest, tok->next);
    add_type(node);
    if (node->ty->kind == TY_NULLPTR)
      error_tok(tok, "invalid nullptr_t operand");
    return node;
  }

  if (equal(tok, "-"))
    return new_unary(ND_NEG, cast(rest, tok->next), tok);

  if (equal(tok, "&")) {
    Node *lhs = cast(rest, tok->next);
    add_type(lhs);
    if (lhs->kind == ND_MEMBER && lhs->member->is_bitfield)
      error_tok(tok, "cannot take address of bitfield");
    return new_unary(ND_ADDR, lhs, tok);
  }

  if (equal(tok, "*")) {
    // [https://www.sigbus.info/n1570#6.5.3.2p4] This is an oddity
    // in the C spec, but dereferencing a function shouldn't do
    // anything. If foo is a function, `*foo`, `**foo` or `*****foo`
    // are all equivalent to just `foo`.
    Node *node = cast(rest, tok->next);
    add_type(node);
    if (node->ty->kind == TY_FUNC)
      return node;
    return new_unary(ND_DEREF, node, tok);
  }

  if (equal(tok, "!"))
    return new_unary(ND_NOT, cast(rest, tok->next), tok);

  if (equal(tok, "~"))
    return new_unary(ND_BITNOT, cast(rest, tok->next), tok);

  // Read ++i as i+=1
  if (equal(tok, "++"))
    return to_assign(new_add(unary(rest, tok->next), new_num(1, tok), tok));

  // Read --i as i-=1
  if (equal(tok, "--"))
    return to_assign(new_sub(unary(rest, tok->next), new_num(1, tok), tok));

  // [GNU] labels-as-values
  if (equal(tok, "&&")) {
    Node *node = new_node(ND_LABEL_VAL, tok);
    node->label = get_ident(tok->next);
    node->goto_next = gotos;
    gotos = node;
    *rest = tok->next->next;
    return node;
  }

  return postfix(rest, tok);
}

// struct-members = (declspec declarator (","  declarator)* ";")*
static void struct_members(Token **rest, Token *tok, Type *ty) {
  Member head = {};
  Member *cur = &head;
  int idx = 0;

  while (!equal(tok, "}")) {
    VarAttr attr = {};
    Type *basety = declspec(&tok, tok, &attr);
    bool first = true;

    // Anonymous struct member
    if ((basety->kind == TY_STRUCT || basety->kind == TY_UNION) &&
        consume(&tok, tok, ";")) {
      Member *mem = calloc(1, sizeof(Member));
      mem->ty = basety;
      mem->tok = tok;
      mem->idx = idx++;
      mem->align = attr.align ? attr.align : mem->ty->align;
      cur = cur->next = mem;
      continue;
    }

    // Regular struct members
    while (!consume(&tok, tok, ";")) {
      if (!first)
        tok = skip(tok, ",");
      first = false;

      Member *mem = calloc(1, sizeof(Member));
      mem->tok = tok;
      mem->ty = declarator(&tok, tok, basety);
      mem->name = mem->ty->name;
      mem->idx = idx++;
      mem->align = attr.align ? attr.align : mem->ty->align;

      if (consume(&tok, tok, ":")) {
        mem->is_bitfield = true;
        mem->bit_width = const_expr(&tok, tok);
      }

      cur = cur->next = mem;
    }
  }

  // If the last element is an array of incomplete type, it's
  // called a "flexible array member". It should behave as if
  // if were a zero-sized array.
  if (cur != &head && cur->ty->kind == TY_ARRAY && cur->ty->array_len < 0) {
    cur->ty = array_of(cur->ty->base, 0);
    ty->is_flexible = true;
  }

  *rest = tok->next;
  ty->members = head.next;
}

static void ppc_spec_members(Token **rest, Token *tok, Type *ty, bool pointers_only) {
  Member head = {};
  Member *cur = &head;
  int idx = 0;

  while (!equal(tok, "}")) {
    VarAttr attr = {};
    Type *basety = declspec(&tok, tok, &attr);
    bool first = true;

    if (pointers_only && !equal(tok, "*"))
      error_tok(tok, "PPC nullptr specialization requires a pointer declarator");

    while (!consume(&tok, tok, ";")) {
      if (!first)
        tok = skip(tok, ",");
      first = false;

      if (pointers_only && !equal(tok, "*"))
        error_tok(tok, "PPC nullptr specialization requires a pointer declarator");
      Member *mem = calloc(1, sizeof(Member));
      mem->tok = tok;
      mem->ty = declarator(&tok, tok, basety);
      mem->name = mem->ty->name;
      mem->idx = idx++;
      mem->align = attr.align ? attr.align : mem->ty->align;

      if (consume(&tok, tok, ":")) {
        mem->is_bitfield = true;
        mem->bit_width = const_expr(&tok, tok);
      }

      if (pointers_only && (mem->ty->kind != TY_PTR || mem->is_bitfield ||
                            mem->align != mem->ty->align))
        error_tok(mem->tok, "PPC nullptr specialization must be an ordinary pointer");

      cur = cur->next = mem;
    }
  }

  *rest = tok->next;
  ty->members = head.next;
}

static Member *ppc_default_spec_member(Token **rest, Token *tok) {
  tok = skip(tok, "default");

  VarAttr attr = {};
  Type *basety = declspec(&tok, tok, &attr);

  Member *mem = calloc(1, sizeof(Member));
  mem->tok = tok;
  mem->ty = declarator(&tok, tok, basety);
  mem->name = mem->ty->name;
  if (!mem->name)
    error_tok(mem->tok, "PPC default specialization name omitted");
  mem->align = attr.align ? attr.align : mem->ty->align;

  if (consume(&tok, tok, ":")) {
    mem->is_bitfield = true;
    mem->bit_width = const_expr(&tok, tok);
  }

  *rest = skip(tok, ";");
  return mem;
}

static Member *ppc_enum_members(Token **rest, Token *tok) {
  Member head = {};
  Member *cur = &head;
  int idx = 0;

  while (!consume_end(rest, tok)) {
    if (idx > 0)
      tok = skip(tok, ",");

    get_ident(tok);

    Member *mem = calloc(1, sizeof(Member));
    mem->tok = tok;
    mem->ty = ty_void;
    mem->name = tok;
    mem->idx = idx++;
    mem->align = ty_void->align;

    cur = cur->next = mem;
    tok = tok->next;
  }

  return head.next;
}

static bool ppc_specs_are_void(Member *specs) {
  for (Member *mem = specs; mem; mem = mem->next)
    if (mem->ty->kind != TY_VOID)
      return false;
  return true;
}

static void ppc_mark_enum_generalization(Type *base, Token *tok) {
  if (base->is_ppc_pointer_generalization)
    error_tok(tok, "PPC nullptr generalization cannot use enum specializations");
  if (base->ppc_default_spec)
    error_tok(tok, "PPC enum generalization cannot have a default specialization");
  if (!ppc_specs_are_void(base->ppc_specs))
    error_tok(tok, "PPC enum generalization can only contain void specializations");
  base->is_ppc_enum_generalization = true;
}

static bool is_ppc_enum_extension_list(Token *tok) {
  return tok->kind == TK_IDENT && (equal(tok->next, ",") || equal(tok->next, "}"));
}

static bool tok_eq(Token *a, Token *b) {
  return a && b && a->len == b->len && !strncmp(a->loc, b->loc, a->len);
}

static Member *next_user_member(Member *mem) {
  while (mem && mem->is_ppc_tag)
    mem = mem->next;
  return mem;
}

static Member *ppc_tag_member(Type *ty) {
  for (Member *mem = ty->members; mem; mem = mem->next)
    if (mem->is_ppc_tag)
      return mem;
  return NULL;
}

static Obj *new_ppc_common_var(char *name, Type *ty) {
  Obj *var = new_gvar(name, ty);
  var->is_static = false;
  var->is_definition = true;
  var->is_tentative = true;
  var->init_data = NULL;
  return var;
}

static void remember_ppc_gen(Type *ty, Token *tok) {
  for (PpcGen *gen = ppc_gens; gen; gen = gen->next)
    if (gen->ty == ty) {
      if (!gen->tok)
        gen->tok = tok;
      return;
    }

  PpcGen *gen = calloc(1, sizeof(PpcGen));
  gen->ty = ty;
  gen->tok = tok;
  gen->next = ppc_gens;
  ppc_gens = gen;
}

static char *ppc_type_name(Type *ty) {
  if (ty->ppc_name)
    return ty->ppc_name;
  ty->ppc_name = new_unique_name();
  return ty->ppc_name;
}

static char *ppc_named_coord(char *prefix, Token *name) {
  return format("%s_%.*s", prefix, name->len, name->loc);
}

static void ensure_ppc_count_var(Type *ty) {
  if (!ty->ppc_count_var)
    ty->ppc_count_var =
      new_ppc_common_var(format("__ppc_%s_count", ppc_type_name(ty)), ty_int);
}

static void ensure_ppc_spec_ptr_tags_var(Type *ty) {
  ensure_ppc_count_var(ty);
  if (!ty->ppc_spec_ptr_tags_var)
    ty->ppc_spec_ptr_tags_var =
      new_ppc_common_var(format("__ppc_%s_spec_ptr_tags", ppc_type_name(ty)),
                         pointer_to(ty_int));
}

static void renumber_members(Type *ty) {
  int idx = 0;
  for (Member *mem = ty->members; mem; mem = mem->next)
    mem->idx = idx++;
}

static void ensure_ppc_tag_member(Type *ty) {
  if (ppc_tag_member(ty))
    return;

  Member *mem = calloc(1, sizeof(Member));
  mem->ty = ty_int;
  mem->align = ty_int->align;
  mem->is_ppc_tag = true;
  mem->ppc_index = 0;
  mem->next = ty->members;
  ty->members = mem;
  renumber_members(ty);
}

static void layout_struct_members(Type *ty) {
  int bits = 0;

  for (Member *mem = ty->members; mem; mem = mem->next) {
    if (mem->is_bitfield && mem->bit_width == 0) {
      // Zero-width anonymous bitfield has a special meaning.
      // It affects only alignment.
      bits = align_to(bits, mem->ty->size * 8);
    } else if (mem->is_bitfield) {
      int sz = mem->ty->size;
      if (bits / (sz * 8) != (bits + mem->bit_width - 1) / (sz * 8))
        bits = align_to(bits, sz * 8);

      mem->offset = align_down(bits / 8, sz);
      mem->bit_offset = bits % (sz * 8);
      bits += mem->bit_width;
    } else {
      if (!ty->is_packed)
        bits = align_to(bits, mem->align * 8);
      mem->offset = bits / 8;
      bits += mem->ty->size * 8;
    }

    if (!ty->is_packed && ty->align < mem->align)
      ty->align = mem->align;
  }

  ty->size = align_to(bits, ty->align * 8) / 8;
}

static Member *copy_member(Member *src) {
  Member *mem = calloc(1, sizeof(Member));
  *mem = *src;
  mem->next = NULL;
  return mem;
}

static Member **member_tail(Member **mem) {
  while (*mem)
    mem = &(*mem)->next;
  return mem;
}

static int ppc_next_spec_index(Type *base) {
  int idx = 1;
  for (Member *mem = base->ppc_specs; mem; mem = mem->next)
    if (idx <= mem->idx)
      idx = mem->idx + 1;
  return idx;
}

static Member *find_ppc_spec(Type *base, Token *name) {
  for (Member *mem = base->ppc_specs; mem; mem = mem->next)
    if (tok_eq(mem->name, name))
      return mem;
  return NULL;
}

static Type *try_ppc_direct_specialization_type(Type *base, Token *name);
static Type *try_ppc_nested_specialization_type(Type *outer, Token *name);
static Type *try_ppc_member_specialization_type(Type *ty, Token *name);

static Type *ppc_void_specialization_type(Type *base, Token *name) {
  if (base->ppc_default_spec)
    error_tok(name, "PPC zero specialization is named; use the default specialization name");

  if (base->ppc_void_ty)
    return base->ppc_void_ty;

  Type *ty = struct_type();
  if (base->is_ppc_pointer_generalization) {
    ty->is_packed = base->is_packed;
    ty->align = base->align;
  }
  ty->ppc_base = base;
  ty->ppc_spec_name = name;
  ty->ppc_name = ppc_named_coord(ppc_type_name(base), name);

  Member head = {};
  Member *cur = &head;
  int idx = 0;

  for (Member *mem = base->members; mem; mem = mem->next) {
    cur = cur->next = copy_member(mem);
    if (cur->is_ppc_tag) {
      cur->ppc_index = 0;
      cur->ppc_tag_var = NULL;
    }
    cur->idx = idx++;
  }
  ty->members = head.next;

  layout_struct_members(ty);
  base->ppc_void_ty = ty;
  return ty;
}

static void append_ppc_default_spec(Type *base, Member *spec) {
  if (!spec)
    return;
  if (base->is_ppc_generalization || base->ppc_default_spec || base->ppc_specs)
    error_tok(spec->tok, "PPC default specialization must be the first case item");
  if (base->is_ppc_enum_generalization)
    error_tok(spec->tok, "PPC enum generalization cannot have a default specialization");
  if (get_struct_member(base, spec->name))
    error_tok(spec->name, "duplicate PPC default specialization name");

  base->is_ppc_generalization = true;
  ensure_ppc_tag_member(base);
  ensure_ppc_count_var(base);
  remember_ppc_gen(base, spec->tok);

  spec->idx = 0;
  spec->ppc_ty = NULL;
  spec->ppc_tag_var = NULL;
  spec->next = NULL;

  base->ppc_default_spec = spec;
  base->ppc_specs = spec;

  if (spec->ty->kind != TY_VOID) {
    Member *mem = copy_member(spec);
    mem->is_ppc_default_spec_member = true;
    *member_tail(&base->members) = mem;
    renumber_members(base);
  }
}

static void append_ppc_specs(Type *base, Member *specs) {
  if (specs && base->is_ppc_const_generalization)
    error_tok(specs->tok, "cannot extend const PPC generalization");
  base->is_ppc_generalization = true;
  ensure_ppc_tag_member(base);
  ensure_ppc_count_var(base);
  remember_ppc_gen(base, specs ? specs->tok : NULL);

  if (base->is_ppc_enum_generalization && !ppc_specs_are_void(specs))
    error_tok(specs->tok, "PPC enum generalization can only contain void specializations");

  Member **tail = &base->ppc_specs;
  int idx = ppc_next_spec_index(base);
  while (*tail)
    tail = &(*tail)->next;

  for (Member *mem = specs; mem; mem = mem->next) {
    if (!mem->name)
      error_tok(mem->tok, "PPC specialization name omitted");

    if (base->is_ppc_pointer_generalization &&
        (mem->ty->kind != TY_PTR || mem->is_bitfield ||
         mem->align != mem->ty->align))
      error_tok(mem->tok, "PPC nullptr generalization can only contain pointer specializations");

    if (get_struct_member(base, mem->name) || find_ppc_spec(base, mem->name))
      error_tok(mem->name, "duplicate PPC specialization name");

    mem->idx = idx++;
    mem->ppc_ty = NULL;
    mem->ppc_tag_var =
      new_ppc_common_var(format("__ppc_%s_%.*s_tag", ppc_type_name(base),
                                mem->name->len, mem->name->loc),
                         ty_int);
  }

  *tail = specs;
}

static void ppc_mark_pointer_generalization(Type *base, Token *tok) {
  if (base->is_ppc_generalization)
    error_tok(tok, "PPC nullptr generalization already declared; use + to extend it");
  base->is_ppc_pointer_generalization = true;
  ensure_ppc_tag_member(base);

  // Reserve the same payload slot in the base/zero type as in every
  // pointer specialization. `void` also remains its specialization name.
  Token *name = calloc(1, sizeof(Token));
  *name = *tok;
  name->kind = TK_KEYWORD;
  name->loc = "void";
  name->len = 4;

  Member *mem = calloc(1, sizeof(Member));
  mem->tok = tok;
  mem->name = name;
  mem->ty = pointer_to(ty_void);
  mem->align = mem->ty->align;
  mem->is_ppc_pointer_zero_member = true;
  *member_tail(&base->members) = mem;
  renumber_members(base);
}

static Type *ppc_direct_specialization_type(Type *base, Token *name) {
  if (!base->is_ppc_generalization)
    return NULL;

  if (equal(name, "void"))
    return ppc_void_specialization_type(base, name);

  Member *spec = find_ppc_spec(base, name);
  if (!spec)
    return NULL;

  if (spec->ppc_ty)
    return spec->ppc_ty;

  bool is_default_spec = spec == base->ppc_default_spec;

  Type *ty = struct_type();
  if (base->is_ppc_pointer_generalization) {
    ty->is_packed = base->is_packed;
    ty->align = base->align;
  }
  ty->ppc_base = base;
  ty->ppc_spec_name = spec->name;
  ty->ppc_name = ppc_named_coord(ppc_type_name(base), spec->name);

  Member head = {};
  Member *cur = &head;
  int idx = 0;

  for (Member *mem = base->members; mem; mem = mem->next) {
    if (mem->is_ppc_pointer_zero_member)
      continue;
    if (mem->is_ppc_default_spec_member && !is_default_spec)
      continue;

    cur = cur->next = copy_member(mem);
    if (cur->is_ppc_tag) {
      cur->ppc_index = spec->idx;
      cur->ppc_tag_var = spec->ppc_tag_var;
    }
    cur->idx = idx++;
  }

  if (spec->ty->kind != TY_VOID && !is_default_spec) {
    cur = cur->next = copy_member(spec);
    cur->idx = idx++;
  }
  ty->members = head.next;

  layout_struct_members(ty);
  spec->ppc_ty = ty;
  return ty;
}

static Type *try_ppc_direct_specialization_type(Type *base, Token *name) {
  return ppc_direct_specialization_type(base, name);
}

static Type *find_ppc_nested_type(Type *outer, Token *name) {
  for (PpcNestedType *ent = ppc_nested_types; ent; ent = ent->next)
    if (ent->outer == outer && tok_eq(ent->name, name))
      return ent->ty;
  return NULL;
}

static void remember_ppc_nested_type(Type *outer, Token *name, Type *ty) {
  PpcNestedType *ent = calloc(1, sizeof(PpcNestedType));
  ent->outer = outer;
  ent->name = name;
  ent->ty = ty;
  ent->next = ppc_nested_types;
  ppc_nested_types = ent;
}

static Type *try_ppc_nested_specialization_type(Type *outer, Token *name) {
  if (!outer->ppc_base)
    return NULL;

  Type *cached = find_ppc_nested_type(outer, name);
  if (cached)
    return cached;

  Member *matched_mem = NULL;
  Type *matched_ty = NULL;

  for (Member *mem = outer->members; mem; mem = mem->next) {
    if (mem->is_ppc_tag)
      continue;

    Type *ty = try_ppc_member_specialization_type(mem->ty, name);
    if (!ty)
      continue;

    if (matched_mem)
      error_tok(name, "ambiguous nested PPC specialization");
    matched_mem = mem;
    matched_ty = ty;
  }

  if (!matched_mem)
    return NULL;

  Type *ty = struct_type();
  ty->ppc_base = outer->ppc_base;
  ty->ppc_spec_name = name;
  ty->ppc_name = ppc_named_coord(ppc_type_name(outer), name);

  Member head = {};
  Member *cur = &head;
  int idx = 0;

  for (Member *mem = outer->members; mem; mem = mem->next) {
    cur = cur->next = copy_member(mem);
    if (mem == matched_mem)
      cur->ty = matched_ty;
    cur->idx = idx++;
  }
  ty->members = head.next;

  layout_struct_members(ty);
  remember_ppc_nested_type(outer, name, ty);
  return ty;
}

static Type *try_ppc_member_specialization_type(Type *ty, Token *name) {
  Type *direct = try_ppc_direct_specialization_type(ty, name);
  if (direct)
    return direct;

  if (ty->ppc_base)
    return try_ppc_nested_specialization_type(ty, name);

  if (!ty->is_ppc_generalization)
    return NULL;

  Type *matched = NULL;
  for (Member *spec = ty->ppc_specs; spec; spec = spec->next) {
    if (spec->ty->kind == TY_VOID)
      continue;

    Type *inner = try_ppc_member_specialization_type(spec->ty, name);
    if (!inner)
      continue;

    if (matched)
      error_tok(name, "ambiguous nested PPC specialization");

    Type *outer = try_ppc_direct_specialization_type(ty, spec->name);
    matched = try_ppc_nested_specialization_type(outer, name);
  }

  return matched;
}

static Type *ppc_specialization_type(Type *base, Token *name) {
  Type *ty = try_ppc_direct_specialization_type(base, name);
  if (ty)
    return ty;

  ty = try_ppc_nested_specialization_type(base, name);
  if (ty)
    return ty;

  if (!base->is_ppc_generalization && !base->ppc_base)
    error_tok(name, "not a PPC generalization or specialization");

  error_tok(name, "unknown PPC specialization");
}

static Type *ppc_type_suffix(Token **rest, Type *ty, Token *tok) {
  while (equal(tok, ".")) {
    ty = ppc_specialization_type(ty, tok->next);
    tok = tok->next->next;
  }

  *rest = tok;
  return ty;
}

static Token *ppc_extend_generalization(Token *tok, Type *base) {
  if (!base->is_ppc_generalization)
    error_tok(tok, "not a PPC generalization");
  if (base->is_ppc_const_generalization)
    error_tok(tok, "cannot extend const PPC generalization");

  tok = skip(tok, "+");
  tok = skip(tok, "{");

  if (base->is_ppc_pointer_generalization) {
    Type tmp = {};
    ppc_spec_members(&tok, tok, &tmp, true);
    append_ppc_specs(base, tmp.members);
  } else if (base->is_ppc_enum_generalization || is_ppc_enum_extension_list(tok)) {
    if (!base->is_ppc_enum_generalization)
      ppc_mark_enum_generalization(base, tok);
    append_ppc_specs(base, ppc_enum_members(&tok, tok));
  } else {
    Type tmp = {};
    ppc_spec_members(&tok, tok, &tmp, false);
    append_ppc_specs(base, tmp.members);
  }
  return tok;
}

// attribute = ("__attribute__" "(" "(" "packed" ")" ")")*
static Token *attribute_list(Token *tok, Type *ty) {
  while (consume(&tok, tok, "__attribute__")) {
    tok = skip(tok, "(");
    tok = skip(tok, "(");

    bool first = true;

    while (!consume(&tok, tok, ")")) {
      if (!first)
        tok = skip(tok, ",");
      first = false;

      if (consume(&tok, tok, "packed")) {
        ty->is_packed = true;
        continue;
      }

      if (consume(&tok, tok, "aligned")) {
        tok = skip(tok, "(");
        ty->align = const_expr(&tok, tok);
        tok = skip(tok, ")");
        continue;
      }

      error_tok(tok, "unknown attribute");
    }

    tok = skip(tok, ")");
  }

  return tok;
}

// struct-union-decl = attribute? ident? ("{" struct-members)?
static Type *struct_union_decl(Token **rest, Token *tok) {
  Type *ty = struct_type();
  tok = attribute_list(tok, ty);

  // Read a tag.
  Token *tag = NULL;
  if (tok->kind == TK_IDENT) {
    tag = tok;
    tok = tok->next;
  }

  if (tag && !equal(tok, "{")) {
    Type *ty2 = find_tag(tag);
    if (ty2) {
      ty2 = ppc_type_suffix(&tok, ty2, tok);
      *rest = tok;
      return ty2;
    }

    if (equal(tok, "."))
      error_tok(tag, "unknown PPC generalization");

    ty->size = -1;
    ty->ppc_name = get_ident(tag);
    push_tag_scope(tag, ty);
    *rest = tok;
    return ty;
  }

  tok = skip(tok, "{");

  // Construct a struct object.
  struct_members(&tok, tok, ty);
  *rest = attribute_list(tok, ty);

  if (tag) {
    ty->ppc_name = get_ident(tag);

    // If this is a redefinition, overwrite a previous type.
    // Otherwise, register the struct type.
    Type *ty2 = hashmap_get2(&scope->tags, tag->loc, tag->len);
    if (ty2) {
      *ty2 = *ty;
      return ty2;
    }

    push_tag_scope(tag, ty);
  }

  return ty;
}

// struct-decl = struct-union-decl
static Type *struct_decl(Token **rest, Token *tok) {
  Type *ty = struct_union_decl(&tok, tok);
  ty->kind = TY_STRUCT;

  if (equal(tok, "case")) {
    tok = skip(tok->next, "{");

    Member *default_spec = NULL;
    if (equal(tok, "default"))
      default_spec = ppc_default_spec_member(&tok, tok);

    Type tmp = {};
    ppc_spec_members(&tok, tok, &tmp, false);
    append_ppc_default_spec(ty, default_spec);
    append_ppc_specs(ty, tmp.members);
  } else if (equal(tok, "enum")) {
    tok = skip(tok->next, "{");
    ppc_mark_enum_generalization(ty, tok);
    append_ppc_specs(ty, ppc_enum_members(&tok, tok));
  } else if (equal(tok, "nullptr")) {
    Token *start = tok;
    tok = skip(tok->next, "{");
    ppc_mark_pointer_generalization(ty, start);
    Type tmp = {};
    ppc_spec_members(&tok, tok, &tmp, true);
    append_ppc_specs(ty, tmp.members);
  }

  if (ty->is_ppc_generalization) {
    if (consume(&tok, tok, "const"))
      ty->is_ppc_const_generalization = true;

    if (equal(tok, "="))
      error_tok(tok, "PPC = 0 zero-specialization syntax was replaced by default");
  }

  *rest = tok;

  if (ty->size < 0)
    return ty;

  // Assign offsets within the struct to members.
  layout_struct_members(ty);
  return ty;
}

// union-decl = struct-union-decl
static Type *union_decl(Token **rest, Token *tok) {
  Type *ty = struct_union_decl(rest, tok);
  ty->kind = TY_UNION;

  if (ty->size < 0)
    return ty;

  // If union, we don't have to assign offsets because they
  // are already initialized to zero. We need to compute the
  // alignment and the size though.
  for (Member *mem = ty->members; mem; mem = mem->next) {
    if (ty->align < mem->align)
      ty->align = mem->align;
    if (ty->size < mem->ty->size)
      ty->size = mem->ty->size;
  }
  ty->size = align_to(ty->size, ty->align);
  return ty;
}

// Find a struct member by name.
static Member *get_struct_member(Type *ty, Token *tok) {
  for (Member *mem = ty->members; mem; mem = mem->next) {
    if (mem->is_ppc_tag)
      continue;

    // Anonymous struct member
    if ((mem->ty->kind == TY_STRUCT || mem->ty->kind == TY_UNION) &&
        !mem->name) {
      if (get_struct_member(mem->ty, tok))
        return mem;
      continue;
    }

    // Regular struct member
    if (mem->name && mem->name->len == tok->len &&
        !strncmp(mem->name->loc, tok->loc, tok->len))
      return mem;
  }
  return NULL;
}

// Create a node representing a struct member access, such as foo.bar
// where foo is a struct and bar is a member name.
//
// C has a feature called "anonymous struct" which allows a struct to
// have another unnamed struct as a member like this:
//
//   struct { struct { int a; }; int b; } x;
//
// The members of an anonymous struct belong to the outer struct's
// member namespace. Therefore, in the above example, you can access
// member "a" of the anonymous struct as "x.a".
//
// This function takes care of anonymous structs.
static Node *struct_ref(Node *node, Token *tok) {
  add_type(node);
  if (node->ty->kind != TY_STRUCT && node->ty->kind != TY_UNION)
    error_tok(node->tok, "not a struct nor a union");

  Type *ty = node->ty;

  for (;;) {
    Member *mem = get_struct_member(ty, tok);
    if (!mem)
      error_tok(tok, "no such member");
    node = new_unary(ND_MEMBER, node, tok);
    node->member = mem;
    if (mem->name)
      break;
    ty = mem->ty;
  }
  return node;
}

// Convert A++ to `(typeof A)((A += 1) - 1)`
static Node *new_inc_dec(Node *node, Token *tok, int addend) {
  add_type(node);
  return new_cast(new_add(to_assign(new_add(node, new_num(addend, tok), tok)),
                          new_num(-addend, tok), tok),
                  node->ty);
}

// postfix = "(" type-name ")" "{" initializer-list "}"
//         = ident "(" func-args ")" postfix-tail*
//         | primary postfix-tail*
//
// postfix-tail = "[" expr "]"
//              | "(" func-args ")"
//              | "." ident
//              | "->" ident
//              | "++"
//              | "--"
static Node *postfix(Token **rest, Token *tok) {
  if (equal(tok, "(") && is_typename(tok->next)) {
    // Compound literal
    Token *start = tok;
    Type *ty = typename(&tok, tok->next);
    tok = skip(tok, ")");

    if (scope->next == NULL) {
      Obj *var = new_anon_gvar(ty);
      Initializer *init = gvar_initializer(rest, tok, var);
      ppc_gvar_tag_initializer(var, init, tok);
      return new_var_node(var, start);
    }

    Obj *var = new_lvar("", ty);
    Node *lhs = lvar_initializer(rest, tok, var);
    Node *rhs = new_var_node(var, tok);
    return new_binary(ND_COMMA, lhs, rhs, start);
  }

  Node *node = primary(&tok, tok);

  for (;;) {
    if (equal(tok, "(")) {
      node = funcall(&tok, tok->next, node);
      continue;
    }

    if (equal(tok, "[")) {
      // x[y] is short for *(x+y)
      Token *start = tok;
      Node *idx = expr(&tok, tok->next);
      tok = skip(tok, "]");
      node = new_unary(ND_DEREF, new_add(node, idx, start), start);
      continue;
    }

    if (equal(tok, ".")) {
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }

    if (equal(tok, "->")) {
      // x->y is short for (*x).y
      node = new_unary(ND_DEREF, node, tok);
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }

    if (equal(tok, "++")) {
      node = new_inc_dec(node, tok, 1);
      tok = tok->next;
      continue;
    }

    if (equal(tok, "--")) {
      node = new_inc_dec(node, tok, -1);
      tok = tok->next;
      continue;
    }

    *rest = tok;
    return node;
  }
}

// funcall = (assign ("," assign)*)? ")"
static Node *funcall(Token **rest, Token *tok, Node *fn) {
  add_type(fn);

  if (fn->ty->kind != TY_FUNC &&
      (fn->ty->kind != TY_PTR || fn->ty->base->kind != TY_FUNC))
    error_tok(fn->tok, "not a function");

  Type *ty = (fn->ty->kind == TY_FUNC) ? fn->ty : fn->ty->base;
  Type *param_ty = ty->params;

  Node head = {};
  Node *cur = &head;

  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");

    Node *arg = assign(&tok, tok);
    add_type(arg);

    if (!param_ty && !ty->is_variadic)
      error_tok(tok, "too many arguments");

    if (param_ty) {
      if (param_ty->kind == TY_STRUCT || param_ty->kind == TY_UNION) {
        if (!is_compatible(param_ty, arg->ty))
          error_tok(arg->tok, "incompatible argument type");
      } else {
        arg = new_cast(arg, param_ty);
      }
      param_ty = param_ty->next;
    } else if (arg->ty->kind == TY_FLOAT) {
      // If parameter type is omitted (e.g. in "..."), float
      // arguments are promoted to double.
      arg = new_cast(arg, ty_double);
    }

    cur = cur->next = arg;
  }

  if (param_ty)
    error_tok(tok, "too few arguments");

  *rest = skip(tok, ")");

  Node *node = new_unary(ND_FUNCALL, fn, tok);
  node->func_ty = ty;
  node->ty = ty->return_ty;
  node->args = head.next;

  // If a function returns a struct, it is caller's responsibility
  // to allocate a space for the return value.
  if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)
    node->ret_buffer = new_lvar("", node->ty);
  return node;
}

// generic-selection = "(" assign "," generic-assoc ("," generic-assoc)* ")"
//
// generic-assoc = type-name ":" assign
//               | "default" ":" assign
static Node *generic_selection(Token **rest, Token *tok) {
  Token *start = tok;
  tok = skip(tok, "(");

  Node *ctrl = assign(&tok, tok);
  add_type(ctrl);

  Type *t1 = ctrl->ty;
  if (t1->kind == TY_FUNC)
    t1 = pointer_to(t1);
  else if (t1->kind == TY_ARRAY)
    t1 = pointer_to(t1->base);

  Node *ret = NULL;

  while (!consume(rest, tok, ")")) {
    tok = skip(tok, ",");

    if (equal(tok, "default")) {
      tok = skip(tok->next, ":");
      Node *node = assign(&tok, tok);
      if (!ret)
        ret = node;
      continue;
    }

    Type *t2 = typename(&tok, tok);
    tok = skip(tok, ":");
    Node *node = assign(&tok, tok);
    if (is_compatible(t1, t2))
      ret = node;
  }

  if (!ret)
    error_tok(start, "controlling expression type not compatible with"
              " any generic association type");
  return ret;
}

// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")"
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | "_Alignof" "(" type-name ")"
//         | "_Alignof" unary
//         | "_Generic" generic-selection
//         | "__builtin_types_compatible_p" "(" type-name, type-name, ")"
//         | "__builtin_reg_class" "(" type-name ")"
//         | ident
//         | str
//         | num
static Node *primary(Token **rest, Token *tok) {
  Token *start = tok;

  if (equal(tok, "nullptr")) {
    Node *node = new_num(0, tok);
    node->ty = ty_nullptr;
    *rest = tok->next;
    return node;
  }

  if (equal(tok, "(") && equal(tok->next, "{")) {
    // This is a GNU statement expresssion.
    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = compound_stmt(&tok, tok->next->next)->body;
    *rest = skip(tok, ")");
    return node;
  }

  if (equal(tok, "(")) {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }

  if (equal(tok, "sizeof") && equal(tok->next, "(") && is_typename(tok->next->next)) {
    Type *ty = typename(&tok, tok->next->next);
    *rest = skip(tok, ")");

    if (ty->kind == TY_VLA) {
      if (ty->vla_size)
        return new_var_node(ty->vla_size, tok);

      Node *lhs = compute_vla_size(ty, tok);
      Node *rhs = new_var_node(ty->vla_size, tok);
      return new_binary(ND_COMMA, lhs, rhs, tok);
    }

    return new_ulong(ty->size, start);
  }

  if (equal(tok, "sizeof")) {
    Node *node = unary(rest, tok->next);
    add_type(node);
    if (node->ty->kind == TY_VLA)
      return new_var_node(node->ty->vla_size, tok);
    return new_ulong(node->ty->size, tok);
  }

  if (equal(tok, "_Alignof") && equal(tok->next, "(") && is_typename(tok->next->next)) {
    Type *ty = typename(&tok, tok->next->next);
    *rest = skip(tok, ")");
    return new_ulong(ty->align, tok);
  }

  if (equal(tok, "_Alignof")) {
    Node *node = unary(rest, tok->next);
    add_type(node);
    return new_ulong(node->ty->align, tok);
  }

  if (equal(tok, "_Generic"))
    return generic_selection(rest, tok->next);

  if (equal(tok, "__builtin_types_compatible_p")) {
    tok = skip(tok->next, "(");
    Type *t1 = typename(&tok, tok);
    tok = skip(tok, ",");
    Type *t2 = typename(&tok, tok);
    *rest = skip(tok, ")");
    return new_num(is_compatible(t1, t2), start);
  }

  if (equal(tok, "__builtin_reg_class")) {
    tok = skip(tok->next, "(");
    Type *ty = typename(&tok, tok);
    *rest = skip(tok, ")");

    if (is_integer(ty) || ty->kind == TY_PTR || ty->kind == TY_NULLPTR)
      return new_num(0, start);
    if (is_flonum(ty))
      return new_num(1, start);
    return new_num(2, start);
  }

  if (equal(tok, "__builtin_compare_and_swap")) {
    Node *node = new_node(ND_CAS, tok);
    tok = skip(tok->next, "(");
    node->cas_addr = assign(&tok, tok);
    tok = skip(tok, ",");
    node->cas_old = assign(&tok, tok);
    tok = skip(tok, ",");
    node->cas_new = assign(&tok, tok);
    *rest = skip(tok, ")");
    return node;
  }

  if (equal(tok, "__builtin_atomic_exchange")) {
    Node *node = new_node(ND_EXCH, tok);
    tok = skip(tok->next, "(");
    node->lhs = assign(&tok, tok);
    tok = skip(tok, ",");
    node->rhs = assign(&tok, tok);
    *rest = skip(tok, ")");
    return node;
  }

  if (equal(tok, "create_spec")) {
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

  if (equal(tok, "get_spec_size")) {
    tok = skip(tok->next, "(");
    Type *ty = typename(&tok, tok);
    *rest = skip(tok, ")");
    return ppc_get_spec_size_expr(start, ty);
  }

  if (equal(tok, "get_spec_ptr")) {
    tok = skip(tok->next, "(");
    Type *ty = typename(&tok, tok);
    tok = skip(tok, ",");
    Node *idx = assign(&tok, tok);
    *rest = skip(tok, ")");
    return ppc_get_spec_ptr_expr(start, ty, idx);
  }

  if (equal(tok, "spec_index_cmp")) {
    tok = skip(tok->next, "(");
    Node *lhs = assign(&tok, tok);
    tok = skip(tok, ",");
    Node *rhs = assign(&tok, tok);
    *rest = skip(tok, ")");
    return ppc_spec_index_cmp_expr(start, lhs, rhs);
  }

  if (tok->kind == TK_IDENT && equal(tok->next, "<") && find_ppc_func(tok))
    return ppc_funcall_expr(rest, tok);

  if (tok->kind == TK_IDENT) {
    // Variable or enum constant
    VarScope *sc = find_var(tok);
    *rest = tok->next;

    // For "static inline" function
    if (sc && sc->var && sc->var->is_function) {
      if (current_fn)
        strarray_push(&current_fn->refs, sc->var->name);
      else
        sc->var->is_root = true;
    }

    if (sc) {
      if (sc->var)
        return new_var_node(sc->var, tok);
      if (sc->enum_ty)
        return new_num(sc->enum_val, tok);
    }

    if (equal(tok->next, "("))
      error_tok(tok, "implicit declaration of a function");
    error_tok(tok, "undefined variable");
  }

  if (tok->kind == TK_STR) {
    Obj *var = new_string_literal(tok->str, tok->ty);
    *rest = tok->next;
    return new_var_node(var, tok);
  }

  if (tok->kind == TK_NUM) {
    Node *node;
    if (is_flonum(tok->ty)) {
      node = new_node(ND_NUM, tok);
      node->fval = tok->fval;
    } else {
      node = new_num(tok->val, tok);
    }

    node->ty = tok->ty;
    *rest = tok->next;
    return node;
  }

  error_tok(tok, "expected an expression");
}

static Token *parse_typedef(Token *tok, Type *basety) {
  bool first = true;

  while (!consume(&tok, tok, ";")) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    Type *ty = declarator(&tok, tok, basety);
    if (!ty->name)
      error_tok(ty->name_pos, "typedef name omitted");
    if (ty->is_ppc_generalization && !ty->ppc_name)
      ty->ppc_name = get_ident(ty->name);
    push_scope(get_ident(ty->name))->type_def = ty;
  }
  return tok;
}

static void create_param_lvars(Type *param) {
  if (param) {
    create_param_lvars(param->next);
    if (!param->name)
      error_tok(param->name_pos, "parameter name omitted");
    new_lvar(get_ident(param->name), param);
  }
}

// This function matches gotos or labels-as-values with labels.
//
// We cannot resolve gotos as we parse a function because gotos
// can refer a label that appears later in the function.
// So, we need to do this after we parse the entire function.
static void resolve_goto_labels(void) {
  for (Node *x = gotos; x; x = x->goto_next) {
    for (Node *y = labels; y; y = y->goto_next) {
      if (!strcmp(x->label, y->label)) {
        x->unique_label = y->unique_label;
        break;
      }
    }

    if (x->unique_label == NULL)
      error_tok(x->tok->next, "use of undeclared label");
  }

  gotos = labels = NULL;
}

static Obj *find_func(char *name) {
  Scope *sc = scope;
  while (sc->next)
    sc = sc->next;

  VarScope *sc2 = hashmap_get(&sc->vars, name);
  if (sc2 && sc2->var && sc2->var->is_function)
    return sc2->var;
  return NULL;
}

static bool is_incomplete_struct(Type *ty) {
  return ty->kind == TY_STRUCT && ty->size < 0;
}

static Type *assume_external_ppc_generalization(Type *ty, Token *tok) {
  if (!is_incomplete_struct(ty))
    return NULL;

  if (!ty->ppc_name)
    error_tok(tok, "PPC forward generalization must be a named struct");

  ty->is_ppc_generalization = true;
  ensure_ppc_count_var(ty);
  return ty;
}

static PpcFunc *find_ppc_func(Token *tok) {
  for (PpcFunc *fn = ppc_funcs; fn; fn = fn->next)
    if (fn->name_tok->len == tok->len && !strncmp(fn->name_tok->loc, tok->loc, tok->len))
      return fn;
  return NULL;
}

static Type *ppc_param_base_ty(Type *ty, Token *tok) {
  if (ty->kind != TY_PTR)
    error_tok(tok, "PPC parameter must be a pointer");

  Type *base = ty->base;
  if (base->ppc_base)
    return base->ppc_base;
  if (base->is_ppc_generalization)
    return base;
  if (assume_external_ppc_generalization(base, tok))
    return base;

  error_tok(tok, "PPC parameter must point to a generalization or specialization");
}

static int ppc_param_index(Type *ty) {
  if (ty->kind != TY_PTR)
    return -1;

  Type *base = ty->base;
  if (!base->ppc_base) {
    if (!base->is_ppc_generalization)
      assume_external_ppc_generalization(base, ty->name);
    return 0;
  }

  Member *tag = ppc_tag_member(base);
  return tag ? tag->ppc_index : -1;
}

static bool ppc_param_is_wildcard(Type *ty) {
  if (ty->kind != TY_PTR)
    return false;

  Type *base = ty->base;
  if (!base->ppc_base && !base->is_ppc_generalization)
    assume_external_ppc_generalization(base, ty->name);

  return base->is_ppc_generalization && !base->ppc_base;
}

static int type_list_len(Type *param) {
  int len = 0;
  for (; param; param = param->next)
    len++;
  return len;
}

static Type **ppc_base_ty_array(Type *params, int count) {
  Type **bases = calloc(count, sizeof(Type *));
  int i = 0;

  for (Type *param = params; param; param = param->next)
    bases[i++] = ppc_param_base_ty(param, param->name);

  return bases;
}

static int *ppc_index_array(Type *params, int count) {
  int *indices = calloc(count, sizeof(int));
  int i = 0;

  for (Type *param = params; param; param = param->next)
    indices[i++] = ppc_param_index(param);

  return indices;
}

static bool *ppc_wildcard_array(Type *params, int count) {
  bool *wildcards = calloc(count, sizeof(bool));
  int i = 0;

  for (Type *param = params; param; param = param->next)
    wildcards[i++] = ppc_param_is_wildcard(param);

  return wildcards;
}

static bool same_ppc_base_list(PpcFunc *fn, Type *params) {
  if (type_list_len(params) != fn->ppc_param_count)
    return false;

  int i = 0;
  for (Type *param = params; param; param = param->next)
    if (ppc_param_base_ty(param, param->name) != fn->base_tys[i++])
      return false;
  return true;
}

static bool same_ppc_handler_key(PpcFunc *fn, int *indices, bool *wildcards,
                                 PpcHandler *handler) {
  for (int i = 0; i < fn->ppc_param_count; i++)
    if (handler->indices[i] != indices[i] || handler->wildcards[i] != wildcards[i])
      return false;
  return true;
}

static bool ppc_all_wildcards(PpcFunc *fn, bool *wildcards) {
  for (int i = 0; i < fn->ppc_param_count; i++)
    if (!wildcards[i])
      return false;
  return true;
}

static bool ppc_any_wildcard(PpcFunc *fn, bool *wildcards) {
  for (int i = 0; i < fn->ppc_param_count; i++)
    if (wildcards[i])
      return true;
  return false;
}

static PpcHandler *find_ppc_handler(PpcFunc *fn, int *indices, bool *wildcards) {
  for (PpcHandler *h = fn->handlers; h; h = h->next)
    if (same_ppc_handler_key(fn, indices, wildcards, h))
      return h;
  return NULL;
}

static Type *copy_param_list(Type *param) {
  Type head = {};
  Type *cur = &head;

  for (; param; param = param->next) {
    cur = cur->next = copy_type(param);
    cur->next = NULL;
  }

  return head.next;
}

static bool same_param_list(Type *a, Type *b) {
  for (; a && b; a = a->next, b = b->next)
    if (!is_compatible(a, b))
      return false;
  return !a && !b;
}

static Node *clone_node(Node *node) {
  Node *copy = calloc(1, sizeof(Node));
  *copy = *node;
  copy->next = NULL;
  return copy;
}

static Type *ppc_actual_func_type(Type *return_ty, Type *ppc_params, Type *regular_params,
                                  bool is_variadic) {
  Type *ty = func_type(return_ty);
  Type *params = copy_param_list(ppc_params);
  Type **tail = &params;
  while (*tail)
    tail = &(*tail)->next;
  *tail = copy_param_list(regular_params);
  ty->params = params;
  ty->is_variadic = is_variadic;
  return ty;
}

static char *ppc_mangled_name(PpcFunc *fn, PpcHandler *handler) {
  char *name = format("__ppc_%s", fn->name);

  int i = 0;
  for (Type *param = handler->param_tys; param; param = param->next, i++) {
    if (handler->wildcards[i]) {
      name = format("%s_g", name);
      continue;
    }

    if (param->kind != TY_PTR)
      error_tok(param->name_pos, "PPC parameter must be a pointer");
    name = format("%s_%s", name, ppc_type_name(param->base));
  }
  return name;
}

static Obj *get_ppc_handler_obj(PpcFunc *ppc_fn, PpcHandler *handler, Type *fn_ty,
                                VarAttr *attr, Token *tok, bool is_definition) {
  char *name = ppc_mangled_name(ppc_fn, handler);
  bool is_static = attr->is_static || (attr->is_inline && !attr->is_extern);
  Obj *fn = find_func(name);

  if (fn) {
    if (!fn->is_function)
      error_tok(tok, "redeclared as a different kind of symbol");
    if (fn->is_definition && is_definition)
      error_tok(tok, "redefinition of %s", name);
    if (!fn->is_static && is_static)
      error_tok(tok, "static declaration follows a non-static declaration");
    fn->is_definition = fn->is_definition || is_definition;
    fn->is_inline = fn->is_inline || attr->is_inline;
    return fn;
  }

  fn = new_gvar(name, fn_ty);
  fn->is_function = true;
  fn->is_definition = is_definition;
  fn->is_static = is_static;
  fn->is_inline = attr->is_inline;
  return fn;
}

static Token *finish_function(Token *tok, Obj *fn, Type *ty) {
  current_fn = fn;
  locals = NULL;
  enter_scope();
  create_param_lvars(ty->params);

  // A buffer for a struct/union return value is passed
  // as the hidden first parameter.
  Type *rty = ty->return_ty;
  if ((rty->kind == TY_STRUCT || rty->kind == TY_UNION) && rty->size > 16)
    new_lvar("", pointer_to(rty));

  fn->params = locals;

  if (ty->is_variadic)
    fn->va_area = new_lvar("__va_area__", array_of(ty_char, 136));
  fn->alloca_bottom = new_lvar("__alloca_size__", pointer_to(ty_char));

  tok = skip(tok, "{");

  // [https://www.sigbus.info/n1570#6.4.2.2p1] "__func__" is
  // automatically defined as a local variable containing the
  // current function name.
  push_scope("__func__")->var =
    new_string_literal(fn->name, array_of(ty_char, strlen(fn->name) + 1));

  // [GNU] __FUNCTION__ is yet another name of __func__.
  push_scope("__FUNCTION__")->var =
    new_string_literal(fn->name, array_of(ty_char, strlen(fn->name) + 1));

  fn->body = compound_stmt(&tok, tok);
  fn->locals = locals;
  leave_scope();
  resolve_goto_labels();
  return tok;
}

static void mark_live(Obj *var) {
  if (!var->is_function || var->is_live)
    return;
  var->is_live = true;

  for (int i = 0; i < var->refs.len; i++) {
    Obj *fn = find_func(var->refs.data[i]);
    if (fn)
      mark_live(fn);
  }
}

static Obj *get_lib_func(char *name, Type *ty) {
  Obj *fn = find_func(name);
  if (fn)
    return fn;

  fn = new_gvar(name, ty);
  fn->is_function = true;
  fn->is_definition = false;
  fn->is_static = false;
  return fn;
}

static Obj *get_calloc_func(void) {
  Type *ty = func_type(pointer_to(ty_void));
  Type *p1 = copy_type(ty_ulong);
  Type *p2 = copy_type(ty_ulong);
  p1->next = p2;
  ty->params = p1;
  return get_lib_func("calloc", ty);
}

static Obj *get_free_func(void) {
  Type *ty = func_type(ty_void);
  ty->params = copy_type(pointer_to(ty_void));
  return get_lib_func("free", ty);
}

static Obj *get_dprintf_func(void) {
  Type *ty = func_type(ty_int);
  Type *p1 = copy_type(ty_int);
  Type *p2 = copy_type(pointer_to(ty_char));
  p1->next = p2;
  ty->params = p1;
  ty->is_variadic = true;
  return get_lib_func("dprintf", ty);
}

static Obj *get_abort_func(void) {
  return get_lib_func("abort", func_type(ty_void));
}

static Node *new_func_call_node(Obj *fn, Node *args, Token *tok) {
  Node *fn_node = new_var_node(fn, tok);
  add_type(fn_node);

  Node *node = new_unary(ND_FUNCALL, fn_node, tok);
  node->func_ty = fn->ty;
  node->args = args;
  return node;
}

static Type *ppc_object_family_base_ty(Token *tok, Type *ty, char *name) {
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

static Type *ppc_generalization_arg(Token *tok, Type *ty, char *name) {
  if (ty->kind != TY_STRUCT || !ty->is_ppc_generalization)
    error_tok(tok, "%s expects a PPC generalization type", name);

  ensure_ppc_count_var(ty);
  remember_ppc_gen(ty, tok);
  return ty;
}

static Node *ppc_get_spec_size_expr(Token *tok, Type *ty) {
  Type *base = ppc_generalization_arg(tok, ty, "get_spec_size");
  return ppc_extent_expr(base, tok);
}

static Node *ppc_get_spec_ptr_expr(Token *tok, Type *ty, Node *idx) {
  if (!current_fn)
    error_tok(tok, "get_spec_ptr is not allowed outside a function");

  Type *base = ppc_generalization_arg(tok, ty, "get_spec_ptr");
  ensure_ppc_spec_ptr_tags_var(base);

  Obj *idx_var = new_lvar("", ty_long);

  Node *assign_idx =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(idx_var, tok),
                                  new_cast(idx, ty_long), tok),
                       tok);

  Node *size = ppc_extent_expr(base, tok);
  Node *lower_bound = new_num(0, tok);
  Node *lower = new_binary(ND_LT, new_var_node(idx_var, tok), lower_bound, tok);
  Node *upper = new_binary(ND_LE, size, new_var_node(idx_var, tok), tok);
  Node *cond = new_binary(ND_LOGOR, lower, upper, tok);

  char *fmt =
    format("PPC get_spec_ptr error: index %%ld out of range for %s (size %%d)\n",
           ppc_type_name(base));
  Obj *fmt_var = new_string_literal(fmt, array_of(ty_char, strlen(fmt) + 1));

  Node *args = new_num(2, tok);
  args->next = new_cast(new_var_node(fmt_var, tok), pointer_to(ty_char));
  args->next->next = new_var_node(idx_var, tok);
  args->next->next->next = ppc_extent_expr(base, tok);

  Node *print = new_expr_stmt_node(new_func_call_node(get_dprintf_func(), args, tok), tok);
  Node *abort = new_expr_stmt_node(new_func_call_node(get_abort_func(), NULL, tok), tok);
  print->next = abort;

  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = cond;
  if_node->then = new_block_node(print, tok);

  Node *ptr =
    new_cast(new_add(new_var_node(base->ppc_spec_ptr_tags_var, tok),
                     new_var_node(idx_var, tok), tok),
             pointer_to(base));
  Node *ret = new_expr_stmt_node(ptr, tok);

  assign_idx->next = if_node;
  if_node->next = ret;

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = assign_idx;
  return node;
}

static Node *ppc_spec_index_cmp_expr(Token *tok, Node *lhs, Node *rhs) {
  if (!current_fn)
    error_tok(tok, "spec_index_cmp is not allowed outside a function");

  add_type(lhs);
  add_type(rhs);

  Type *base = ppc_param_base_ty(lhs->ty, tok);
  Type *rhs_base = ppc_param_base_ty(rhs->ty, tok);
  if (base != rhs_base)
    error_tok(tok, "spec_index_cmp operands must belong to the same PPC generalization family");

  Obj *lhs_ptr = new_lvar("", pointer_to(base));
  Obj *rhs_ptr = new_lvar("", pointer_to(base));
  Obj *lhs_idx = new_lvar("", ty_int);
  Obj *rhs_idx = new_lvar("", ty_int);

  Node *assign_lhs_ptr =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(lhs_ptr, tok),
                                  new_cast(lhs, pointer_to(base)), tok),
                       tok);
  Node *assign_rhs_ptr =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(rhs_ptr, tok),
                                  new_cast(rhs, pointer_to(base)), tok),
                       tok);
  Node *assign_lhs_idx =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(lhs_idx, tok),
                                  ppc_tag_expr(lhs_ptr, base, tok), tok),
                       tok);
  Node *assign_rhs_idx =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(rhs_idx, tok),
                                  ppc_tag_expr(rhs_ptr, base, tok), tok),
                       tok);

  Node *ret = new_node(ND_COND, tok);
  ret->cond = new_binary(ND_EQ, new_var_node(lhs_idx, tok), new_var_node(rhs_idx, tok), tok);
  ret->then = new_var_node(lhs_idx, tok);
  ret->els = new_num(-1, tok);

  Node *ret_stmt = new_expr_stmt_node(ret, tok);
  assign_lhs_ptr->next = assign_rhs_ptr;
  assign_rhs_ptr->next = assign_lhs_idx;
  assign_lhs_idx->next = assign_rhs_idx;
  assign_rhs_idx->next = ret_stmt;

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = assign_lhs_ptr;
  return node;
}

static Type *ppc_generic_func_type(PpcFunc *ppc_fn) {
  if (ppc_fn->generic_func_ty)
    return ppc_fn->generic_func_ty;

  Type *ty = func_type(ppc_fn->return_ty);
  Type head = {};
  Type *cur = &head;

  for (int i = 0; i < ppc_fn->ppc_param_count; i++)
    cur = cur->next = copy_type(pointer_to(ppc_fn->base_tys[i]));
  cur->next = copy_param_list(ppc_fn->regular_params);

  ty->params = head.next;
  ty->is_variadic = ppc_fn->is_variadic;
  ppc_fn->generic_func_ty = ty;
  ppc_fn->fnptr_ty = pointer_to(ty);
  return ty;
}

static Obj *ppc_default_handler(PpcFunc *ppc_fn, Token *tok) {
  for (PpcHandler *handler = ppc_fn->handlers; handler; handler = handler->next)
    if (handler->is_default && !handler->is_abstract && handler->fn)
      return handler->fn;

  if (ppc_fn->default_fn)
    return ppc_fn->default_fn;

  Obj *fn = new_generated_func(format("__ppc_%s_default", ppc_fn->name),
                               ppc_generic_func_type(ppc_fn), 0);
  fn->is_static = true;
  fn->init_priority = 0;

  Obj *locals = NULL;
  Obj **params = calloc(ppc_fn->ppc_param_count, sizeof(Obj *));
  for (int i = ppc_fn->ppc_param_count - 1; i >= 0; i--)
    params[i] = new_generated_lvar(&locals, pointer_to(ppc_fn->base_tys[i]));
  fn->params = locals;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));

  char *fmt = format("PPC dispatch error: uncovered handler for %s<", ppc_fn->name);
  for (int i = 0; i < ppc_fn->ppc_param_count; i++)
    fmt = format("%s%s%%d", fmt, i ? ", " : "");
  fmt = format("%s>()\n", fmt);

  Obj *fmt_var = new_string_literal(fmt, array_of(ty_char, strlen(fmt) + 1));

  Node head = {};
  Node *cur = &head;
  cur = cur->next = new_num(2, tok);
  cur = cur->next = new_cast(new_var_node(fmt_var, tok), pointer_to(ty_char));
  for (int i = 0; i < ppc_fn->ppc_param_count; i++)
    cur = cur->next = ppc_tag_expr(params[i], ppc_fn->base_tys[i], tok);

  Node *print = new_expr_stmt_node(new_func_call_node(get_dprintf_func(), head.next, tok), tok);
  Node *abort = new_expr_stmt_node(new_func_call_node(get_abort_func(), NULL, tok), tok);
  print->next = abort;

  fn->body = new_block_node(print, tok);
  fn->locals = locals;
  add_type(fn->body);

  ppc_fn->default_fn = fn;
  return fn;
}

static Node *ppc_table_slot(Obj *table, Node *idx, Token *tok) {
  return new_unary(ND_DEREF, new_add(new_var_node(table, tok), idx, tok), tok);
}

static Node *ppc_cast_fnptr(Node *fn, Type *fnptr_ty) {
  return new_cast(fn, fnptr_ty);
}

static Node *ppc_extent_expr(Type *base, Token *tok) {
  return new_add(new_var_node(base->ppc_count_var, tok), new_num(1, tok), tok);
}

static Node *ppc_table_size_expr(PpcFunc *ppc_fn, Token *tok) {
  Node *size = new_num(1, tok);

  for (int i = 0; i < ppc_fn->ppc_param_count; i++)
    size = new_binary(ND_MUL, size, ppc_extent_expr(ppc_fn->base_tys[i], tok), tok);
  return size;
}

static bool ppc_has_concrete_default_handler(PpcFunc *ppc_fn) {
  for (PpcHandler *handler = ppc_fn->handlers; handler; handler = handler->next)
    if (handler->is_default && !handler->is_abstract && handler->fn)
      return true;
  return false;
}

static bool ppc_needs_exhaustive_check(PpcFunc *ppc_fn) {
  if (ppc_has_concrete_default_handler(ppc_fn))
    return false;

  for (int i = 0; i < ppc_fn->ppc_param_count; i++)
    if (!ppc_fn->base_tys[i]->is_ppc_const_generalization)
      return false;
  return true;
}

static Node *ppc_mixed_index_expr(PpcFunc *ppc_fn, Node **tags, Token *tok) {
  Node *idx = tags[0];

  for (int i = 1; i < ppc_fn->ppc_param_count; i++) {
    idx = new_binary(ND_MUL, idx, ppc_extent_expr(ppc_fn->base_tys[i], tok), tok);
    idx = new_add(idx, tags[i], tok);
  }
  return idx;
}

static Node *ppc_handler_tag_expr(Type *param_ty, Token *tok) {
  if (ppc_param_index(param_ty) == 0)
    return new_num(0, tok);

  Member *tag = ppc_tag_member(param_ty->base);
  if (!tag || !tag->ppc_tag_var)
    error_tok(tok, "PPC specialization has no runtime tag");
  return new_var_node(tag->ppc_tag_var, tok);
}

static Node *ppc_handler_table_index(PpcFunc *ppc_fn, PpcHandler *handler, Token *tok) {
  Node **tags = calloc(ppc_fn->ppc_param_count, sizeof(Node *));
  int i = 0;

  for (Type *param = handler->param_tys; param; param = param->next)
    tags[i++] = ppc_handler_tag_expr(param, tok);

  return ppc_mixed_index_expr(ppc_fn, tags, tok);
}

static void ppc_make_spec_ctor(Type *base, Member *spec) {
  if (!spec->ppc_tag_var)
    return;

  static int id = 0;
  Token *tok = spec->tok ? spec->tok : spec->name;
  Obj *fn = new_generated_func(format("__ppc_ctor_101_spec_%d", id++),
                               func_type(ty_void), 101);

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));

  Node *early_return = new_node(ND_RETURN, tok);
  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = new_var_node(spec->ppc_tag_var, tok);
  if_node->then = early_return;

  Node *count = new_var_node(base->ppc_count_var, tok);
  Node *inc = new_binary(ND_ASSIGN, new_var_node(base->ppc_count_var, tok),
                         new_add(new_var_node(base->ppc_count_var, tok), new_num(1, tok), tok),
                         tok);
  Node *set_tag = new_binary(ND_ASSIGN, new_var_node(spec->ppc_tag_var, tok), count, tok);

  Node *stmt1 = if_node;
  Node *stmt2 = new_expr_stmt_node(inc, tok);
  Node *stmt3 = new_expr_stmt_node(set_tag, tok);
  stmt1->next = stmt2;
  stmt2->next = stmt3;

  fn->body = new_block_node(stmt1, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_spec_ptr_tags_ctor(PpcGen *gen) {
  Type *base = gen->ty;
  if (!base->ppc_spec_ptr_tags_var)
    return;

  static int id = 0;
  Token *tok = gen->tok;
  Obj *fn = new_generated_func(format("__ppc_ctor_151_spec_ptr_tags_%d", id++),
                               func_type(ty_void), 151);

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));
  Obj *i = new_generated_lvar(&locals, ty_int);

  Node *early_return = new_node(ND_RETURN, tok);
  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = new_var_node(base->ppc_spec_ptr_tags_var, tok);
  if_node->then = early_return;

  Node *arg1 = new_cast(ppc_extent_expr(base, tok), ty_ulong);
  Node *arg2 = new_ulong(ty_int->size, tok);
  arg1->next = arg2;
  Node *calloc_call = new_func_call_node(get_calloc_func(), arg1, tok);
  Node *alloc =
    new_expr_stmt_node(new_binary(ND_ASSIGN,
                                  new_var_node(base->ppc_spec_ptr_tags_var, tok),
                                  new_cast(calloc_call, base->ppc_spec_ptr_tags_var->ty),
                                  tok),
                       tok);

  Node *init = new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(i, tok),
                                             new_num(0, tok), tok),
                                  tok);
  Node *cond = new_binary(ND_LT, new_var_node(i, tok), ppc_extent_expr(base, tok), tok);
  Node *inc = new_binary(ND_ASSIGN, new_var_node(i, tok),
                         new_add(new_var_node(i, tok), new_num(1, tok), tok), tok);

  Node *slot =
    new_unary(ND_DEREF,
              new_add(new_var_node(base->ppc_spec_ptr_tags_var, tok),
                      new_var_node(i, tok), tok),
              tok);
  Node *fill = new_expr_stmt_node(new_binary(ND_ASSIGN, slot, new_var_node(i, tok), tok), tok);

  Node *loop = new_node(ND_FOR, tok);
  loop->init = init;
  loop->cond = cond;
  loop->inc = inc;
  loop->then = new_block_node(fill, tok);
  loop->brk_label = new_unique_name();
  loop->cont_label = new_unique_name();

  if_node->next = alloc;
  alloc->next = loop;

  fn->body = new_block_node(if_node, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_spec_ptr_tags_dtor(PpcGen *gen) {
  Type *base = gen->ty;
  if (!base->ppc_spec_ptr_tags_var)
    return;

  static int id = 0;
  Token *tok = gen->tok;
  Obj *fn = new_generated_func(format("__ppc_dtor_151_spec_ptr_tags_%d", id++),
                               func_type(ty_void), 0);
  fn->fini_priority = 151;

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));

  Node *early_return = new_node(ND_RETURN, tok);
  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = new_unary(ND_NOT, new_var_node(base->ppc_spec_ptr_tags_var, tok), tok);
  if_node->then = early_return;

  Node *arg = new_cast(new_var_node(base->ppc_spec_ptr_tags_var, tok), pointer_to(ty_void));
  Node *free_call = new_func_call_node(get_free_func(), arg, tok);
  Node *free_stmt = new_expr_stmt_node(free_call, tok);
  Node *clear =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(base->ppc_spec_ptr_tags_var, tok),
                                  new_num(0, tok), tok),
                       tok);

  if_node->next = free_stmt;
  free_stmt->next = clear;

  fn->body = new_block_node(if_node, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_table_ctor(PpcFunc *ppc_fn) {
  static int id = 0;
  Token *tok = ppc_fn->name_tok;
  Type *generic_ty = ppc_generic_func_type(ppc_fn);
  Type *fnptr_ty = pointer_to(generic_ty);

  Obj *fn = new_generated_func(format("__ppc_ctor_201_table_%d", id++),
                               func_type(ty_void), 201);

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));
  Obj *i = new_generated_lvar(&locals, ty_int);

  Node *early_return = new_node(ND_RETURN, tok);
  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = new_var_node(ppc_fn->table_var, tok);
  if_node->then = early_return;

  Node *table_size = ppc_table_size_expr(ppc_fn, tok);
  Node *arg1 = new_cast(table_size, ty_ulong);
  Node *arg2 = new_ulong(8, tok);
  arg1->next = arg2;
  Node *calloc_call = new_func_call_node(get_calloc_func(), arg1, tok);
  Node *alloc = new_binary(ND_ASSIGN, new_var_node(ppc_fn->table_var, tok),
                           new_cast(calloc_call, ppc_fn->table_var->ty), tok);

  Node *init = new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(i, tok),
                                             new_num(0, tok), tok),
                                  tok);
  Node *cond = new_binary(ND_LT, new_var_node(i, tok),
                          ppc_table_size_expr(ppc_fn, tok), tok);
  Node *inc = new_binary(ND_ASSIGN, new_var_node(i, tok),
                         new_add(new_var_node(i, tok), new_num(1, tok), tok), tok);

  Node *slot = ppc_table_slot(ppc_fn->table_var, new_var_node(i, tok), tok);
  Node *def = ppc_needs_exhaustive_check(ppc_fn) ?
    new_cast(new_num(0, tok), fnptr_ty) :
    ppc_cast_fnptr(new_var_node(ppc_default_handler(ppc_fn, tok), tok), fnptr_ty);
  Node *fill = new_expr_stmt_node(new_binary(ND_ASSIGN, slot, def, tok), tok);

  Node *loop = new_node(ND_FOR, tok);
  loop->init = init;
  loop->cond = cond;
  loop->inc = inc;
  loop->then = new_block_node(fill, tok);
  loop->brk_label = new_unique_name();
  loop->cont_label = new_unique_name();

  Node *stmt1 = if_node;
  Node *stmt2 = new_expr_stmt_node(alloc, tok);
  stmt1->next = stmt2;
  stmt2->next = loop;

  fn->body = new_block_node(stmt1, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_exhaustive_check_ctor(PpcFunc *ppc_fn) {
  if (!ppc_needs_exhaustive_check(ppc_fn))
    return;

  static int id = 0;
  Token *tok = ppc_fn->name_tok;
  Obj *fn = new_generated_func(format("__ppc_ctor_351_exhaustive_%d", id++),
                               func_type(ty_void), 351);

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));
  Obj *i = new_generated_lvar(&locals, ty_long);

  Node *init = new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(i, tok),
                                             new_num(0, tok), tok),
                                  tok);
  Node *cond = new_binary(ND_LT, new_var_node(i, tok),
                          new_cast(ppc_table_size_expr(ppc_fn, tok), ty_long),
                          tok);
  Node *inc = new_binary(ND_ASSIGN, new_var_node(i, tok),
                         new_add(new_var_node(i, tok), new_num(1, tok), tok), tok);

  Node *missing = new_unary(ND_NOT,
                            ppc_table_slot(ppc_fn->table_var, new_var_node(i, tok), tok),
                            tok);

  char *fmt = format("PPC completeness error: missing handler for %s table slot %%ld\n",
                     ppc_fn->name);
  Obj *fmt_var = new_string_literal(fmt, array_of(ty_char, strlen(fmt) + 1));

  Node *args = new_num(2, tok);
  args->next = new_cast(new_var_node(fmt_var, tok), pointer_to(ty_char));
  args->next->next = new_var_node(i, tok);

  Node *print = new_expr_stmt_node(new_func_call_node(get_dprintf_func(), args, tok), tok);
  Node *abort = new_expr_stmt_node(new_func_call_node(get_abort_func(), NULL, tok), tok);
  print->next = abort;

  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = missing;
  if_node->then = new_block_node(print, tok);

  Node *loop = new_node(ND_FOR, tok);
  loop->init = init;
  loop->cond = cond;
  loop->inc = inc;
  loop->then = new_block_node(if_node, tok);
  loop->brk_label = new_unique_name();
  loop->cont_label = new_unique_name();

  fn->body = new_block_node(loop, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_table_dtor(PpcFunc *ppc_fn) {
  static int id = 0;
  Token *tok = ppc_fn->name_tok;

  Obj *fn = new_generated_func(format("__ppc_dtor_201_table_%d", id++),
                               func_type(ty_void), 0);
  fn->fini_priority = 201;

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));

  Node *early_return = new_node(ND_RETURN, tok);
  Node *if_node = new_node(ND_IF, tok);
  if_node->cond = new_unary(ND_NOT, new_var_node(ppc_fn->table_var, tok), tok);
  if_node->then = early_return;

  Node *arg = new_cast(new_var_node(ppc_fn->table_var, tok), pointer_to(ty_void));
  Node *free_call = new_func_call_node(get_free_func(), arg, tok);
  Node *free_stmt = new_expr_stmt_node(free_call, tok);
  Node *clear =
    new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(ppc_fn->table_var, tok),
                                  new_num(0, tok), tok),
                       tok);

  if_node->next = free_stmt;
  free_stmt->next = clear;

  fn->body = new_block_node(if_node, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_handler_ctor(PpcFunc *ppc_fn, PpcHandler *handler) {
  if (handler->is_default || handler->is_abstract || !handler->fn)
    return;

  static int id = 0;
  Token *tok = handler->param_tys && handler->param_tys->name ?
    handler->param_tys->name : ppc_fn->name_tok;
  Type *fnptr_ty = pointer_to(ppc_generic_func_type(ppc_fn));

  Obj *fn = new_generated_func(format("__ppc_ctor_301_handler_%d", id++),
                               func_type(ty_void), 301);

  Obj *locals = NULL;
  fn->alloca_bottom = new_generated_lvar(&locals, pointer_to(ty_char));

  Node *slot = ppc_table_slot(ppc_fn->table_var, ppc_handler_table_index(ppc_fn, handler, tok), tok);
  Node *rhs = ppc_cast_fnptr(new_var_node(handler->fn, tok), fnptr_ty);
  Node *stmt = new_expr_stmt_node(new_binary(ND_ASSIGN, slot, rhs, tok), tok);

  fn->body = new_block_node(stmt, tok);
  fn->locals = locals;
  add_type(fn->body);
}

static void ppc_make_spec_ctors(Type *base, Member *spec) {
  if (!spec)
    return;
  ppc_make_spec_ctors(base, spec->next);
  ppc_make_spec_ctor(base, spec);
}

static void finalize_ppc_runtime(void) {
  for (PpcGen *gen = ppc_gens; gen; gen = gen->next) {
    ppc_make_spec_ctors(gen->ty, gen->ty->ppc_specs);
    ppc_make_spec_ptr_tags_ctor(gen);
    ppc_make_spec_ptr_tags_dtor(gen);
  }

  for (PpcFunc *fn = ppc_funcs; fn; fn = fn->next) {
    ppc_make_table_ctor(fn);
    ppc_make_table_dtor(fn);
    for (PpcHandler *handler = fn->handlers; handler; handler = handler->next)
      ppc_make_handler_ctor(fn, handler);
    ppc_make_exhaustive_check_ctor(fn);
  }
}

static bool is_ppc_function(Token *tok) {
  if (equal(tok, ";"))
    return false;

  while (equal(tok, "*")) {
    tok = tok->next;
    while (equal(tok, "const") || equal(tok, "volatile") || equal(tok, "restrict") ||
           equal(tok, "__restrict") || equal(tok, "__restrict__"))
      tok = tok->next;
  }

  return tok->kind == TK_IDENT && equal(tok->next, "<");
}

static PpcFunc *get_or_create_ppc_func(Token *name, Type *return_ty, Type *ppc_params,
                                       Type *regular_params, bool is_variadic) {
  PpcFunc *fn = find_ppc_func(name);
  if (fn) {
    if (!same_ppc_base_list(fn, ppc_params))
      error_tok(name, "PPC function redeclared for different generalizations");
    if (!is_compatible(fn->return_ty, return_ty))
      error_tok(name, "PPC function redeclared with a different return type");
    if (fn->is_variadic != is_variadic || !same_param_list(fn->regular_params, regular_params))
      error_tok(name, "PPC function redeclared with different ordinary parameters");
    return fn;
  }

  fn = calloc(1, sizeof(PpcFunc));
  fn->name = get_ident(name);
  fn->name_tok = name;
  fn->return_ty = return_ty;
  fn->ppc_param_count = type_list_len(ppc_params);
  fn->base_tys = ppc_base_ty_array(ppc_params, fn->ppc_param_count);
  fn->regular_params = copy_param_list(regular_params);
  fn->is_variadic = is_variadic;
  for (int i = 0; i < fn->ppc_param_count; i++)
    ensure_ppc_count_var(fn->base_tys[i]);
  fn->table_var =
    new_ppc_common_var(format("__ppc_%s_table", fn->name),
                       pointer_to(pointer_to(ppc_generic_func_type(fn))));
  fn->next = ppc_funcs;
  ppc_funcs = fn;
  return fn;
}

static Type *ppc_param(Token **rest, Token *tok) {
  Type *ty = declspec(&tok, tok, NULL);
  ty = declarator(&tok, tok, ty);
  if (!ty->name)
    error_tok(ty->name_pos, "PPC parameter name omitted");
  *rest = tok;
  return ty;
}

static Type *ppc_param_list(Token **rest, Token *tok) {
  Type head = {};
  Type *cur = &head;

  while (!equal(tok, ">")) {
    if (cur != &head)
      tok = skip(tok, ",");

    cur = cur->next = ppc_param(&tok, tok);
    cur->next = NULL;
  }

  *rest = tok;
  return head.next;
}

static Token *ppc_function(Token *tok, Type *basety, VarAttr *attr) {
  basety = pointers(&tok, tok, basety);

  Token *name = tok;
  get_ident(tok);
  tok = skip(tok->next, "<");

  Type *ppc_param_tys = ppc_param_list(&tok, tok);
  tok = skip(tok, ">");

  if (!equal(tok, "("))
    error_tok(tok, "expected '(' after PPC parameter list");
  Type *ordinary_fn_ty = func_params(&tok, tok->next, basety);

  int ppc_param_count = type_list_len(ppc_param_tys);
  if (!ppc_param_count)
    error_tok(name, "PPC function requires at least one PPC parameter");
  int *indices = ppc_index_array(ppc_param_tys, ppc_param_count);
  bool *wildcards = ppc_wildcard_array(ppc_param_tys, ppc_param_count);

  PpcFunc *ppc_fn = get_or_create_ppc_func(name, ordinary_fn_ty->return_ty, ppc_param_tys,
                                           ordinary_fn_ty->params, ordinary_fn_ty->is_variadic);

  PpcHandler *handler = find_ppc_handler(ppc_fn, indices, wildcards);
  if (!handler) {
    handler = calloc(1, sizeof(PpcHandler));
    handler->indices = indices;
    handler->wildcards = wildcards;
    handler->param_tys = copy_param_list(ppc_param_tys);
    handler->is_default = ppc_all_wildcards(ppc_fn, wildcards);
    handler->next = ppc_fn->handlers;
    ppc_fn->handlers = handler;
  }

  if (equal(tok, "=")) {
    if (!handler->is_default)
      error_tok(tok, "only a generalizing PPC function may be abstract");
    if (handler->fn)
      error_tok(tok, "PPC handler already declared");
    tok = tok->next;
    if (tok->kind != TK_NUM || tok->val != 0)
      error_tok(tok, "expected 0");
    handler->is_abstract = true;
    return skip(tok->next, ";");
  }

  if (!handler->is_default && ppc_any_wildcard(ppc_fn, handler->wildcards))
    error_tok(name, "mixed PPC wildcard handlers are not implemented; use .void for tag 0");

  Type *actual_ty = ppc_actual_func_type(ordinary_fn_ty->return_ty, ppc_param_tys,
                                         ordinary_fn_ty->params, ordinary_fn_ty->is_variadic);

  bool is_definition = equal(tok, "{");
  Obj *fn = get_ppc_handler_obj(ppc_fn, handler, actual_ty, attr, name, is_definition);
  handler->fn = fn;
  handler->is_abstract = false;
  fn->is_root = !(fn->is_static && fn->is_inline);

  if (consume(&tok, tok, ";"))
    return tok;
  if (!is_definition)
    error_tok(tok, "expected function body");

  return finish_function(tok, fn, actual_ty);
}

static Node *new_expr_stmt_node(Node *expr, Token *tok) {
  Node *node = new_unary(ND_EXPR_STMT, expr, tok);
  return node;
}

static Node *ppc_tag_expr(Obj *var, Type *base_ty, Token *tok) {
  Member *tag = ppc_tag_member(base_ty);
  if (!tag && base_ty->is_ppc_generalization && is_incomplete_struct(base_ty))
    return new_unary(ND_DEREF,
                     new_cast(new_var_node(var, tok), pointer_to(ty_int)),
                     tok);
  if (!tag)
    error_tok(tok, "PPC generalization has no specialization tag");

  Node *node = new_unary(ND_DEREF, new_var_node(var, tok), tok);
  node = new_unary(ND_MEMBER, node, tok);
  node->member = tag;
  return node;
}

static int node_list_len(Node *node) {
  int len = 0;
  for (; node; node = node->next)
    len++;
  return len;
}

static Node *ppc_runtime_dispatch(PpcFunc *ppc_fn, Node *ppc_args, Node *regular_args, Token *tok) {
  Obj **arg_vars = calloc(ppc_fn->ppc_param_count, sizeof(Obj *));
  Node **tags = calloc(ppc_fn->ppc_param_count, sizeof(Node *));
  Node head = {};
  Node *cur = &head;

  int i = 0;
  for (Node *arg = ppc_args; arg; arg = arg->next) {
    arg_vars[i] = new_lvar("", pointer_to(ppc_fn->base_tys[i]));
    cur = cur->next =
      new_expr_stmt_node(new_binary(ND_ASSIGN, new_var_node(arg_vars[i], tok),
                                    new_cast(arg, pointer_to(ppc_fn->base_tys[i])), tok),
                         tok);
    tags[i] = ppc_tag_expr(arg_vars[i], ppc_fn->base_tys[i], tok);
    i++;
  }

  Node *fn = ppc_table_slot(ppc_fn->table_var, ppc_mixed_index_expr(ppc_fn, tags, tok), tok);

  Type *fn_ty = ppc_generic_func_type(ppc_fn);
  Type *param_ty = fn_ty->params;

  Node arg_head = {};
  Node *arg_cur = &arg_head;

  for (int i = 0; i < ppc_fn->ppc_param_count; i++) {
    arg_cur = arg_cur->next = new_var_node(arg_vars[i], tok);
    param_ty = param_ty->next;
  }

  for (Node *arg = regular_args; arg; arg = arg->next) {
    Node *copy = clone_node(arg);
    add_type(copy);

    if (!param_ty && !fn_ty->is_variadic)
      error_tok(arg->tok, "too many arguments");

    if (param_ty) {
      if (param_ty->kind != TY_STRUCT && param_ty->kind != TY_UNION)
        copy = new_cast(copy, param_ty);
      param_ty = param_ty->next;
    } else if (copy->ty->kind == TY_FLOAT) {
      copy = new_cast(copy, ty_double);
    }

    arg_cur = arg_cur->next = copy;
  }

  if (param_ty)
    error_tok(tok, "too few arguments");

  Node *call = new_unary(ND_FUNCALL, fn, tok);
  call->func_ty = fn_ty;
  call->args = arg_head.next;

  if (fn_ty->return_ty->kind == TY_STRUCT || fn_ty->return_ty->kind == TY_UNION)
    call->ret_buffer = new_lvar("", fn_ty->return_ty);

  cur = cur->next = new_expr_stmt_node(call, tok);

  if (fn_ty->return_ty->kind == TY_VOID)
    cur = cur->next = new_expr_stmt_node(new_num(0, tok), tok);

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = head.next;
  return node;
}

static Node *ppc_funcall_expr(Token **rest, Token *tok) {
  PpcFunc *ppc_fn = find_ppc_func(tok);
  if (!ppc_fn)
    error_tok(tok, "unknown PPC function");

  Token *start = tok;
  tok = skip(tok->next, "<");

  Node ppc_head = {};
  Node *ppc_cur = &ppc_head;

  while (!equal(tok, ">")) {
    if (ppc_cur != &ppc_head)
      tok = skip(tok, ",");
    ppc_cur = ppc_cur->next = cast(&tok, tok);
  }

  tok = skip(tok, ">");
  tok = skip(tok, "(");

  Node head = {};
  Node *cur = &head;

  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");
    cur = cur->next = assign(&tok, tok);
  }

  *rest = skip(tok, ")");

  if (node_list_len(ppc_head.next) != ppc_fn->ppc_param_count)
    error_tok(start, "wrong number of PPC arguments");

  int i = 0;
  for (Node *arg = ppc_head.next; arg; arg = arg->next) {
    add_type(arg);
    Type *base_ty = ppc_param_base_ty(arg->ty, start);
    if (base_ty != ppc_fn->base_tys[i++])
      error_tok(start, "PPC call argument belongs to a different generalization");
  }

  return ppc_runtime_dispatch(ppc_fn, ppc_head.next, head.next, start);
}

static Token *function(Token *tok, Type *basety, VarAttr *attr) {
  Type *ty = declarator(&tok, tok, basety);
  if (!ty->name)
    error_tok(ty->name_pos, "function name omitted");
  char *name_str = get_ident(ty->name);

  Obj *fn = find_func(name_str);
  if (fn) {
    // Redeclaration
    if (!fn->is_function)
      error_tok(tok, "redeclared as a different kind of symbol");
    if (fn->is_definition && equal(tok, "{"))
      error_tok(tok, "redefinition of %s", name_str);
    if (!fn->is_static && attr->is_static)
      error_tok(tok, "static declaration follows a non-static declaration");
    fn->is_definition = fn->is_definition || equal(tok, "{");
  } else {
    fn = new_gvar(name_str, ty);
    fn->is_function = true;
    fn->is_definition = equal(tok, "{");
    fn->is_static = attr->is_static || (attr->is_inline && !attr->is_extern);
    fn->is_inline = attr->is_inline;
  }

  fn->is_root = !(fn->is_static && fn->is_inline);

  if (consume(&tok, tok, ";"))
    return tok;

  return finish_function(tok, fn, ty);
}

static Token *global_variable(Token *tok, Type *basety, VarAttr *attr) {
  bool first = true;

  while (!consume(&tok, tok, ";")) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    Type *ty = declarator(&tok, tok, basety);
    if (!ty->name)
      error_tok(ty->name_pos, "variable name omitted");

    Obj *var = new_gvar(get_ident(ty->name), ty);
    var->is_definition = !attr->is_extern;
    var->is_static = attr->is_static;
    var->is_tls = attr->is_tls;
    if (attr->align)
      var->align = attr->align;

    Initializer *init = NULL;
    if (equal(tok, "="))
      init = gvar_initializer(&tok, tok->next, var);
    else if (!attr->is_extern && !attr->is_tls) {
      if (!var->init_data)
        var->is_tentative = true;
    }

    if (!attr->is_extern && !attr->is_tls)
      ppc_gvar_tag_initializer(var, init, tok);
  }
  return tok;
}

// Lookahead tokens and returns true if a given token is a start
// of a function definition or declaration.
static bool is_function(Token *tok) {
  if (equal(tok, ";"))
    return false;

  Type dummy = {};
  Type *ty = declarator(&tok, tok, &dummy);
  return ty->kind == TY_FUNC;
}

// Remove redundant tentative definitions.
static void scan_globals(void) {
  Obj head;
  Obj *cur = &head;

  for (Obj *var = globals; var; var = var->next) {
    if (!var->is_tentative) {
      cur = cur->next = var;
      continue;
    }

    // Find another definition of the same identifier.
    Obj *var2 = globals;
    for (; var2; var2 = var2->next)
      if (var != var2 && var2->is_definition && !strcmp(var->name, var2->name))
        break;

    // If there's another definition, the tentative definition
    // is redundant
    if (!var2)
      cur = cur->next = var;
  }

  cur->next = NULL;
  globals = head.next;
}

static void declare_builtin_functions(void) {
  Type *ty = func_type(pointer_to(ty_void));
  ty->params = copy_type(ty_int);
  builtin_alloca = new_gvar("alloca", ty);
  builtin_alloca->is_definition = false;
}

// program = (typedef | function-definition | global-variable)*
Obj *parse(Token *tok) {
  declare_builtin_functions();
  globals = NULL;
  ppc_funcs = NULL;
  ppc_gens = NULL;
  ppc_nested_types = NULL;

  while (tok->kind != TK_EOF) {
    if (equal(tok, "struct") && tok->next->kind == TK_IDENT &&
        equal(tok->next->next, "+")) {
      Type *base = find_tag(tok->next);
      if (!base)
        error_tok(tok->next, "unknown PPC generalization");

      tok = ppc_extend_generalization(tok->next->next, base);
      tok = skip(tok, ";");
      continue;
    }

    VarAttr attr = {};
    Type *basety = declspec(&tok, tok, &attr);

    if (equal(tok, "+")) {
      tok = ppc_extend_generalization(tok, basety);
      tok = skip(tok, ";");
      continue;
    }

    // Typedef
    if (attr.is_typedef) {
      tok = parse_typedef(tok, basety);
      continue;
    }

    // Function
    if (is_ppc_function(tok)) {
      tok = ppc_function(tok, basety, &attr);
      continue;
    }

    if (is_function(tok)) {
      tok = function(tok, basety, &attr);
      continue;
    }

    // Global variable
    tok = global_variable(tok, basety, &attr);
  }

  finalize_ppc_runtime();

  for (Obj *var = globals; var; var = var->next)
    if (var->is_root)
      mark_live(var);

  // Remove redundant tentative definitions.
  scan_globals();
  return globals;
}
