#include "chibicc.h"

Type *ty_void = &(Type){TY_VOID, 1, 1};
Type *ty_bool = &(Type){TY_BOOL, 1, 1};
Type *ty_nullptr = &(Type){TY_NULLPTR, 8, 8};

Type *ty_char = &(Type){TY_CHAR, 1, 1};
Type *ty_short = &(Type){TY_SHORT, 2, 2};
Type *ty_int = &(Type){TY_INT, 4, 4};
Type *ty_long = &(Type){TY_LONG, 8, 8};

Type *ty_uchar = &(Type){TY_CHAR, 1, 1, true};
Type *ty_ushort = &(Type){TY_SHORT, 2, 2, true};
Type *ty_uint = &(Type){TY_INT, 4, 4, true};
Type *ty_ulong = &(Type){TY_LONG, 8, 8, true};

Type *ty_float = &(Type){TY_FLOAT, 4, 4};
Type *ty_double = &(Type){TY_DOUBLE, 8, 8};
Type *ty_ldouble = &(Type){TY_LDOUBLE, 16, 16};

static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

bool is_integer(Type *ty) {
  TypeKind k = ty->kind;
  return k == TY_BOOL || k == TY_CHAR || k == TY_SHORT ||
         k == TY_INT  || k == TY_LONG || k == TY_ENUM;
}

bool is_flonum(Type *ty) {
  return ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE ||
         ty->kind == TY_LDOUBLE;
}

bool is_numeric(Type *ty) {
  return is_integer(ty) || is_flonum(ty);
}

static bool is_aggregate(Type *ty) {
  return ty->kind == TY_STRUCT || ty->kind == TY_UNION;
}

static Type *canonical_type(Type *ty) {
  while (ty->origin)
    ty = ty->origin;
  return ty;
}

static Type *ppc_family_base(Type *ty) {
  ty = canonical_type(ty);

  if (ty->kind != TY_STRUCT)
    return NULL;
  if (ty->ppc_base)
    return canonical_type(ty->ppc_base);
  if (ty->is_ppc_generalization)
    return ty;
  return NULL;
}

static Member *ppc_tag_member_type(Type *ty) {
  ty = canonical_type(ty);

  if (ty->kind != TY_STRUCT)
    return NULL;

  for (Member *mem = ty->members; mem; mem = mem->next)
    if (mem->is_ppc_tag)
      return mem;
  return NULL;
}

static bool is_ppc_zero_specialization(Type *ty, Type *base) {
  ty = canonical_type(ty);

  if (ty == base)
    return true;
  if (ty->ppc_base != base)
    return false;

  Member *tag = ppc_tag_member_type(ty);
  return tag && tag->ppc_index == 0;
}

bool is_compatible(Type *t1, Type *t2) {
  if (t1 == t2)
    return true;

  if (t1->origin)
    return is_compatible(t1->origin, t2);

  if (t2->origin)
    return is_compatible(t1, t2->origin);

  if (t1->kind != t2->kind)
    return false;

  switch (t1->kind) {
  case TY_CHAR:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
    return t1->is_unsigned == t2->is_unsigned;
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
  case TY_NULLPTR:
    return true;
  case TY_PTR:
    return is_compatible(t1->base, t2->base);
  case TY_FUNC: {
    if (!is_compatible(t1->return_ty, t2->return_ty))
      return false;
    if (t1->is_variadic != t2->is_variadic)
      return false;

    Type *p1 = t1->params;
    Type *p2 = t2->params;
    for (; p1 && p2; p1 = p1->next, p2 = p2->next)
      if (!is_compatible(p1, p2))
        return false;
    return p1 == NULL && p2 == NULL;
  }
  case TY_ARRAY:
    if (!is_compatible(t1->base, t2->base))
      return false;
    return t1->array_len < 0 && t2->array_len < 0 &&
           t1->array_len == t2->array_len;
  }
  return false;
}

static bool is_ppc_enum_value_assignment(Type *lhs, Type *rhs) {
  lhs = canonical_type(lhs);
  rhs = canonical_type(rhs);

  Type *base = ppc_family_base(lhs);
  if (!base || base != ppc_family_base(rhs))
    return false;
  if (!base->is_ppc_enum_generalization)
    return false;

  return is_ppc_zero_specialization(lhs, base) &&
         (rhs == base || rhs->ppc_base == base);
}

static bool is_ppc_named_zero_assignment(Type *lhs, Type *rhs) {
  lhs = canonical_type(lhs);
  rhs = canonical_type(rhs);

  Type *base = ppc_family_base(lhs);
  if (!base || base != ppc_family_base(rhs))
    return false;
  if (!base->ppc_default_spec || base->is_ppc_enum_generalization)
    return false;

  return is_ppc_zero_specialization(lhs, base) &&
         is_ppc_zero_specialization(rhs, base);
}

bool is_assignable(Type *lhs, Type *rhs) {
  return is_compatible(lhs, rhs) || is_ppc_enum_value_assignment(lhs, rhs) ||
         is_ppc_named_zero_assignment(lhs, rhs);
}

Type *copy_type(Type *ty) {
  Type *ret = calloc(1, sizeof(Type));
  *ret = *ty;
  ret->origin = ty;
  return ret;
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  ty->is_unsigned = true;
  return ty;
}

Type *func_type(Type *return_ty) {
  // The C spec disallows sizeof(<function type>), but
  // GCC allows that and the expression is evaluated to 1.
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_ty;
  return ty;
}

Type *array_of(Type *base, int len) {
  Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;
  return ty;
}

Type *vla_of(Type *base, Node *len) {
  Type *ty = new_type(TY_VLA, 8, 8);
  ty->base = base;
  ty->vla_len = len;
  return ty;
}

Type *enum_type(void) {
  return new_type(TY_ENUM, 4, 4);
}

Type *struct_type(void) {
  return new_type(TY_STRUCT, 0, 1);
}

static Type *get_common_type(Type *ty1, Type *ty2) {
  if (ty1->base)
    return pointer_to(ty1->base);

  if (ty1->kind == TY_FUNC)
    return pointer_to(ty1);
  if (ty2->kind == TY_FUNC)
    return pointer_to(ty2);

  if (ty1->kind == TY_LDOUBLE || ty2->kind == TY_LDOUBLE)
    return ty_ldouble;
  if (ty1->kind == TY_DOUBLE || ty2->kind == TY_DOUBLE)
    return ty_double;
  if (ty1->kind == TY_FLOAT || ty2->kind == TY_FLOAT)
    return ty_float;

  if (ty1->size < 4)
    ty1 = ty_int;
  if (ty2->size < 4)
    ty2 = ty_int;

  if (ty1->size != ty2->size)
    return (ty1->size < ty2->size) ? ty2 : ty1;

  if (ty2->is_unsigned)
    return ty2;
  return ty1;
}

// For many binary operators, we implicitly promote operands so that
// both operands have the same type. Any integral type smaller than
// int is always promoted to int. If the type of one operand is larger
// than the other's (e.g. "long" vs. "int"), the smaller operand will
// be promoted to match with the other.
//
// This operation is called the "usual arithmetic conversion".
static void usual_arith_conv(Node **lhs, Node **rhs) {
  if ((*lhs)->ty->kind == TY_NULLPTR || (*rhs)->ty->kind == TY_NULLPTR)
    error_tok((*lhs)->tok, "invalid nullptr_t operands");
  Type *ty = get_common_type((*lhs)->ty, (*rhs)->ty);
  *lhs = new_cast(*lhs, ty);
  *rhs = new_cast(*rhs, ty);
}

// Apply array/function decay when nullptr_t meets a pointer operand.
static Type *decayed_pointer_type(Type *ty) {
  if (ty->kind == TY_PTR)
    return ty;
  if (ty->kind == TY_ARRAY || ty->kind == TY_VLA)
    return pointer_to(ty->base);
  if (ty->kind == TY_FUNC)
    return pointer_to(ty);
  return NULL;
}

static void nullptr_conversion(Node **lhs, Node **rhs, bool equality) {
  Node *other = (*lhs)->ty->kind == TY_NULLPTR ? *rhs : *lhs;
  Type *ty = decayed_pointer_type(other->ty);
  if (!ty) {
    if (other->ty->kind != TY_NULLPTR &&
        !(equality && is_null_pointer_constant(other)))
      error_tok(other->tok, "invalid nullptr_t operands");
    ty = ty_nullptr;
  }
  *lhs = new_cast(*lhs, ty);
  *rhs = new_cast(*rhs, ty);
}

void add_type(Node *node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->inc);

  for (Node *n = node->body; n; n = n->next)
    add_type(n);
  for (Node *n = node->args; n; n = n->next)
    add_type(n);

  switch (node->kind) {
  case ND_NUM:
    node->ty = ty_int;
    return;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
    usual_arith_conv(&node->lhs, &node->rhs);
    node->ty = node->lhs->ty;
    return;
  case ND_NEG: {
    if (node->lhs->ty->kind == TY_NULLPTR)
      error_tok(node->tok, "invalid nullptr_t operand");
    Type *ty = get_common_type(ty_int, node->lhs->ty);
    node->lhs = new_cast(node->lhs, ty);
    node->ty = ty;
    return;
  }
  case ND_ASSIGN:
    if (node->lhs->ty->kind == TY_ARRAY)
      error_tok(node->lhs->tok, "not an lvalue");
    if (is_aggregate(node->lhs->ty) || is_aggregate(node->rhs->ty)) {
      if (!is_assignable(node->lhs->ty, node->rhs->ty))
        error_tok(node->rhs->tok, "incompatible aggregate assignment");
    } else {
      node->rhs = new_cast(node->rhs, node->lhs->ty);
    }
    node->ty = node->lhs->ty;
    return;
  case ND_EQ:
  case ND_NE:
    if (node->lhs->ty->kind == TY_NULLPTR || node->rhs->ty->kind == TY_NULLPTR)
      nullptr_conversion(&node->lhs, &node->rhs, true);
    else
      usual_arith_conv(&node->lhs, &node->rhs);
    node->ty = ty_int;
    return;
  case ND_LT:
  case ND_LE:
    usual_arith_conv(&node->lhs, &node->rhs);
    node->ty = ty_int;
    return;
  case ND_FUNCALL:
    node->ty = node->func_ty->return_ty;
    return;
  case ND_NOT:
  case ND_LOGOR:
  case ND_LOGAND:
    node->ty = ty_int;
    return;
  case ND_BITNOT:
  case ND_SHL:
  case ND_SHR:
    if (node->lhs->ty->kind == TY_NULLPTR ||
        (node->rhs && node->rhs->ty->kind == TY_NULLPTR))
      error_tok(node->tok, "invalid nullptr_t operands");
    node->ty = node->lhs->ty;
    return;
  case ND_VAR:
  case ND_VLA_PTR:
    node->ty = node->var->ty;
    return;
  case ND_COND:
    if (node->then->ty->kind == TY_NULLPTR || node->els->ty->kind == TY_NULLPTR) {
      nullptr_conversion(&node->then, &node->els, false);
      node->ty = node->then->ty;
      return;
    }
    if (node->then->ty->kind == TY_VOID || node->els->ty->kind == TY_VOID) {
      node->ty = ty_void;
    } else if (is_aggregate(node->then->ty) || is_aggregate(node->els->ty)) {
      if (!is_compatible(node->then->ty, node->els->ty))
        error_tok(node->els->tok, "incompatible aggregate operands");
      node->ty = node->then->ty;
    } else {
      usual_arith_conv(&node->then, &node->els);
      node->ty = node->then->ty;
    }
    return;
  case ND_COMMA:
    node->ty = node->rhs->ty;
    return;
  case ND_MEMBER:
    node->ty = node->member->ty;
    return;
  case ND_ADDR: {
    Type *ty = node->lhs->ty;
    if (ty->kind == TY_ARRAY)
      node->ty = pointer_to(ty->base);
    else
      node->ty = pointer_to(ty);
    return;
  }
  case ND_DEREF:
    if (!node->lhs->ty->base)
      error_tok(node->tok, "invalid pointer dereference");
    if (node->lhs->ty->base->kind == TY_VOID)
      error_tok(node->tok, "dereferencing a void pointer");

    node->ty = node->lhs->ty->base;
    return;
  case ND_STMT_EXPR:
    if (node->body) {
      Node *stmt = node->body;
      while (stmt->next)
        stmt = stmt->next;
      if (stmt->kind == ND_EXPR_STMT) {
        node->ty = stmt->lhs->ty;
        return;
      }
    }
    error_tok(node->tok, "statement expression returning void is not supported");
    return;
  case ND_LABEL_VAL:
    node->ty = pointer_to(ty_void);
    return;
  case ND_CAS:
    add_type(node->cas_addr);
    add_type(node->cas_old);
    add_type(node->cas_new);
    node->ty = ty_bool;

    if (node->cas_addr->ty->kind != TY_PTR)
      error_tok(node->cas_addr->tok, "pointer expected");
    if (node->cas_old->ty->kind != TY_PTR)
      error_tok(node->cas_old->tok, "pointer expected");
    return;
  case ND_EXCH:
    if (node->lhs->ty->kind != TY_PTR)
      error_tok(node->cas_addr->tok, "pointer expected");
    node->ty = node->lhs->ty->base;
    return;
  }
}
