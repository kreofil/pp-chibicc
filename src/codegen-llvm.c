#include "chibicc.h"

// An intentionally small, text-only LLVM backend. Keep the frontend's AST and
// the native backend independent of LLVM libraries. Mutable C variables live
// in memory; opt's SROA/mem2reg passes can promote suitable allocations later.
typedef struct LlvmLocal LlvmLocal;
struct LlvmLocal {
  LlvmLocal *next;
  Obj *obj;
  char *addr;
};

typedef struct LlvmRef LlvmRef;
struct LlvmRef {
  LlvmRef *next;
  Obj *obj;
  bool emitted;
};

typedef struct LlvmArgSlot LlvmArgSlot;
struct LlvmArgSlot {
  LlvmArgSlot *next;
  Node *arg;
  char *addr;
};

typedef struct {
  bool memory;
  int count;
  int bits[2];
  int offsets[2];
} LlvmAbi;

static FILE *llvm_out;
static Obj *llvm_prog;
static Obj *llvm_fn;
static LlvmLocal *llvm_locals;
static LlvmRef *llvm_refs;
static int llvm_reg_id;
static int llvm_block_id;
static char *llvm_block;
static bool llvm_terminated;
static bool llvm_memset;
static bool llvm_memmove;
static LlvmArgSlot *llvm_arg_slots;
static int llvm_arg_slot_id;
static LlvmAbi llvm_return_abi;

static char *llvm_expr(Node *node);
static void llvm_stmt(Node *node);

static void llvm_print(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(llvm_out, fmt, ap);
  va_end(ap);
  fputc('\n', llvm_out);
}

static void llvm_unsupported(Token *tok, char *what) {
  if (tok)
    error_tok(tok, "LLVM backend: %s is not supported yet", what);
  error("LLVM backend: %s is not supported yet", what);
}

static bool llvm_pointer(Type *ty) {
  return ty->kind == TY_PTR || ty->kind == TY_NULLPTR ||
         ty->kind == TY_ARRAY || ty->kind == TY_FUNC;
}

static bool llvm_aggregate(Type *ty) {
  return ty->kind == TY_STRUCT || ty->kind == TY_UNION;
}

static void llvm_check_scalar(Type *ty, Token *tok) {
  if (ty->is_atomic)
    llvm_unsupported(tok, "atomic types");
  if (ty->is_volatile)
    llvm_unsupported(tok, "volatile types");
  if (is_integer(ty) || llvm_pointer(ty) || ty->kind == TY_VOID)
    return;
  llvm_unsupported(tok, "this value type (aggregate, floating point or VLA)");
}

static void llvm_check_object(Type *ty, Token *tok) {
  if (ty->is_atomic)
    llvm_unsupported(tok, "atomic types");
  if (ty->is_volatile)
    llvm_unsupported(tok, "volatile types");
  if (ty->kind == TY_ARRAY) {
    llvm_check_object(ty->base, tok);
    return;
  }
  if (llvm_aggregate(ty)) {
    for (Member *mem = ty->members; mem; mem = mem->next) {
      if (mem->is_bitfield)
        llvm_unsupported(tok, "bitfields");
      llvm_check_object(mem->ty, tok);
    }
    return;
  }
  llvm_check_scalar(ty, tok);
  if (ty->kind == TY_VOID || ty->kind == TY_FUNC)
    llvm_unsupported(tok, "this object type");
}

static char *llvm_type(Type *ty) {
  if (llvm_pointer(ty))
    return "ptr";
  if (ty->kind == TY_VOID)
    return "void";
  return format("i%d", ty->size * 8);
}

static char *llvm_storage_type(Type *ty) {
  // Use the frontend's exact size and alignment, including record padding
  // and PPC tags. Aggregate expression values are addresses of this storage.
  if (llvm_aggregate(ty))
    return format("[%d x i8]", ty->size);
  if (ty->kind == TY_ARRAY)
    return format("[%d x %s]", ty->array_len, llvm_storage_type(ty->base));
  return llvm_type(ty);
}

// The narrow integer attributes matter when exchanging objects with a system
// C compiler: LLVM must extend char/short arguments and results as SysV expects.
static char *llvm_abi_attr(Type *ty) {
  if (is_integer(ty) && ty->size < 4)
    return ty->is_unsigned || ty->kind == TY_BOOL ? "zeroext " : "signext ";
  return "";
}

// The supported records contain only INTEGER-class fields. Classify actual
// fields, not padding: an aligned record may have a wholly empty eightbyte.
// Unaligned fields and records larger than two eightbytes use memory.
static bool llvm_abi_fields(Type *ty, int offset, int *ends) {
  if (!ty->size)
    return true;
  if (offset % MAX(1, ty->align))
    return false;
  if (llvm_aggregate(ty)) {
    for (Member *mem = ty->members; mem; mem = mem->next)
      if (!llvm_abi_fields(mem->ty, offset + mem->offset, ends))
        return false;
  } else if (ty->kind == TY_ARRAY) {
    for (int i = 0; i < ty->array_len; i++)
      if (!llvm_abi_fields(ty->base, offset + i * ty->base->size, ends))
        return false;
  } else {
    for (int i = offset; i < offset + ty->size; i++) {
      if (i < 0 || i >= 16)
        return false;
      ends[i / 8] = MAX(ends[i / 8], i % 8 + 1);
    }
  }
  return true;
}

static void llvm_record_abi(Type *ty, LlvmAbi *abi) {
  memset(abi, 0, sizeof(*abi));
  int ends[2] = {};
  if (ty->size > 16 || !llvm_abi_fields(ty, 0, ends)) {
    abi->memory = true;
    return;
  }
  for (int i = 0; i < 2; i++)
    if (ends[i]) {
      abi->offsets[abi->count] = i * 8;
      abi->bits[abi->count++] = ends[i] * 8;
    }
}

static void llvm_parameter_abi(Type *ty, int *gp, LlvmAbi *abi) {
  llvm_record_abi(ty, abi);
  if (!abi->memory && abi->count > 6 - *gp)
    abi->memory = true;
  if (!abi->memory)
    *gp += abi->count;
}

static char *llvm_return_type(Type *ty) {
  if (!llvm_aggregate(ty))
    return llvm_type(ty);
  LlvmAbi abi;
  llvm_record_abi(ty, &abi);
  if (abi.memory || !abi.count)
    return "void";
  if (abi.count == 1)
    return format("i%d", abi.bits[0]);
  return format("{ i%d, i%d }", abi.bits[0], abi.bits[1]);
}

static char *llvm_list_append(char *list, char *item) {
  return format("%s%s%s", list, *list ? ", " : "", item);
}

// Produce the same lowering for declarations, definitions and variadic call
// prototypes. byval must not consume a GP register; a hidden sret pointer does.
static char *llvm_parameters(Type *ty, bool names, bool attributes) {
  char *list = "";
  int gp = 0;
  if (llvm_aggregate(ty->return_ty)) {
    LlvmAbi abi;
    llvm_record_abi(ty->return_ty, &abi);
    if (abi.memory) {
      list = format("ptr%s%s", attributes ?
                    format(" sret(%s) align %d", llvm_storage_type(ty->return_ty),
                           MAX(1, ty->return_ty->align)) : "",
                    names ? " %sret" : "");
      gp = 1;
    }
  }
  int index = 0;
  for (Type *param = ty->params; param; param = param->next, index++) {
    if (llvm_aggregate(param)) {
      LlvmAbi abi;
      llvm_parameter_abi(param, &gp, &abi);
      if (abi.memory) {
        list = llvm_list_append(list, format("ptr%s%s", attributes ?
          format(" byval(%s) align %d", llvm_storage_type(param), MAX(8, param->align)) : "",
          names ? format(" %%a%d", index) : ""));
      } else {
        for (int i = 0; i < abi.count; i++)
          list = llvm_list_append(list, format("i%d%s", abi.bits[i],
            names ? format(" %%a%d_%d", index, i) : ""));
      }
    } else {
      list = llvm_list_append(list, format("%s %s%s", llvm_type(param),
        attributes ? llvm_abi_attr(param) : "",
        names ? format("%%a%d", index) : ""));
      gp = MIN(6, gp + 1);
    }
  }
  if (ty->is_variadic)
    list = llvm_list_append(list, "...");
  return list;
}

static char *llvm_global_name(char *name) {
  char *buf = calloc(1, strlen(name) * 3 + 4);
  char *p = buf;
  *p++ = '@';
  *p++ = '"';
  for (unsigned char *s = (unsigned char *)name; *s; s++) {
    if (*s < 32 || *s >= 127 || *s == '"' || *s == '\\') {
      sprintf(p, "\\%02X", *s);
      p += 3;
    } else {
      *p++ = *s;
    }
  }
  *p = '"';
  return buf;
}

static char *llvm_reg(void) {
  return format("%%r%d", ++llvm_reg_id);
}

static char *llvm_label(void) {
  return format("bb%d", ++llvm_block_id);
}

static void llvm_branch(char *label) {
  if (!llvm_terminated) {
    llvm_print("  br label %%\"%s\"", label);
    llvm_terminated = true;
  }
}

static void llvm_begin(char *label) {
  llvm_branch(label);
  llvm_print("\"%s\":", label);
  llvm_block = label;
  llvm_terminated = false;
}

// The parser retains statements after return/break. Give such statements an
// unreachable block rather than emitting instructions after a terminator.
static void llvm_ensure_block(void) {
  if (llvm_terminated)
    llvm_begin(llvm_label());
}

static void llvm_reference(Obj *obj) {
  // C permits repeated extern declarations and a declaration after a
  // definition. These may be distinct Obj entries but denote one IR symbol.
  // Prefer its definition so it cannot also acquire an external declaration.
  Obj *canonical = obj;
  for (Obj *other = llvm_prog; other; other = other->next)
    if (!strcmp(other->name, obj->name)) {
      canonical = other;
      if (other->is_definition)
        break;
    }
  obj = canonical;
  for (LlvmRef *ref = llvm_refs; ref; ref = ref->next)
    if (!strcmp(ref->obj->name, obj->name))
      return;
  LlvmRef *ref = calloc(1, sizeof(LlvmRef));
  ref->obj = obj;
  ref->next = llvm_refs;
  llvm_refs = ref;
}

static Obj *llvm_find_global(char *name) {
  for (Obj *obj = llvm_prog; obj; obj = obj->next)
    if (!strcmp(obj->name, name))
      return obj;
  return NULL;
}

static char *llvm_addr_var(Obj *obj) {
  if (!obj->is_local) {
    llvm_reference(obj);
    return llvm_global_name(obj->name);
  }
  for (LlvmLocal *local = llvm_locals; local; local = local->next)
    if (local->obj == obj)
      return local->addr;
  error("LLVM backend: local variable has no allocation");
}

static char *llvm_offset(char *addr, int offset) {
  if (!offset)
    return addr;
  char *result = llvm_reg();
  llvm_print("  %s = getelementptr i8, ptr %s, i64 %d", result, addr, offset);
  return result;
}

static void llvm_copy(char *dest, char *source, Type *ty) {
  llvm_memmove = true;
  llvm_print("  call void @llvm.memmove.p0.p0.i64(ptr %s, ptr %s, i64 %d, i1 false)",
             dest, source, ty->size);
}

static char *llvm_load_piece(char *addr, LlvmAbi *abi, int index) {
  addr = llvm_offset(addr, abi->offsets[index]);
  char *value = llvm_reg();
  llvm_print("  %s = load i%d, ptr %s, align 1", value, abi->bits[index], addr);
  return value;
}

static void llvm_store_piece(char *addr, LlvmAbi *abi, int index, char *value) {
  addr = llvm_offset(addr, abi->offsets[index]);
  llvm_print("  store i%d %s, ptr %s, align 1", abi->bits[index], value, addr);
}

// Snapshot aggregate arguments into reusable entry-block slots. An alloca at
// a call inside a loop would otherwise grow the stack on every iteration.
static void llvm_argument_slots(Node *node) {
  if (!node)
    return;
  if (node->kind == ND_FUNCALL) {
    for (Node *arg = node->args; arg; arg = arg->next) {
      if (!llvm_aggregate(arg->ty))
        continue;
      bool found = false;
      for (LlvmArgSlot *slot = llvm_arg_slots; slot; slot = slot->next)
        if (slot->arg == arg)
          found = true;
      if (found)
        continue;
      llvm_check_object(arg->ty, arg->tok);
      LlvmArgSlot *slot = calloc(1, sizeof(LlvmArgSlot));
      slot->arg = arg;
      slot->addr = format("%%arg%d", ++llvm_arg_slot_id);
      slot->next = llvm_arg_slots;
      llvm_arg_slots = slot;
      llvm_print("  %s = alloca %s, align %d", slot->addr,
                 llvm_storage_type(arg->ty), MAX(8, arg->ty->align));
    }
  }
  llvm_argument_slots(node->lhs);
  llvm_argument_slots(node->rhs);
  llvm_argument_slots(node->cond);
  llvm_argument_slots(node->then);
  llvm_argument_slots(node->els);
  llvm_argument_slots(node->init);
  llvm_argument_slots(node->inc);
  for (Node *child = node->body; child; child = child->next)
    llvm_argument_slots(child);
  for (Node *arg = node->args; arg; arg = arg->next)
    llvm_argument_slots(arg);
}

static char *llvm_argument_slot(Node *arg) {
  for (LlvmArgSlot *slot = llvm_arg_slots; slot; slot = slot->next)
    if (slot->arg == arg)
      return slot->addr;
  error("LLVM backend: aggregate argument has no allocation");
}

static char *llvm_bool(char *value, Type *ty) {
  llvm_check_scalar(ty, NULL);
  if (ty->kind == TY_VOID)
    llvm_unsupported(NULL, "void condition");
  char *result = llvm_reg();
  llvm_print("  %s = icmp ne %s %s, %s", result, llvm_type(ty), value,
             llvm_pointer(ty) ? "null" : "0");
  return result;
}

static char *llvm_cast(char *value, Type *from, Type *to, Token *tok) {
  if (to->kind == TY_VOID && llvm_aggregate(from)) {
    llvm_check_object(from, tok);
    return "0";
  }
  llvm_check_scalar(from, tok);
  llvm_check_scalar(to, tok);
  if (to->kind == TY_VOID)
    return "0";
  if (from->kind == TY_VOID)
    llvm_unsupported(tok, "conversion from void");
  if (to->kind == TY_BOOL) {
    char *bit = llvm_bool(value, from);
    char *result = llvm_reg();
    llvm_print("  %s = zext i1 %s to i8", result, bit);
    return result;
  }
  if (llvm_pointer(from) && llvm_pointer(to))
    return value;
  if (!llvm_pointer(from) && !llvm_pointer(to) && from->size == to->size)
    return value;

  // inttoptr itself zero-extends narrow inputs. Match the native backend's
  // signed conversion to the full pointer width before forming the pointer.
  if (!llvm_pointer(from) && llvm_pointer(to) && from->size < 8) {
    value = llvm_cast(value, from, ty_long, tok);
    from = ty_long;
  }

  char *result = llvm_reg();
  char *op;
  if (llvm_pointer(from))
    op = "ptrtoint";
  else if (llvm_pointer(to))
    op = "inttoptr";
  else if (from->size > to->size)
    op = "trunc";
  else
    op = from->is_unsigned || from->kind == TY_BOOL ? "zext" : "sext";
  llvm_print("  %s = %s %s %s to %s", result, op, llvm_type(from), value,
             llvm_type(to));
  return result;
}

static char *llvm_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    return llvm_addr_var(node->var);
  case ND_DEREF:
    return llvm_expr(node->lhs);
  case ND_COMMA:
    llvm_expr(node->lhs);
    return llvm_addr(node->rhs);
  case ND_MEMBER:
    if (node->member->is_bitfield)
      llvm_unsupported(node->tok, "bitfields");
    char *base = llvm_expr(node->lhs);
    char *addr = llvm_reg();
    llvm_print("  %s = getelementptr i8, ptr %s, i64 %d", addr, base,
               node->member->offset);
    return addr;
  }
  llvm_unsupported(node->tok, "this lvalue");
  return NULL;
}

static char *llvm_load(char *addr, Type *ty, Token *tok) {
  if (llvm_aggregate(ty)) {
    llvm_check_object(ty, tok);
    return addr;
  }
  llvm_check_scalar(ty, tok);
  if (ty->kind == TY_ARRAY || ty->kind == TY_FUNC)
    return addr;
  char *result = llvm_reg();
  llvm_print("  %s = load %s, ptr %s, align 1", result, llvm_type(ty), addr);
  return result;
}

static char *llvm_logical(Node *node) {
  char *left = llvm_bool(llvm_expr(node->lhs), node->lhs->ty);
  char *left_block = llvm_block;
  char *right_block = llvm_label();
  char *end = llvm_label();
  bool is_and = node->kind == ND_LOGAND;
  llvm_print("  br i1 %s, label %%\"%s\", label %%\"%s\"", left,
             is_and ? right_block : end, is_and ? end : right_block);
  llvm_terminated = true;
  llvm_begin(right_block);
  char *right = llvm_bool(llvm_expr(node->rhs), node->rhs->ty);
  char *right_end = llvm_block;
  llvm_branch(end);
  llvm_begin(end);
  char *bit = llvm_reg();
  llvm_print("  %s = phi i1 [%d, %%\"%s\"], [%s, %%\"%s\"]", bit,
             is_and ? 0 : 1, left_block, right, right_end);
  char *result = llvm_reg();
  llvm_print("  %s = zext i1 %s to i32", result, bit);
  return result;
}

static char *llvm_conditional(Node *node) {
  if (llvm_aggregate(node->ty))
    llvm_check_object(node->ty, node->tok);
  else
    llvm_check_scalar(node->ty, node->tok);
  char *cond = llvm_bool(llvm_expr(node->cond), node->cond->ty);
  char *then_block = llvm_label();
  char *else_block = llvm_label();
  char *end = llvm_label();
  llvm_print("  br i1 %s, label %%\"%s\", label %%\"%s\"", cond,
             then_block, else_block);
  llvm_terminated = true;
  llvm_begin(then_block);
  char *then_value = llvm_expr(node->then);
  char *then_end = llvm_block;
  llvm_branch(end);
  llvm_begin(else_block);
  char *else_value = llvm_expr(node->els);
  char *else_end = llvm_block;
  llvm_branch(end);
  llvm_begin(end);
  if (node->ty->kind == TY_VOID)
    return "0";
  char *result = llvm_reg();
  llvm_print("  %s = phi %s [%s, %%\"%s\"], [%s, %%\"%s\"]", result,
             llvm_aggregate(node->ty) ? "ptr" : llvm_type(node->ty),
             then_value, then_end, else_value, else_end);
  return result;
}

static void llvm_check_signature(Type *ty, Token *tok, bool definition) {
  if (ty->is_variadic && (definition || !ty->params))
    llvm_unsupported(tok, "variadic definitions or unprototyped functions");
  if (llvm_aggregate(ty->return_ty))
    llvm_check_object(ty->return_ty, tok);
  else
    llvm_check_scalar(ty->return_ty, tok);
  if (llvm_aggregate(ty->return_ty) && ty->return_ty->size < 0)
    llvm_unsupported(tok, "incomplete aggregate ABI types");
  for (Type *param = ty->params; param; param = param->next) {
    if (llvm_aggregate(param))
      llvm_check_object(param, tok);
    else
      llvm_check_scalar(param, tok);
    if (llvm_aggregate(param) && param->size < 0)
      llvm_unsupported(tok, "incomplete aggregate ABI types");
    if (param->kind == TY_VOID || param->kind == TY_ARRAY || param->kind == TY_FUNC)
      llvm_unsupported(tok, "this parameter type");
  }
}

static char *llvm_call(Node *node) {
  llvm_check_signature(node->func_ty, node->tok, false);
  if (node->lhs->kind == ND_VAR && !strcmp(node->lhs->var->name, "alloca"))
    llvm_unsupported(node->tok, "alloca");
  char *callee = llvm_expr(node->lhs);
  char *args = "";
  Type *ret = node->func_ty->return_ty;
  LlvmAbi ret_abi = {};
  char *ret_addr = NULL;
  int gp = 0;
  if (llvm_aggregate(ret)) {
    llvm_record_abi(ret, &ret_abi);
    ret_addr = llvm_addr_var(node->ret_buffer);
    if (ret_abi.memory) {
      args = format("ptr sret(%s) align %d %s", llvm_storage_type(ret),
                    MAX(1, ret->align), ret_addr);
      gp = 1;
    }
  }
  Type *param = node->func_ty->params;
  for (Node *arg = node->args; arg; arg = arg->next) {
    if (!param && !node->func_ty->is_variadic)
      llvm_unsupported(node->tok, "arguments without a fixed prototype");
    Type *target = param ? param : arg->ty;
    if (llvm_aggregate(target)) {
      if (!param)
        llvm_unsupported(arg->tok, "aggregate variadic arguments");
      if (!llvm_aggregate(arg->ty) || !is_assignable(target, arg->ty))
        error_tok(arg->tok, "LLVM backend: incompatible aggregate argument");
      LlvmAbi abi;
      llvm_parameter_abi(target, &gp, &abi);
      char *source = llvm_expr(arg);
      char *slot = llvm_argument_slot(arg);
      llvm_copy(slot, source, target);
      if (abi.memory) {
        args = llvm_list_append(args, format("ptr byval(%s) align %d %s",
          llvm_storage_type(target), MAX(8, target->align), slot));
      } else {
        for (int i = 0; i < abi.count; i++)
          args = llvm_list_append(args, format("i%d %s", abi.bits[i],
            llvm_load_piece(slot, &abi, i)));
      }
    } else {
      if (!param && is_integer(target) && target->size < ty_int->size)
        target = ty_int;
      if (target->kind == TY_VOID)
        llvm_unsupported(arg->tok, "void arguments");
      char *value = llvm_cast(llvm_expr(arg), arg->ty, target, arg->tok);
      args = llvm_list_append(args, format("%s %s%s", llvm_type(target),
        param ? llvm_abi_attr(target) : "", value));
      gp = MIN(6, gp + 1);
    }
    if (param)
      param = param->next;
  }
  if (param)
    llvm_unsupported(node->tok, "a call with missing arguments");
  char *ret_type = llvm_return_type(ret);
  char *result = !strcmp(ret_type, "void") ? NULL : llvm_reg();
  char *prototype = "";
  if (node->func_ty->is_variadic)
    prototype = format(" (%s)", llvm_parameters(node->func_ty, false, false));
  llvm_print("  %scall %s%s%s %s(%s)", result ? format("%s = ", result) : "",
             llvm_aggregate(ret) ? "" : llvm_abi_attr(ret), ret_type, prototype, callee, args);
  if (ret_addr) {
    if (!ret_abi.memory)
      for (int i = 0; i < ret_abi.count; i++) {
        char *piece = result;
        if (ret_abi.count > 1) {
          piece = llvm_reg();
          llvm_print("  %s = extractvalue %s %s, %d", piece, ret_type, result, i);
        }
        llvm_store_piece(ret_addr, &ret_abi, i, piece);
      }
    return ret_addr;
  }
  return result ? result : "0";
}

static char *llvm_binary(Node *node) {
  char *left = llvm_expr(node->lhs);
  char *right = llvm_expr(node->rhs);
  Type *ty = node->lhs->ty;
  llvm_check_scalar(ty, node->tok);
  llvm_check_scalar(node->rhs->ty, node->tok);

  // The chibicc AST has already scaled pointer offsets, and its usual
  // conversions may even cast the scaled integer to a pointer. Express the
  // existing byte arithmetic explicitly without claiming LLVM inbounds.
  bool pointer_arith = (node->kind == ND_ADD || node->kind == ND_SUB) &&
                       (llvm_pointer(ty) || llvm_pointer(node->rhs->ty));
  if (pointer_arith) {
    left = llvm_cast(left, ty, ty_ulong, node->tok);
    right = llvm_cast(right, node->rhs->ty, ty_ulong, node->tok);
    char *result = llvm_reg();
    llvm_print("  %s = %s i64 %s, %s", result,
               node->kind == ND_ADD ? "add" : "sub", left, right);
    return llvm_cast(result, ty_ulong, node->ty, node->tok);
  }

  char *op = NULL;
  bool comparison = false;
  bool unsig = llvm_pointer(ty) || ty->is_unsigned;
  switch (node->kind) {
  case ND_ADD: op = "add"; break;
  case ND_SUB: op = "sub"; break;
  case ND_MUL: op = "mul"; break;
  case ND_DIV: op = unsig ? "udiv" : "sdiv"; break;
  case ND_MOD: op = unsig ? "urem" : "srem"; break;
  case ND_BITAND: op = "and"; break;
  case ND_BITOR: op = "or"; break;
  case ND_BITXOR: op = "xor"; break;
  case ND_SHL: op = "shl"; break;
  case ND_SHR: op = unsig ? "lshr" : "ashr"; break;
  case ND_EQ: op = "icmp eq"; comparison = true; break;
  case ND_NE: op = "icmp ne"; comparison = true; break;
  case ND_LT: op = unsig ? "icmp ult" : "icmp slt"; comparison = true; break;
  case ND_LE: op = unsig ? "icmp ule" : "icmp sle"; comparison = true; break;
  default:
    llvm_unsupported(node->tok, "this binary operator");
  }
  if (llvm_pointer(ty) && !comparison)
    llvm_unsupported(node->tok, "this pointer operator");
  right = llvm_cast(right, node->rhs->ty, ty, node->tok);
  char *result = llvm_reg();
  llvm_print("  %s = %s %s %s, %s", result, op, llvm_type(ty), left, right);
  if (!comparison)
    return result;
  char *integer = llvm_reg();
  llvm_print("  %s = zext i1 %s to i32", integer, result);
  return integer;
}

static char *llvm_expr(Node *node) {
  llvm_ensure_block();
  switch (node->kind) {
  case ND_NULL_EXPR:
    return "0";
  case ND_NUM:
    llvm_check_scalar(node->ty, node->tok);
    if (node->ty->kind == TY_NULLPTR)
      return "null";
    if (!is_integer(node->ty))
      llvm_unsupported(node->tok, "noninteger literal");
    return format("%ld", (long)node->val);
  case ND_VAR:
    if (node->var->is_tls)
      llvm_unsupported(node->tok, "thread-local storage");
    return llvm_load(llvm_addr_var(node->var), node->ty, node->tok);
  case ND_ADDR:
    return llvm_addr(node->lhs);
  case ND_DEREF:
    return llvm_load(llvm_expr(node->lhs), node->ty, node->tok);
  case ND_ASSIGN: {
    char *addr = llvm_addr(node->lhs);
    if (llvm_aggregate(node->ty)) {
      llvm_check_object(node->ty, node->tok);
      char *source = llvm_expr(node->rhs);
      llvm_copy(addr, source, node->ty);
      return addr;
    }
    llvm_check_scalar(node->ty, node->tok);
    char *value = llvm_cast(llvm_expr(node->rhs), node->rhs->ty, node->ty, node->tok);
    llvm_print("  store %s %s, ptr %s, align 1", llvm_type(node->ty), value, addr);
    return value;
  }
  case ND_CAST:
    return llvm_cast(llvm_expr(node->lhs), node->lhs->ty, node->ty, node->tok);
  case ND_COMMA:
    llvm_expr(node->lhs);
    return llvm_expr(node->rhs);
  case ND_NEG:
  case ND_BITNOT: {
    llvm_check_scalar(node->ty, node->tok);
    char *value = llvm_expr(node->lhs);
    char *result = llvm_reg();
    if (node->kind == ND_NEG)
      llvm_print("  %s = sub %s 0, %s", result, llvm_type(node->ty), value);
    else
      llvm_print("  %s = xor %s %s, -1", result, llvm_type(node->ty), value);
    return result;
  }
  case ND_NOT: {
    char *bit = llvm_bool(llvm_expr(node->lhs), node->lhs->ty);
    char *negated = llvm_reg();
    llvm_print("  %s = xor i1 %s, true", negated, bit);
    char *result = llvm_reg();
    llvm_print("  %s = zext i1 %s to i32", result, negated);
    return result;
  }
  case ND_LOGAND:
  case ND_LOGOR:
    return llvm_logical(node);
  case ND_COND:
    return llvm_conditional(node);
  case ND_FUNCALL:
    return llvm_call(node);
  case ND_MEMZERO:
    llvm_memset = true;
    llvm_print("  call void @llvm.memset.p0.i64(ptr %s, i8 0, i64 %d, i1 false)",
               llvm_addr_var(node->var), node->var->ty->size);
    return "0";
  case ND_STMT_EXPR: {
    char *value = "0";
    for (Node *stmt = node->body; stmt; stmt = stmt->next) {
      if (!stmt->next && stmt->kind == ND_EXPR_STMT)
        value = llvm_expr(stmt->lhs);
      else
        llvm_stmt(stmt);
    }
    return value;
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
    return llvm_binary(node);
  case ND_MEMBER:
    return llvm_load(llvm_addr(node), node->ty, node->tok);
  case ND_CAS:
  case ND_EXCH:
    llvm_unsupported(node->tok, "atomic operations");
  case ND_VLA_PTR:
    llvm_unsupported(node->tok, "variable-length arrays");
  case ND_LABEL_VAL:
    llvm_unsupported(node->tok, "labels as values");
  default:
    llvm_unsupported(node->tok, "this expression");
  }
  return NULL;
}

static void llvm_stmt(Node *node) {
  if (node->kind != ND_BLOCK && node->kind != ND_LABEL)
    llvm_ensure_block();
  switch (node->kind) {
  case ND_BLOCK:
    for (Node *stmt = node->body; stmt; stmt = stmt->next)
      llvm_stmt(stmt);
    return;
  case ND_EXPR_STMT:
    llvm_expr(node->lhs);
    return;
  case ND_RETURN: {
    Type *ty = llvm_fn->ty->return_ty;
    if (llvm_aggregate(ty)) {
      if (!node->lhs || !llvm_aggregate(node->lhs->ty) ||
          !is_assignable(ty, node->lhs->ty))
        error_tok(node->tok, "LLVM backend: incompatible aggregate return value");
      char *source = llvm_expr(node->lhs);
      if (llvm_return_abi.memory) {
        llvm_copy("%sret", source, ty);
        llvm_print("  ret void");
      } else if (!llvm_return_abi.count) {
        llvm_print("  ret void");
      } else {
        char *result = NULL;
        char *ret_type = llvm_return_type(ty);
        for (int i = 0; i < llvm_return_abi.count; i++) {
          char *piece = llvm_load_piece(source, &llvm_return_abi, i);
          if (llvm_return_abi.count == 1) {
            result = piece;
          } else {
            char *inserted = llvm_reg();
            llvm_print("  %s = insertvalue %s %s, i%d %s, %d", inserted,
                       ret_type, result ? result : "poison",
                       llvm_return_abi.bits[i], piece, i);
            result = inserted;
          }
        }
        llvm_print("  ret %s %s", ret_type, result);
      }
      llvm_terminated = true;
      return;
    }
    char *value = "0";
    if (node->lhs)
      value = llvm_cast(llvm_expr(node->lhs), node->lhs->ty, ty, node->tok);
    if (ty->kind == TY_VOID)
      llvm_print("  ret void");
    else
      llvm_print("  ret %s %s", llvm_type(ty), node->lhs ? value :
                 (llvm_pointer(ty) ? "null" : "0"));
    llvm_terminated = true;
    return;
  }
  case ND_IF: {
    char *cond = llvm_bool(llvm_expr(node->cond), node->cond->ty);
    char *then_block = llvm_label();
    char *else_block = llvm_label();
    char *end = llvm_label();
    llvm_print("  br i1 %s, label %%\"%s\", label %%\"%s\"", cond,
               then_block, else_block);
    llvm_terminated = true;
    llvm_begin(then_block);
    llvm_stmt(node->then);
    llvm_branch(end);
    llvm_begin(else_block);
    if (node->els)
      llvm_stmt(node->els);
    llvm_branch(end);
    llvm_begin(end);
    return;
  }
  case ND_FOR: {
    if (node->init)
      llvm_stmt(node->init);
    char *test = llvm_label();
    char *body = llvm_label();
    llvm_begin(test);
    if (node->cond) {
      char *cond = llvm_bool(llvm_expr(node->cond), node->cond->ty);
      llvm_print("  br i1 %s, label %%\"%s\", label %%\"%s\"", cond,
                 body, node->brk_label);
      llvm_terminated = true;
    }
    llvm_begin(body);
    llvm_stmt(node->then);
    llvm_begin(node->cont_label);
    if (node->inc)
      llvm_expr(node->inc);
    llvm_branch(test);
    llvm_begin(node->brk_label);
    return;
  }
  case ND_DO: {
    char *body = llvm_label();
    llvm_begin(body);
    llvm_stmt(node->then);
    llvm_begin(node->cont_label);
    char *cond = llvm_bool(llvm_expr(node->cond), node->cond->ty);
    llvm_print("  br i1 %s, label %%\"%s\", label %%\"%s\"", cond,
               body, node->brk_label);
    llvm_terminated = true;
    llvm_begin(node->brk_label);
    return;
  }
  case ND_GOTO:
    llvm_branch(node->unique_label);
    return;
  case ND_LABEL:
    llvm_begin(node->unique_label);
    llvm_stmt(node->lhs);
    return;
  case ND_ASM:
    llvm_unsupported(node->tok, "inline assembly");
  case ND_GOTO_EXPR:
    llvm_unsupported(node->tok, "computed goto");
  case ND_SWITCH:
  case ND_CASE:
    llvm_unsupported(node->tok, "switch statements");
  default:
    llvm_unsupported(node->tok, "this statement");
  }
}

static void llvm_bytes(char *data, int offset, int len) {
  fprintf(llvm_out, "c\"");
  for (int i = 0; i < len; i++)
    fprintf(llvm_out, "\\%02X", data ? (unsigned char)data[offset + i] : 0);
  fprintf(llvm_out, "\"");
}

// Use exact byte storage for data objects. A packed collection of byte runs
// and pointer constants also preserves the frontend's relocation offsets.
// Opaque pointers allow scalar/array lvalues to access this storage normally.
static void llvm_global(Obj *obj) {
  if (obj->is_tls)
    llvm_unsupported(obj->tok, "thread-local storage");
  llvm_check_object(obj->ty, obj->tok);
  if (obj->ty->size < 0)
    llvm_unsupported(obj->tok, "incomplete object definitions");
  int size = obj->ty->size;
  char *name = llvm_global_name(obj->name);
  char *linkage = obj->is_static ? "internal " :
                  (opt_fcommon && obj->is_tentative ? "common " : "");
  fprintf(llvm_out, "%s = %sglobal ", name, linkage);
  if (!obj->rel) {
    fprintf(llvm_out, "[%d x i8] ", size);
    if (obj->init_data)
      llvm_bytes(obj->init_data, 0, size);
    else
      fprintf(llvm_out, "zeroinitializer");
  } else {
    int pos = 0;
    fprintf(llvm_out, "<{ ");
    bool first = true;
    for (Relocation *rel = obj->rel; rel; rel = rel->next) {
      if (rel->offset < pos || rel->offset + 8 > size)
        llvm_unsupported(obj->tok, "this data relocation");
      if (pos != rel->offset) {
        fprintf(llvm_out, "%s[%d x i8]", first ? "" : ", ", rel->offset - pos);
        first = false;
      }
      fprintf(llvm_out, "%sptr", first ? "" : ", ");
      first = false;
      pos = rel->offset + 8;
    }
    if (pos != size)
      fprintf(llvm_out, ", [%d x i8]", size - pos);
    fprintf(llvm_out, " }> <{ ");
    pos = 0;
    first = true;
    for (Relocation *rel = obj->rel; rel; rel = rel->next) {
      if (pos != rel->offset) {
        fprintf(llvm_out, "%s[%d x i8] ", first ? "" : ", ", rel->offset - pos);
        llvm_bytes(obj->init_data, pos, rel->offset - pos);
        first = false;
      }
      Obj *target = llvm_find_global(*rel->label);
      if (!target)
        llvm_unsupported(obj->tok, "a relocation to an unknown symbol");
      llvm_reference(target);
      fprintf(llvm_out, "%sptr getelementptr (i8, ptr %s, i64 %ld)",
              first ? "" : ", ", llvm_global_name(target->name), rel->addend);
      first = false;
      pos = rel->offset + 8;
    }
    if (pos != size) {
      fprintf(llvm_out, ", [%d x i8] ", size - pos);
      llvm_bytes(obj->init_data, pos, size - pos);
    }
    fprintf(llvm_out, " }>");
  }
  llvm_print(", align %d", MAX(1, obj->align));
}

static void llvm_function(Obj *fn) {
  llvm_check_signature(fn->ty, fn->tok, true);
  llvm_reference(fn);
  for (LlvmRef *ref = llvm_refs; ref; ref = ref->next)
    if (!strcmp(ref->obj->name, fn->name))
      ref->emitted = true;
  llvm_fn = fn;
  llvm_locals = NULL;
  llvm_arg_slots = NULL;
  llvm_arg_slot_id = 0;
  memset(&llvm_return_abi, 0, sizeof(llvm_return_abi));
  if (llvm_aggregate(fn->ty->return_ty))
    llvm_record_abi(fn->ty->return_ty, &llvm_return_abi);
  llvm_reg_id = 0;
  llvm_block_id = 0;
  llvm_block = "entry";
  llvm_terminated = false;

  llvm_print("\ndefine %s%s%s %s(%s) {", fn->is_static ? "internal " : "",
             llvm_abi_attr(fn->ty->return_ty), llvm_return_type(fn->ty->return_ty),
             llvm_global_name(fn->name), llvm_parameters(fn->ty, true, true));
  llvm_print("\"entry\":");
  int local_id = 0;
  for (Obj *obj = fn->locals; obj; obj = obj->next) {
    llvm_check_object(obj->ty, obj->tok ? obj->tok : fn->tok);
    if (obj->ty->size < 0)
      llvm_unsupported(fn->tok, "incomplete local objects");
    LlvmLocal *local = calloc(1, sizeof(LlvmLocal));
    local->obj = obj;
    local->addr = format("%%l%d", ++local_id);
    local->next = llvm_locals;
    llvm_locals = local;
    llvm_print("  %s = alloca %s, align %d", local->addr,
               llvm_storage_type(obj->ty), MAX(1, obj->align));
  }
  llvm_argument_slots(fn->body);
  Obj *param = fn->params;
  if (fn->ret_param) {
    if (!llvm_return_abi.memory || param != fn->ret_param)
      error("LLVM backend: inconsistent hidden return parameter");
    llvm_print("  store ptr %%sret, ptr %s, align 1", llvm_addr_var(param));
    param = param->next;
  }
  int gp = llvm_return_abi.memory ? 1 : 0;
  int index = 0;
  for (Type *ty = fn->ty->params; ty; ty = ty->next, index++) {
    if (llvm_aggregate(ty)) {
      LlvmAbi abi;
      llvm_parameter_abi(ty, &gp, &abi);
      if (param) {
        char *addr = llvm_addr_var(param);
        if (abi.memory)
          llvm_copy(addr, format("%%a%d", index), ty);
        else
          for (int i = 0; i < abi.count; i++)
            llvm_store_piece(addr, &abi, i, format("%%a%d_%d", index, i));
      }
    } else {
      if (param)
        llvm_print("  store %s %%a%d, ptr %s, align 1", llvm_type(ty),
                   index, llvm_addr_var(param));
      gp = MIN(6, gp + 1);
    }
    // Generated abort handlers can omit their unused ordinary parameters.
    if (param)
      param = param->next;
  }
  llvm_stmt(fn->body);
  if (!llvm_terminated) {
    Type *ret = fn->ty->return_ty;
    if (ret->kind == TY_VOID || (llvm_aggregate(ret) &&
        (llvm_return_abi.memory || !llvm_return_abi.count)))
      llvm_print("  ret void");
    else if (llvm_aggregate(ret))
      llvm_print("  ret %s zeroinitializer", llvm_return_type(ret));
    else
      llvm_print("  ret %s %s", llvm_type(ret), llvm_pointer(ret) ? "null" : "0");
  }
  llvm_print("}");
}

// PPC runtime registration is already ordinary AST. Preserve its existing
// phase priorities when LLVM emits the ELF constructor/destructor arrays.
static void llvm_initializers(Obj *prog, bool destructors) {
  int count = 0;
  for (Obj *fn = prog; fn; fn = fn->next)
    if (fn->is_function && fn->is_definition &&
        (destructors ? fn->fini_priority : fn->init_priority))
      count++;
  if (!count)
    return;
  llvm_print("@llvm.global_%s = appending global [%d x { i32, ptr, ptr }] [",
             destructors ? "dtors" : "ctors", count);
  int index = 0;
  for (Obj *fn = prog; fn; fn = fn->next) {
    int priority = destructors ? fn->fini_priority : fn->init_priority;
    if (!fn->is_function || !fn->is_definition || !priority)
      continue;
    llvm_reference(fn);
    llvm_print("  { i32, ptr, ptr } { i32 %d, ptr %s, ptr null }%s",
               priority, llvm_global_name(fn->name), ++index == count ? "" : ",");
  }
  llvm_print("]");
}

void codegen_llvm(Obj *prog, FILE *out) {
  llvm_out = out;
  llvm_prog = prog;
  llvm_refs = NULL;
  llvm_memset = false;
  llvm_memmove = false;
  llvm_print("; ppchibicc experimental LLVM IR backend (LLVM 19+, Linux x86-64)");
  llvm_print("target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
  llvm_print("target triple = \"x86_64-pc-linux-gnu\"");

  for (Obj *obj = prog; obj; obj = obj->next) {
    if (!obj->is_function && obj->is_definition)
      llvm_global(obj);
  }
  llvm_initializers(prog, false);
  llvm_initializers(prog, true);
  for (Obj *fn = prog; fn; fn = fn->next)
    if (fn->is_function && fn->is_definition && fn->is_live && !fn->is_static)
      llvm_function(fn);

  // Headers may contain unused static helpers using features outside this
  // backend's subset (notably stdarg.h). Emit internal definitions only when
  // referenced by emitted code or a data relocation, following references to
  // a fixed point so recursion and function pointers work too.
  bool progress;
  do {
    progress = false;
    for (LlvmRef *ref = llvm_refs; ref; ref = ref->next)
      if (ref->obj->is_function && ref->obj->is_definition && !ref->emitted) {
        llvm_function(ref->obj);
        progress = true;
      }
  } while (progress);

  for (LlvmRef *ref = llvm_refs; ref; ref = ref->next) {
    Obj *obj = ref->obj;
    if (obj->is_definition)
      continue;
    if (obj->is_tls)
      llvm_unsupported(obj->tok, "thread-local storage");
    if (!obj->is_function) {
      llvm_check_object(obj->ty, obj->tok);
      llvm_print("%s = external global [%d x i8]", llvm_global_name(obj->name),
                 MAX(0, obj->ty->size));
      continue;
    }
    llvm_check_signature(obj->ty, obj->tok, false);
    llvm_print("declare %s%s %s(%s)", llvm_abi_attr(obj->ty->return_ty),
               llvm_return_type(obj->ty->return_ty), llvm_global_name(obj->name),
               llvm_parameters(obj->ty, false, true));
  }
  if (llvm_memset)
    llvm_print("declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)");
  if (llvm_memmove)
    llvm_print("declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1)");
}
