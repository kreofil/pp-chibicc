#include <stdlib.h>
int puts(const char *s);
int printf(const char *fmt, ...);
#define CHECK(expr) do { if (!(expr)) { printf("closed index failure: %d\n", __LINE__); return 1; } } while (0)

typedef struct ClosedCase { int id; } case {
  struct { int value; } first;
  int second;
} const ClosedCase;
typedef struct ClosedEnum { int id; } enum { first, second } const ClosedEnum;
typedef struct ClosedPointer { int id; } nullptr { int *first; double *second; } const ClosedPointer;
typedef struct ClosedUnion { int id; } union { int first; double second; } const ClosedUnion;
typedef struct ClosedDefault { int id; } case { default int origin; void other; } const ClosedDefault;
typedef struct ClosedEmpty {} case {} const ClosedEmpty;
typedef struct ClosedEmptyEnum {} enum {} const ClosedEmptyEnum;
typedef struct ClosedEmptyPointer {} nullptr {} const ClosedEmptyPointer;
typedef struct ClosedEmptyUnion {} union {} const ClosedEmptyUnion;
typedef ClosedCase CaseAlias;

enum { CaseFirst = get_subtype_index(ClosedCase.first),
       CaseSecond = get_subtype_index(struct ClosedCase.second),
       CaseSize = get_spec_size(ClosedCase),
       EnumSecond = get_subtype_index(ClosedEnum.second),
       PointerSecond = get_subtype_index(ClosedPointer.second),
       UnionSecond = get_subtype_index(ClosedUnion.second),
       DefaultZero = get_subtype_index(ClosedDefault.origin),
       DefaultOther = get_subtype_index(ClosedDefault.other),
       EmptySize = get_spec_size(ClosedEmpty),
       ProductSize = get_spec_size(ClosedCase) * get_spec_size(ClosedEnum) };
int global_bounds[get_spec_size(ClosedCase)];
int global_index = get_subtype_index(ClosedUnion.second);
ClosedCase.first tentative_case;
ClosedCase.second explicit_case = {5, 7};
ClosedCase.first case_array[2] = {{1, {11}}, {2, {22}}};
ClosedEnum global_features[3] = {ClosedEnum.second, ClosedEnum.first};
ClosedPointer.second global_pointer = {9, 0};
ClosedUnion.second global_union = {4, 2.5};
ClosedDefault.origin global_default = {3, 17};

typedef struct OpenInner {} enum { first, second } OpenInner;
typedef struct ClosedOuter {} case { OpenInner child; ClosedEnum closed; } const ClosedOuter;
ClosedOuter.child nested_open = {{OpenInner.second}};
ClosedOuter.closed nested_closed = {{ClosedEnum.second}};

typedef struct OpenArgument {} enum { first } OpenArgument;
int mixed<ClosedCase *a, OpenArgument *b>(void) { return 7; }
OpenArgument + { second };
int mixed<ClosedCase.first *a, OpenArgument.second *b>(void) { return 12; }
int pair<ClosedCase *a, ClosedEnum *b>(void) { return 20; }
int pair<ClosedCase.second *a, ClosedEnum.first *b>(void) { return 31; }
int strict<ClosedEnum *x>(void) = 0;
int strict<ClosedEnum.void *x>(void) { return 0; }
int strict<ClosedEnum.first *x>(void) { return 101; }
int strict<ClosedEnum.second *x>(void) { return 102; }

static int classify_case(ClosedCase *p) {
  switch (get_spec_var_index(p)) {
    case get_subtype_index(ClosedCase): return 10;
    case get_subtype_index(ClosedCase.first): return 11;
    case get_subtype_index(ClosedCase.second): return 12;
    default: return -1;
  }
}
static int classify_enum(ClosedEnum *p) {
  switch (get_spec_var_index(p)) {
    case get_subtype_index(ClosedEnum.void): return 20;
    case get_subtype_index(ClosedEnum.first): return 21;
    case get_subtype_index(ClosedEnum.second): return 22;
    default: return -1;
  }
}
static int classify_pointer(ClosedPointer *p) {
  switch (get_spec_var_index(p)) {
    case get_subtype_index(ClosedPointer): return 30;
    case get_subtype_index(ClosedPointer.first): return 31;
    case get_subtype_index(ClosedPointer.second): return 32;
    default: return -1;
  }
}
static int classify_union(ClosedUnion *p) {
  switch (get_spec_var_index(p)) {
    case get_subtype_index(ClosedUnion): return 40;
    case get_subtype_index(ClosedUnion.first): return 41;
    case get_subtype_index(ClosedUnion.second): return 42;
    default: return -1;
  }
}
int main(void) {
  CHECK(CaseFirst == 1 && CaseSecond == 2 && CaseSize == 3);
  CHECK(EnumSecond == 2 && PointerSecond == 2 && UnionSecond == 2);
  CHECK(DefaultZero == 0 && DefaultOther == 1 && EmptySize == 1 && ProductSize == 9);
  CHECK(get_subtype_index(CaseAlias.first) == 1 && sizeof(global_bounds) == 3 * sizeof(int));
  CHECK(get_spec_size(ClosedEmptyEnum) == 1 && get_spec_size(ClosedEmptyPointer) == 1);
  CHECK(get_spec_size(ClosedEmptyUnion) == 1 && global_index == 2);
  CHECK(get_spec_var_index(&tentative_case) == 1 && classify_case(&explicit_case) == 12);
  CHECK(get_spec_var_index(&case_array[1]) == 1 && case_array[1].first.value == 22);
  CHECK(classify_enum(&global_features[0]) == 22 && classify_enum(&global_features[1]) == 21);
  CHECK(classify_enum(&global_features[2]) == 20 && classify_pointer(&global_pointer) == 32);
  CHECK(classify_union(&global_union) == 42 && global_union.second == 2.5);
  CHECK(get_spec_var_index(&global_default) == 0 && global_default.origin == 17);
  CHECK(get_spec_var_index(&nested_open) == 1);
  CHECK(get_spec_var_index(&nested_open.child) == get_subtype_index(OpenInner.second));
  CHECK(get_spec_var_index(&nested_closed) == 2 && get_spec_var_index(&nested_closed.closed) == 2);
  ClosedCase zero = {0};
  CaseAlias.first local = {8, {14}};
  static ClosedCase.second static_plain;
  static ClosedCase.second static_explicit = {6, 9};
  CHECK(classify_case(&zero) == 10 && classify_case(&local) == 11);
  CHECK(classify_case(&static_plain) == 12 && classify_case(&static_explicit) == 12);
  ClosedEnum e = ClosedEnum.first;
  e.id = 81;
  CHECK(classify_enum(&e) == 21 && strict<&e>() == 101);
  e = ClosedEnum.second;
  CHECK(classify_enum(&e) == 22 && e.id == 81 && strict<&e>() == 102);
  ClosedEnum empty = {0};
  CHECK(strict<&empty>() == 0);
  ClosedPointer.first pointer = {1, 0};
  ClosedUnion.first value = {2, 17};
  CHECK(classify_pointer(&pointer) == 31 && classify_union(&value) == 41);
  for (int i = 0; i < CaseSize; i++) {
    CHECK(get_spec_var_index(get_spec_ptr(ClosedCase, i)) == i);
    CHECK(get_spec_var_index(get_spec_ptr(ClosedEnum, i)) == i);
    CHECK(get_spec_var_index(get_spec_ptr(ClosedPointer, i)) == i);
    CHECK(get_spec_var_index(get_spec_ptr(ClosedUnion, i)) == i);
  }
  CHECK(get_spec_var_index(get_spec_ptr(ClosedEmptyUnion, 0)) == 0);
  ClosedUnion *allocated = create_spec(ClosedUnion.second, 2);
  CHECK(get_spec_var_index(&allocated[0]) == 2 && get_spec_var_index(&allocated[1]) == 2);
  init_spec(ClosedUnion.first, allocated, 2);
  CHECK(get_spec_var_index(&allocated[0]) == 1 && get_spec_var_index(&allocated[1]) == 1);
  free(allocated);
  int side_effect = 0;
  switch (get_spec_var_index((side_effect++, &explicit_case))) {
    case get_subtype_index(ClosedCase.second): break;
    default: return 2;
  }
  CHECK(side_effect == 1);
  OpenArgument.second open_value;
  CHECK((mixed<&local, &open_value>() == 12 && mixed<&zero, &open_value>() == 7));
  e = ClosedEnum.first;
  CHECK((pair<&explicit_case, &e>() == 31 && pair<&zero, &e>() == 20));
  e = ClosedEnum.second;
  CHECK((pair<&explicit_case, &e>() == 20));
  puts("closed indices OK");
  return 0;
}
