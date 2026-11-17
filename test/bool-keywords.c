// Foreign C11 builds provide a compatibility reference. Native ppchibicc and
// foreign C23 builds must recognize the words before including any header.
#if !defined(__chibicc__) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ <= 201710L)
#include <stdbool.h>
#define BOOL_LEGACY_REFERENCE 1
#endif

typedef bool HeaderlessBool;
static HeaderlessBool before_header = true;
static bool before_header_function(bool value) { return !value; }

#ifndef BOOL_LEGACY_REFERENCE
enum { HeaderlessTrueSize = sizeof(true) == sizeof(bool),
       HeaderlessFalseType = _Generic(false, _Bool: 1, default: 0) };
#endif

#include <stdbool.h>
#include <stdarg.h>
int puts(const char *s);
int printf(const char *fmt, ...);
#define CHECK(expr) do { if (!(expr)) { printf("bool failure: %d\n", __LINE__); return 1; } } while (0)
#define JOIN_IMPL(a, b) a##b
#define JOIN(a, b) JOIN_IMPL(a, b)
#define STRING_IMPL(x) #x
#define STRING(x) STRING_IMPL(x)

#if !true || false || (true + true != 2)
#error Boolean keywords must work in preprocessing expressions
#endif
#if false
#error false must not select this branch
#elif true
enum { BooleanPreprocessorBranch = 19 };
#else
#error true must select its elif branch
#endif
#ifndef BOOL_LEGACY_REFERENCE
#if defined(bool) || defined(true) || defined(false)
#error Boolean words must remain keywords after stdbool.h
#endif
#undef bool
#undef true
#undef false
#endif

enum { BoolSameSize = sizeof(bool) == sizeof(_Bool),
       BoolSameAlignment = _Alignof(bool) == _Alignof(_Bool),
       BoolNormalizedConstant = (bool)256 == true };
enum { BoolOff = false, BoolOn = true, BoolCount = true + true };

static int target;
bool bool_emitted_flip(bool value) { return !value; }
bool bool_emitted_many(bool a, bool b, bool c, bool d, bool e,
                       bool f, bool g, bool h, bool i) {
  return a && !b && c && !d && e && !f && g && !h && i;
}
static bool global_false = false;
static bool global_normalized = 512;
static bool global_fractional = 0.25;
static bool global_negative_zero = -0.0;
#ifdef __chibicc__
#define BOOL_CONSTANT_NAN (0.0 / 0.0)
#else
// The reference compilers provide a quiet NaN without an invalid floating
// divide in their static-initializer evaluator.
#define BOOL_CONSTANT_NAN __builtin_nan("")
#endif
static bool global_nan = (bool)BOOL_CONSTANT_NAN;
static bool global_nan_condition = BOOL_CONSTANT_NAN ? true : false;
static bool global_pointer = &target;
static bool global_function = bool_emitted_flip;
static struct { char before; bool flag; char after; } guarded = {17, (bool)&target, 29};

typedef struct BoolRecord { bool flag; int value; } BoolRecord;
static BoolRecord global_record = {512, 13};
BoolRecord bool_emitted_record(BoolRecord value) {
  value.flag = !value.flag;
  value.value += 7;
  return value;
}

static int promoted(int n, ...) {
  va_list ap;
  va_start(ap, n);
  int result = 0;
  for (int i = 0; i < n; ++i) result = 10 * result + va_arg(ap, int);
  va_end(ap);
  return result;
}

static int select(bool value) {
  switch (value) {
    case false: return 13;
    case true: return 17;
  }
  return -1;
}

#ifdef TEST_BOOL_HOST
bool host_bool_flip(_Bool value);
BoolRecord host_bool_record(BoolRecord value);
bool host_bool_many(bool, bool, bool, bool, bool, bool, bool, bool, bool);
int host_bool_checks(void);
#endif

int main(void) {
  CHECK(before_header && before_header_function(false));
  CHECK(sizeof(bool) == 1 && _Alignof(bool) == 1);
  CHECK(BoolSameSize && BoolSameAlignment && BoolNormalizedConstant);
  CHECK(BooleanPreprocessorBranch == 19);
  CHECK(_Generic(before_header, _Bool: 1, default: 0));
#ifndef BOOL_LEGACY_REFERENCE
  CHECK(HeaderlessTrueSize && HeaderlessFalseType);
  CHECK(_Generic(true, bool: 1, default: 0));
  CHECK(_Generic(JOIN(tr, ue), bool: 1, default: 0));
  CHECK(sizeof(false) == 1 && sizeof(STRING(true)) == 5);
#endif
  CHECK(_Generic(true + false, int: 1, default: 0));
  CHECK(_Generic(!true, int: 1, default: 0));
  CHECK(BoolOff == 0 && BoolOn == 1 && BoolCount == 2 && ~true == -2);
  CHECK(!global_false && global_normalized && global_pointer && global_function);
  CHECK(global_fractional && !global_negative_zero && global_nan && global_nan_condition);
  CHECK(global_record.flag && global_record.value == 13);
  CHECK(guarded.before == 17 && guarded.flag && guarded.after == 29);
  bool bits[BoolCount] = {false, true};
  bool *pointer = bits;
  CHECK(!pointer[0] && pointer[1]);
  int n = 3;
  bool dynamic[n];
  for (int i = 0; i < n; ++i) dynamic[i] = i;
  CHECK(!dynamic[0] && dynamic[1] && dynamic[2]);
  bool value = 256;
  CHECK(value == true);
  value = -65536;
  CHECK(value == true);
  value = 0;
  CHECK(value == false);
  value = true;
  value++;
  CHECK(value == true);
  value--;
  CHECK(value == false);
  CHECK((bool)&target && !(bool)(void *)0);
  bool (*callback)(bool) = bool_emitted_flip;
  CHECK(callback(false) && !callback(true));
  CHECK(select(false) == 13 && select(true) == 17);
  CHECK(promoted(4, true, false, (bool)256, (bool)0) == 1010);
  BoolRecord record = bool_emitted_record((BoolRecord){true, 5});
  CHECK(!record.flag && record.value == 12);
  union { unsigned long bits; double value; } nan = {0x7ff8000000000001UL};
  bool from_float = (float)nan.value;
  bool from_double = nan.value;
  bool from_long_double = (long double)nan.value;
  CHECK(from_float && from_double && from_long_double);
  CHECK(!(bool)0.0 && !(bool)-0.0 && !(bool)-0.0L && (bool)0.25L);
#ifdef TEST_BOOL_HOST
  CHECK(host_bool_flip(false) && !host_bool_flip(true));
  CHECK(host_bool_many(true, false, true, false, true, false, true, false, true));
  record = host_bool_record((BoolRecord){true, 5});
  CHECK(!record.flag && record.value == 12 && host_bool_checks());
#endif
  puts("bool keywords OK");
  return 0;
}
