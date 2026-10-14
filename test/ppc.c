#include "test.h"

struct Triangle { int a, b, c; };
struct Rectangle { int x, y; };

struct Figure { int color; } case { struct Triangle triangle; };
struct Figure + { struct Rectangle rectangle; };

typedef struct Figure2 {} case {} Figure2;
Figure2 + { int number; struct Triangle trian; };

struct WeekDay { int week_number; } case { void Monday; } const;

struct NoVoidDay { int week_number; } case {
  default void Unknown;
  void Monday;
  void Tuesday;
} const;

typedef struct InlineFigure {} case {
  default struct { int x, y; } rectangle;
  struct { int x, y; } rhombus;
  union { int number; double real; } scalar;
} InlineFigure;
InlineFigure + { struct { int a, b, c; } triangle; };

struct Switch {} case { void on; };
struct Switch + { void off; };

struct TokenKind { int group; } enum { number, plus };
struct TokenKind + { minus, eof };

struct CaseKind { int group; } case { void first; };
struct CaseKind + { second };

typedef struct Light { int line; } enum { red, green } Light;
Light + { blue };

struct ConstPayload { int code; };
typedef struct ConstKind { int line; } case {
  void none;
  struct ConstPayload payload;
  int number;
} const ConstKind;

struct ShapeRect { int x, y; };
struct ShapeTri { int a, b, c; };

typedef struct Shape {} case { struct ShapeRect rect; struct ShapeTri trian; } Shape;
typedef struct ShapeColor { int color; } case { Shape fig; } ShapeColor;
Shape + { ShapeColor color; };

struct PpcPoint { int x, y; };
struct PpcRect { struct PpcPoint leftDown, rightTop; };
typedef struct PpcColoredFigure { unsigned int color; } case {} PpcColoredFigure;
PpcColoredFigure + { struct PpcRect rect; };

struct Figure.rectangle global_rect;
struct Figure.triangle global_triangle = {.color=11, .triangle={1, 2, 3}};
Shape.color.rect global_shape_color_rect;
Shape.color.color.rect global_shape_color_color_rect;
Shape.color.rect global_shape_color_rect_array[2];
Shape.color.color.rect global_shape_color_color_rect_array[2];

int FigureCode<struct Figure *f>() = 0;
int FigureCode<struct Figure.void *f>() { return 50 + f->color; }
int FigureCode<struct Figure.triangle *t>() { return 100 + t->color + t->triangle.a; }
int FigureCode<struct Figure.rectangle *r>() { return 200 + r->color + r->rectangle.x; }

int FigureColorPlus<struct Figure *f>(int delta) { return f->color + delta; }
int FigureColorPlus<struct Figure.rectangle *r>(int delta) {
  return r->color + r->rectangle.y + delta;
}

int FigurePairCode<struct Figure *a, struct Figure *b>() = 0;
int FigurePairCode<struct Figure.void *a, struct Figure.void *b>() {
  return 1000 + a->color + b->color;
}
int FigurePairCode<struct Figure.void *a, struct Figure.rectangle *b>() {
  return 1300 + a->color + b->rectangle.x;
}
int FigurePairCode<struct Figure.triangle *a, struct Figure.triangle *b>() {
  return 1100 + a->triangle.a + b->triangle.b;
}
int FigurePairCode<struct Figure.triangle *a, struct Figure.rectangle *b>() {
  return 1200 + a->triangle.a + b->rectangle.x;
}
int FigurePairCode<struct Figure.rectangle *a, struct Figure.triangle *b>() {
  return 2100 + a->rectangle.x + b->triangle.a;
}
int FigurePairCode<struct Figure.rectangle *a, struct Figure.rectangle *b>() {
  return 2200 + a->rectangle.x + b->rectangle.y;
}

int FigurePairPlus<struct Figure *a, struct Figure *b>(int delta) = 0;
int FigurePairPlus<struct Figure.triangle *a, struct Figure.rectangle *b>(int delta) {
  return a->color + b->color + a->triangle.c + b->rectangle.y + delta;
}

int FigurePairDefault<struct Figure *a, struct Figure *b>() {
  return a->color * 10 + b->color;
}
int FigurePairDefault<struct Figure.rectangle *a, struct Figure.rectangle *b>() {
  return 900 + a->rectangle.x + b->rectangle.y;
}

int SwitchCode<struct Switch *s>() = 0;
int SwitchCode<struct Switch.on *s>() { return 1; }
int SwitchCode<struct Switch.off *s>() { return 2; }

int TokenKindCode<struct TokenKind *t>() = 0;
int TokenKindCode<struct TokenKind.void *t>() { return 100; }
int TokenKindCode<struct TokenKind.number *t>() { return 200; }
int TokenKindCode<struct TokenKind.plus *t>() { return 300; }
int TokenKindCode<struct TokenKind.minus *t>() { return 400; }
int TokenKindCode<struct TokenKind.eof *t>() { return 500; }

int LightCode<Light *l>() = 0;
int LightCode<Light.void *l>() { return l->line; }
int LightCode<Light.red *l>() { return 10 + l->line; }
int LightCode<Light.green *l>() { return 20 + l->line; }
int LightCode<Light.blue *l>() { return 30 + l->line; }

int WeekDayCode<struct WeekDay *d>() = 0;
int WeekDayCode<struct WeekDay.void *d>() { return d->week_number; }
int WeekDayCode<struct WeekDay.Monday *d>() { return d->week_number + 1; }

int WeekDayPairCode<struct WeekDay *a, struct WeekDay *b>() = 0;
int WeekDayPairCode<struct WeekDay.void *a, struct WeekDay.void *b>() {
  return 10 + a->week_number + b->week_number;
}
int WeekDayPairCode<struct WeekDay.void *a, struct WeekDay.Monday *b>() {
  return 20 + a->week_number + b->week_number;
}
int WeekDayPairCode<struct WeekDay.Monday *a, struct WeekDay.void *b>() {
  return 30 + a->week_number + b->week_number;
}
int WeekDayPairCode<struct WeekDay.Monday *a, struct WeekDay.Monday *b>() {
  return 40 + a->week_number + b->week_number;
}

int NoVoidDayCode<struct NoVoidDay *d>() { return 100 + d->week_number; }
int NoVoidDayCode<struct NoVoidDay.Monday *d>() { return d->week_number + 1; }

int NoVoidDayStrict<struct NoVoidDay *d>() = 0;
int NoVoidDayStrict<struct NoVoidDay.Unknown *d>() { return d->week_number; }
int NoVoidDayStrict<struct NoVoidDay.Monday *d>() { return d->week_number + 10; }
int NoVoidDayStrict<struct NoVoidDay.Tuesday *d>() { return d->week_number + 20; }

int NoVoidDayTag<struct NoVoidDay *d>() = 0;
int NoVoidDayTag<struct NoVoidDay.Unknown *d>() { return 0; }
int NoVoidDayTag<struct NoVoidDay.Monday *d>() { return 1; }
int NoVoidDayTag<struct NoVoidDay.Tuesday *d>() { return 2; }

int InlineFigureCode<InlineFigure *f>() = 0;
int InlineFigureCode<InlineFigure.rectangle *r>() {
  return r->rectangle.x + r->rectangle.y;
}
int InlineFigureCode<InlineFigure.rhombus *r>() {
  return r->rhombus.x * 10 + r->rhombus.y;
}
int InlineFigureCode<InlineFigure.scalar *s>() { return s->scalar.number; }
int InlineFigureCode<InlineFigure.triangle *t>() {
  return t->triangle.a + t->triangle.b + t->triangle.c;
}

int InlineFigureTag<InlineFigure *f>() = 0;
int InlineFigureTag<InlineFigure.rectangle *r>() { return 0; }
int InlineFigureTag<InlineFigure.rhombus *r>() { return 1; }
int InlineFigureTag<InlineFigure.scalar *s>() { return 2; }
int InlineFigureTag<InlineFigure.triangle *t>() { return 3; }

int FigureKind<struct Figure *f>() = 0;
int FigureKind<struct Figure.void *f>() { return 10; }
int FigureKind<struct Figure.triangle *f>() { return 11; }
int FigureKind<struct Figure.rectangle *f>() { return 12; }

int FigurePairKind<struct Figure *a, struct Figure *b>() = 0;
int FigurePairKind<struct Figure.void *a, struct Figure.rectangle *b>() { return 102; }
int FigurePairKind<struct Figure.triangle *a, struct Figure.rectangle *b>() { return 112; }

typedef struct PointerFigure { int color; } nullptr {
  struct Rectangle *rect, *box;
  struct Triangle *trian;
} PointerFigure;
PointerFigure + { int *number; void *raw; };

typedef struct EmptyPointer {} nullptr {} EmptyPointer;
EmptyPointer.void empty_pointer_before_extension;
EmptyPointer + { char *text; };
typedef struct ClosedPointer {} nullptr { int *value; } const ClosedPointer;
typedef struct ClosedEmptyPointer {} nullptr {} const ClosedEmptyPointer;
typedef struct __attribute__((packed)) PackedPointer { char c; } nullptr {
  int *value;
} PackedPointer;
typedef struct AlignedPointer { _Alignas(32) char c; } nullptr {
  int *value;
} AlignedPointer;

static struct Rectangle pointer_rectangle = {3, 10};
static PointerFigure.rect global_pointer_rect = {2, &pointer_rectangle};
static PointerFigure global_pointer_zero = {.color=7, .void=nullptr};
static PointerFigure.rect global_null_pointer_rect = {5, nullptr};

int PointerCode<PointerFigure *p>() { return -1; }
int PointerCode<PointerFigure.void *p>() { return p->color; }
int PointerCode<PointerFigure.rect *p>() {
  return p->color + (p->rect ? p->rect->x + p->rect->y : 100);
}
int PointerCode<PointerFigure.box *p>() { return p->color + p->box->x; }
int PointerCode<PointerFigure.trian *p>() {
  return p->color + p->trian->a + p->trian->b + p->trian->c;
}
int PointerCode<PointerFigure.number *p>() { return p->color + *p->number; }
int PointerPair<PointerFigure *a, PointerFigure *b>() { return -1; }
int PointerPair<PointerFigure.rect *a, PointerFigure.trian *b>() {
  return a->rect->x + b->trian->c;
}

static void test_pointer_generalizations(void) {
  ASSERT(sizeof(PointerFigure), sizeof(PointerFigure.void));
  ASSERT(sizeof(PointerFigure), sizeof(PointerFigure.rect));
  ASSERT(sizeof(PointerFigure), sizeof(PointerFigure.trian));
  ASSERT(sizeof(PointerFigure), sizeof(PointerFigure.raw));
  ASSERT(_Alignof(PointerFigure), _Alignof(PointerFigure.trian));
  ASSERT(sizeof(EmptyPointer), sizeof(EmptyPointer.text));
  ASSERT(sizeof(EmptyPointer), sizeof(EmptyPointer.void));
  ASSERT(16, sizeof(ClosedEmptyPointer));
  ASSERT(sizeof(ClosedPointer), sizeof(ClosedPointer.value));
  ASSERT(sizeof(PackedPointer), sizeof(PackedPointer.value));
  ASSERT(sizeof(PackedPointer), sizeof(PackedPointer.void));
  ASSERT(sizeof(AlignedPointer), sizeof(AlignedPointer.value));
  ASSERT(_Alignof(AlignedPointer), _Alignof(AlignedPointer.value));
  ASSERT(1, _Generic(global_pointer_zero.void, void *: 1, default: 0));
  ASSERT(1, global_pointer_zero.void == nullptr);
  ASSERT(1, empty_pointer_before_extension.void == nullptr);
  ASSERT(15, PointerCode<&global_pointer_rect>());
  ASSERT(7, PointerCode<&global_pointer_zero>());
  ASSERT(105, PointerCode<&global_null_pointer_rect>());
  ASSERT(6, get_spec_size(PointerFigure));
  ASSERT(2, get_spec_size(EmptyPointer));
  ASSERT(1, get_spec_size(ClosedEmptyPointer));

  struct Rectangle r = {3, 10};
  struct Triangle t = {3, 4, 5};
  PointerFigure.rect rect = {2, &r};
  PointerFigure.rect copy = rect;
  PointerFigure.trian trian = {.color=4, .trian=&t};
  PointerFigure.void zero = {9, nullptr};
  PointerFigure p = rect;
  ASSERT(15, PointerCode<&p>());
  ASSERT(15, PointerCode<&copy>());
  p = trian;
  ASSERT(16, PointerCode<&p>());
  ASSERT(8, (PointerPair<&rect, &p>()));
  zero = p;
  ASSERT(16, PointerCode<&zero>());
  p = zero;
  ASSERT(16, PointerCode<&p>());
  PointerFigure empty = {9, nullptr};
  zero = empty;
  p = zero;
  ASSERT(9, PointerCode<&p>());
  ASSERT(1, p.void == nullptr);
  ASSERT(0, spec_index_cmp(&p, get_spec_ptr(PointerFigure, 0)));
  p = rect;
  ASSERT(1, spec_index_cmp(&p, &rect));

  PointerFigure values[4] = {rect, trian, p};
  ASSERT(15, PointerCode<&values[0]>());
  ASSERT(16, PointerCode<&values[1]>());
  ASSERT(15, PointerCode<&values[2]>());
  ASSERT(0, PointerCode<&values[3]>());
  ASSERT(1, values[3].void == nullptr);
  for (int i = 0; i < 3; i++) values[i + 1] = values[i];
  ASSERT(15, PointerCode<&values[3]>());
  values[0] = trian;
  values[1] = empty;
  ASSERT(16, PointerCode<&values[0]>());
  ASSERT(9, PointerCode<&values[1]>());

  struct { long before; PointerFigure value; long after; } guard = {17, {}, 29};
  guard.value = trian;
  ASSERT(17, guard.before);
  ASSERT(29, guard.after);
  ASSERT(16, PointerCode<&guard.value>());

  int n = 42;
  PointerFigure.number number = {1, &n};
  p = number;
  ASSERT(43, PointerCode<&p>());
  PointerFigure.box box = {5, &r};
  p = box;
  ASSERT(8, PointerCode<&p>());
  PointerFigure.raw raw = {6, &r};
  p = raw;
  ASSERT(1, p.void == &r);
  ASSERT(-1, PointerCode<&p>());
  p = rect;
  r.x = 8;
  ASSERT(20, PointerCode<&p>());
  r.x = 3;
  int count = 0;
  int source_count = 0;
  values[count++] = (source_count++, trian);
  ASSERT(1, count);
  ASSERT(1, source_count);
  ASSERT(16, PointerCode<&values[0]>());
  static PointerFigure.rect local_static = {3, &pointer_rectangle};
  ASSERT(16, PointerCode<&local_static>());

  EmptyPointer.text text = "hello";
  EmptyPointer e = text;
  ASSERT(1, spec_index_cmp(&e, &text));
  ASSERT(1, e.void == text.text);
  EmptyPointer nil = nullptr;
  ASSERT(1, nil.void == nullptr);
  EmptyPointer.text null_text = nullptr;
  ASSERT(1, null_text.text == nullptr);
  e = nil;
  ASSERT(0, spec_index_cmp(&e, &nil));

  PointerFigure *allocated = create_spec(PointerFigure);
  ASSERT(1, allocated->void == nullptr);
  *allocated = rect;
  ASSERT(15, PointerCode<allocated>());
  init_spec(PointerFigure.void, allocated);
  ASSERT(2, PointerCode<allocated>());
  PackedPointer.value packed = {'a', &n};
  PackedPointer packed_base = packed;
  ASSERT(1, packed_base.void == &n);
  ASSERT('a', packed_base.c);
  AlignedPointer.value aligned = {'b', &n};
  AlignedPointer aligned_base = aligned;
  ASSERT(1, aligned_base.void == &n);
  ASSERT('b', aligned_base.c);
}

typedef struct FeatureDay { int week_number; } enum {
  Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday
} const FeatureDay;
typedef struct FeatureState { long stamp; int data[3]; } enum {
  Monday, Tuesday
} FeatureState;

static FeatureDay feature_global = FeatureDay.Monday;
static FeatureDay feature_globals[4] = {
  FeatureDay.Sunday, FeatureDay.Thursday, FeatureDay.Friday
};
static FeatureDay feature_matrix[2][2] = {
  {FeatureDay.Monday, FeatureDay.Tuesday}, {FeatureDay.Friday, FeatureDay.Sunday}
};
static struct { int before; FeatureDay day; int after; } feature_wrapper =
  {17, FeatureDay.Tuesday, 29};
static union { FeatureDay day; long other; } feature_union = {.day=FeatureDay.Friday};
static FeatureDay *feature_compound = &(FeatureDay){FeatureDay.Saturday};

int FeatureDayCode<FeatureDay *d>() = 0;
int FeatureDayCode<FeatureDay.void *d>() { return 0; }
int FeatureDayCode<FeatureDay.Monday *d>() { return 1; }
int FeatureDayCode<FeatureDay.Tuesday *d>() { return 2; }
int FeatureDayCode<FeatureDay.Wednesday *d>() { return 3; }
int FeatureDayCode<FeatureDay.Thursday *d>() { return 4; }
int FeatureDayCode<FeatureDay.Friday *d>() { return 5; }
int FeatureDayCode<FeatureDay.Saturday *d>() { return 6; }
int FeatureDayCode<FeatureDay.Sunday *d>() { return 7; }

static FeatureDay *feature_address(FeatureDay *d, int *count) {
  (*count)++;
  return d;
}

static int feature_static_sequence(void) {
  static FeatureDay d = FeatureDay.Monday;
  int old = FeatureDayCode<&d>();
  d = FeatureDay.Tuesday;
  return old;
}

static void test_enum_feature_assignment(void) {
  ASSERT(1, FeatureDayCode<&feature_global>());
  ASSERT(0, feature_global.week_number);
  ASSERT(7, FeatureDayCode<&feature_globals[0]>());
  ASSERT(4, FeatureDayCode<&feature_globals[1]>());
  ASSERT(5, FeatureDayCode<&feature_globals[2]>());
  ASSERT(0, FeatureDayCode<&feature_globals[3]>());
  ASSERT(2, FeatureDayCode<&feature_matrix[0][1]>());
  ASSERT(7, FeatureDayCode<&feature_matrix[1][1]>());
  ASSERT(2, FeatureDayCode<&feature_wrapper.day>());
  ASSERT(17, feature_wrapper.before);
  ASSERT(29, feature_wrapper.after);
  ASSERT(5, FeatureDayCode<&feature_union.day>());
  ASSERT(6, FeatureDayCode<feature_compound>());
  feature_global.week_number = 88;
  feature_global = FeatureDay.Sunday;
  ASSERT(7, FeatureDayCode<&feature_global>());
  ASSERT(88, feature_global.week_number);
  ASSERT(1, feature_static_sequence());
  ASSERT(2, feature_static_sequence());

  FeatureDay wd1 = FeatureDay.Monday;
  FeatureDay wd2[3] = {FeatureDay.Sunday, FeatureDay.Thursday, FeatureDay.Friday};
  ASSERT(1, FeatureDayCode<&wd1>());
  ASSERT(0, wd1.week_number);
  wd1.week_number = 42;
  wd1 = FeatureDay.Tuesday;
  ASSERT(2, FeatureDayCode<&wd1>());
  ASSERT(42, wd1.week_number);
  wd2[2].week_number = 17;
  wd2[2] = FeatureDay.Monday;
  ASSERT(1, FeatureDayCode<&wd2[2]>());
  ASSERT(17, wd2[2].week_number);
  ASSERT(7, FeatureDayCode<&wd2[0]>());
  ASSERT(4, FeatureDayCode<&wd2[1]>());
  FeatureDay.void zero = FeatureDay.Sunday;
  zero.week_number = 23;
  zero = FeatureDay.Friday;
  ASSERT(5, FeatureDayCode<&zero>());
  ASSERT(23, zero.week_number);
  FeatureDay braced = {FeatureDay.Wednesday,};
  FeatureDay parenthesized = ((FeatureDay.Thursday));
  FeatureDay compound = (FeatureDay){FeatureDay.Friday};
  ASSERT(3, FeatureDayCode<&braced>());
  ASSERT(4, FeatureDayCode<&parenthesized>());
  ASSERT(5, FeatureDayCode<&compound>());
  wd1 = (FeatureDay.Sunday);
  ASSERT(7, FeatureDayCode<&wd1>());
  ASSERT(42, wd1.week_number);
  FeatureDay designated[3] = {[2]=FeatureDay.Tuesday};
  ASSERT(0, FeatureDayCode<&designated[0]>());
  ASSERT(2, FeatureDayCode<&designated[2]>());
  static FeatureDay local_static = FeatureDay.Saturday;
  static FeatureDay static_array[2] = {FeatureDay.Friday, FeatureDay.Sunday};
  ASSERT(6, FeatureDayCode<&local_static>());
  ASSERT(5, FeatureDayCode<&static_array[0]>());
  ASSERT(7, FeatureDayCode<&static_array[1]>());

  int index = 0;
  wd2[index++] = FeatureDay.Wednesday;
  ASSERT(1, index);
  ASSERT(3, FeatureDayCode<&wd2[0]>());
  int calls = 0;
  *feature_address(&wd1, &calls) = FeatureDay.Monday;
  ASSERT(1, calls);
  ASSERT(1, FeatureDayCode<&wd1>());
  ASSERT(42, wd1.week_number);
  FeatureDay copied = (wd1 = FeatureDay.Tuesday);
  ASSERT(2, FeatureDayCode<&copied>());
  ASSERT(42, copied.week_number);
  wd2[0] = wd1 = FeatureDay.Friday;
  ASSERT(5, FeatureDayCode<&wd2[0]>());
  ASSERT(42, wd2[0].week_number);

  FeatureState state = {.stamp=123456789, .data={7, 8, 9}};
  FeatureState.Monday monday = {};
  state = FeatureState.Monday;
  ASSERT(1, spec_index_cmp(&state, &monday) >= 0);
  ASSERT(123456789, state.stamp);
  ASSERT(7, state.data[0]);
  ASSERT(8, state.data[1]);
  ASSERT(9, state.data[2]);
  state = FeatureState.Tuesday;
  ASSERT(123456789, state.stamp);
  ASSERT(9, state.data[2]);
  struct CaseKind converted = CaseKind.first;
  converted.group = 67;
  converted = CaseKind.second;
  struct CaseKind.second second_value = {};
  ASSERT(1, spec_index_cmp(&converted, &second_value) >= 0);
  ASSERT(67, converted.group);

  typedef FeatureDay DayAlias;
  DayAlias alias = DayAlias.Monday;
  alias = struct FeatureDay.Tuesday;
  ASSERT(2, FeatureDayCode<&alias>());
  alias = FeatureDay.Wednesday;
  ASSERT(3, FeatureDayCode<&alias>());
  ASSERT(sizeof(FeatureDay), sizeof(FeatureDay.Monday));

  // Feature qualification leaves ordinary variables and member access intact.
  {
    FeatureDay Monday = FeatureDay.Sunday;
    Monday.week_number = 73;
    FeatureDay copy = Monday;
    ASSERT(7, FeatureDayCode<&copy>());
    ASSERT(73, copy.week_number);
    copy = Monday;
    ASSERT(73, copy.week_number);
  }
  {
    enum { Monday = 61 };
    FeatureDay d = FeatureDay.Monday;
    d.week_number = Monday;
    d = FeatureDay.Tuesday;
    ASSERT(2, FeatureDayCode<&d>());
    ASSERT(61, d.week_number);
  }
  {
    struct { DayAlias Monday; } FeatureDay = {DayAlias.Friday};
    DayAlias d = FeatureDay.Monday;
    ASSERT(5, FeatureDayCode<&d>());
    d = struct FeatureDay.Monday;
    ASSERT(1, FeatureDayCode<&d>());
  }
}

int main() {
  test_enum_feature_assignment();
  test_pointer_generalizations();
  ASSERT(1, ({
    struct Figure *p = nullptr;
    struct Figure.rectangle *r = nullptr;
    p == nullptr && r == nullptr;
  }));
  ASSERT(20, ({ struct Figure.triangle t; sizeof(t); }));
  ASSERT(16, ({ struct Figure.rectangle r; sizeof(r); }));
  ASSERT(8, ({ struct WeekDay.Monday monday; sizeof(monday); }));
  ASSERT(4, ({ struct Switch.on s; sizeof(s); }));
  ASSERT(12, ({ InlineFigure f; sizeof(f); }));
  ASSERT(12, ({ InlineFigure.rectangle r; sizeof(r); }));
  ASSERT(12, ({ InlineFigure.rhombus r; sizeof(r); }));
  ASSERT(16, ({ InlineFigure.scalar s; sizeof(s); }));
  ASSERT(16, ({ InlineFigure.triangle t; sizeof(t); }));
  ASSERT(24, ({ Shape.color.rect s; sizeof(s); }));
  ASSERT(36, ({ Shape.color.color.rect s; sizeof(s); }));

  ASSERT(8, ({ struct Figure.triangle t; t.color=7; t.triangle.a=1; t.color+t.triangle.a; }));
  ASSERT(7, ({ struct Figure.rectangle r; r.color=3; r.rectangle.x=4; r.color+r.rectangle.x; }));
  ASSERT(7, ({ InlineFigure.rectangle r={3, 4}; InlineFigureCode<&r>(); }));
  ASSERT(7, ({ InlineFigure f={3, 4}; InlineFigureCode<&f>(); }));
  ASSERT(7, ({ InlineFigure.rectangle r={3, 4}; InlineFigure f=r; InlineFigureCode<&f>(); }));
  ASSERT(7, ({ InlineFigure f={3, 4}; InlineFigure.rectangle r=f; InlineFigureCode<&r>(); }));
  ASSERT(56, ({ InlineFigure.rhombus r={5, 6}; InlineFigureCode<&r>(); }));
  ASSERT(9, ({ InlineFigure.scalar s={.scalar={.number=9}}; InlineFigureCode<&s>(); }));
  ASSERT(24, ({ InlineFigure.triangle t={7, 8, 9}; InlineFigureCode<&t>(); }));
  ASSERT(7, ({ ConstKind.payload k={.line=2, .payload={5}}; k.line+k.payload.code; }));
  ASSERT(12, ({ ConstKind.number k={.line=7, .number=5}; k.line+k.number; }));
  ASSERT(4, get_spec_size(ConstKind));
  ASSERT(17, ({
    Shape.color.rect s;
    s.color.color=7;
    s.color.fig.rect.x=10;
    s.color.color+s.color.fig.rect.x;
  }));
  ASSERT(15, ({
    Shape.color.color.rect s;
    s.color.color=4;
    s.color.fig.color.color=5;
    s.color.fig.color.fig.rect.x=6;
    s.color.color+s.color.fig.color.color+s.color.fig.color.fig.rect.x;
  }));
  ASSERT(10, ({ Shape.rect r={4, 6}; r.rect.x+r.rect.y; }));
  ASSERT(10, ({ Shape.rect r={{4, 6}}; r.rect.x+r.rect.y; }));
  ASSERT(10, ({ Shape.rect r={.rect={4, 6}}; r.rect.x+r.rect.y; }));
  ASSERT(12, ({ struct Figure.rectangle r={3, 4, 5}; r.color+r.rectangle.x+r.rectangle.y; }));
  ASSERT(12, ({ struct Figure.rectangle r={3, {4, 5}}; r.color+r.rectangle.x+r.rectangle.y; }));
  ASSERT(12, ({ struct Figure.rectangle r={.color=3, .rectangle={4, 5}}; r.color+r.rectangle.x+r.rectangle.y; }));
  ASSERT(12, ({ struct Figure.rectangle r={.color=3, .rectangle={.x=4, .y=5}}; r.color+r.rectangle.x+r.rectangle.y; }));
  ASSERT(33, ({ Shape.color.rect s={7, 10, 16}; s.color.color+s.color.fig.rect.x+s.color.fig.rect.y; }));
  ASSERT(33, ({ Shape.color.rect s={{7, {10, 16}}}; s.color.color+s.color.fig.rect.x+s.color.fig.rect.y; }));
  ASSERT(33, ({
    Shape.color.rect s={.color={.color=7, .fig={.rect={10, 16}}}};
    s.color.color+s.color.fig.rect.x+s.color.fig.rect.y;
  }));
  ASSERT(123, ({
    PpcColoredFigure.rect s={
      .color=33,
      .rect.leftDown.x=10, .rect.leftDown.y=10,
      .rect.rightTop.x=30, .rect.rightTop.y=40,
    };
    s.color+s.rect.leftDown.x+s.rect.leftDown.y+s.rect.rightTop.x+s.rect.rightTop.y;
  }));
  ASSERT(6, ({ Shape.color.rect a[2]; *(int *)&a[0]+*(int *)&a[1]; }));
  ASSERT(2, ({ Shape.color.rect a[2]; *(int *)&a[0].color+*(int *)&a[1].color; }));
  ASSERT(2, ({ Shape.color.rect a[2]; *(int *)&a[0].color.fig+*(int *)&a[1].color.fig; }));
  ASSERT(91, ({
    Shape.color.rect a[2]={{7, 10, 16}, {8, 20, 30}};
    a[0].color.color+a[0].color.fig.rect.x+a[0].color.fig.rect.y+
      a[1].color.color+a[1].color.fig.rect.x+a[1].color.fig.rect.y;
  }));
  ASSERT(6, ({ Shape.color.color.rect a[2]; *(int *)&a[0]+*(int *)&a[1]; }));
  ASSERT(6, ({ Shape.color.color.rect a[2]; *(int *)&a[0].color.fig+*(int *)&a[1].color.fig; }));
  ASSERT(2, ({
    Shape.color.color.rect a[2];
    *(int *)&a[0].color.fig.color.fig+*(int *)&a[1].color.fig.color.fig;
  }));

  ASSERT(6, ({ struct Figure.triangle t; t.triangle.b=6; struct Figure.triangle u; u=t; u.triangle.b; }));

  ASSERT(5, ({ Figure2.number n; n.number=5; n.number; }));
  ASSERT(9, ({ Figure2.trian t; t.trian.c=9; t.trian.c; }));

  ASSERT(35, ({ struct WeekDay.Monday monday = {35}; monday.week_number; }));
  ASSERT(8, ({ struct Figure.triangle t; struct Figure *pf=&t; sizeof(pf); }));
  ASSERT(1, ({ struct Figure.triangle t; *(int *)&t; }));
  ASSERT(2, ({ struct Figure.rectangle r; *(int *)&r; }));
  ASSERT(1, ({ struct WeekDay.Monday monday = {35}; *(int *)&monday; }));
  ASSERT(1, ({ struct Switch.on s; *(int *)&s; }));
  ASSERT(2, ({ struct Switch.off s; *(int *)&s; }));
  ASSERT(8, ({ struct TokenKind k; sizeof(k); }));
  ASSERT(8, ({ struct TokenKind.plus p; sizeof(p); }));
  ASSERT(8, ({ struct CaseKind.second s; sizeof(s); }));
  ASSERT(8, ({ Light.green g; sizeof(g); }));
  ASSERT(3, get_spec_size(struct Figure));
  ASSERT(3, get_spec_size(Figure2));
  ASSERT(2, get_spec_size(struct WeekDay));
  ASSERT(3, get_spec_size(struct NoVoidDay));
  ASSERT(4, get_spec_size(InlineFigure));
  ASSERT(3, get_spec_size(struct Switch));
  ASSERT(5, get_spec_size(struct TokenKind));
  ASSERT(3, get_spec_size(struct CaseKind));
  ASSERT(4, get_spec_size(Light));
  ASSERT(4, get_spec_size(Shape));
  ASSERT(2, get_spec_size(ShapeColor));
  ASSERT(0, ({ struct Figure *f=get_spec_ptr(struct Figure, 0); *(int *)f; }));
  ASSERT(1, ({ struct Figure *f=get_spec_ptr(struct Figure, 1); *(int *)f; }));
  ASSERT(2, ({ struct Figure *f=get_spec_ptr(struct Figure, 2); *(int *)f; }));
  ASSERT(10, FigureKind<get_spec_ptr(struct Figure, 0)>());
  ASSERT(11, FigureKind<get_spec_ptr(struct Figure, 1)>());
  ASSERT(12, FigureKind<get_spec_ptr(struct Figure, 2)>());
  ASSERT(0, ({ struct NoVoidDay *d=get_spec_ptr(struct NoVoidDay, 0); *(int *)d; }));
  ASSERT(1, ({ struct NoVoidDay *d=get_spec_ptr(struct NoVoidDay, 1); *(int *)d; }));
  ASSERT(2, ({ struct NoVoidDay *d=get_spec_ptr(struct NoVoidDay, 2); *(int *)d; }));
  ASSERT(0, NoVoidDayTag<get_spec_ptr(struct NoVoidDay, 0)>());
  ASSERT(1, NoVoidDayTag<get_spec_ptr(struct NoVoidDay, 1)>());
  ASSERT(2, NoVoidDayTag<get_spec_ptr(struct NoVoidDay, 2)>());
  ASSERT(0, InlineFigureTag<get_spec_ptr(InlineFigure, 0)>());
  ASSERT(1, InlineFigureTag<get_spec_ptr(InlineFigure, 1)>());
  ASSERT(2, InlineFigureTag<get_spec_ptr(InlineFigure, 2)>());
  ASSERT(3, InlineFigureTag<get_spec_ptr(InlineFigure, 3)>());
  ASSERT(112, ({
    struct Figure *a=get_spec_ptr(struct Figure, 1);
    struct Figure *b=get_spec_ptr(struct Figure, 2);
    FigurePairKind<a, b>();
  }));
  ASSERT(1, SwitchCode<get_spec_ptr(struct Switch, 1)>());
  ASSERT(2, SwitchCode<get_spec_ptr(struct Switch, 2)>());
  ASSERT(200, TokenKindCode<get_spec_ptr(struct TokenKind, 1)>());
  ASSERT(300, TokenKindCode<get_spec_ptr(struct TokenKind, 2)>());
  ASSERT(400, TokenKindCode<get_spec_ptr(struct TokenKind, 3)>());
  ASSERT(500, TokenKindCode<get_spec_ptr(struct TokenKind, 4)>());
  ASSERT(1, ({ struct Figure.triangle a; struct Figure.triangle b; spec_index_cmp(&a, &b); }));
  ASSERT(-1, ({ struct Figure.triangle a; struct Figure.rectangle b; spec_index_cmp(&a, &b); }));
  ASSERT(0, ({
    struct Figure a;
    struct Figure *b=get_spec_ptr(struct Figure, 0);
    spec_index_cmp(&a, b);
  }));
  ASSERT(2, ({
    struct Figure.rectangle a;
    struct Figure *b=get_spec_ptr(struct Figure, 2);
    spec_index_cmp(&a, b);
  }));
  ASSERT(-1, ({
    struct Figure *a=get_spec_ptr(struct Figure, 1);
    struct Figure *b=get_spec_ptr(struct Figure, 2);
    spec_index_cmp(a, b);
  }));
  ASSERT(1, ({ struct Switch.on a; spec_index_cmp(&a, get_spec_ptr(struct Switch, 1)); }));
  ASSERT(-1, ({ struct Switch.on a; spec_index_cmp(&a, get_spec_ptr(struct Switch, 2)); }));
  ASSERT(0, ({ struct NoVoidDay.Unknown a; spec_index_cmp(&a, get_spec_ptr(struct NoVoidDay, 0)); }));
  ASSERT(1, ({ struct NoVoidDay.Monday a; spec_index_cmp(&a, get_spec_ptr(struct NoVoidDay, 1)); }));
  ASSERT(-1, ({ struct NoVoidDay.Monday a; spec_index_cmp(&a, get_spec_ptr(struct NoVoidDay, 2)); }));
  ASSERT(0, ({ InlineFigure.rectangle a; spec_index_cmp(&a, get_spec_ptr(InlineFigure, 0)); }));
  ASSERT(-1, ({ InlineFigure.rectangle a; spec_index_cmp(&a, get_spec_ptr(InlineFigure, 1)); }));
  ASSERT(2, *(int *)&global_rect);
  ASSERT(1, *(int *)&global_triangle);
  ASSERT(11, global_triangle.color);
  ASSERT(3, global_triangle.triangle.c);
  ASSERT(3, ({ Shape.color.rect s; *(int *)&s; }));
  ASSERT(1, ({ Shape.color.rect s; *(int *)&s.color; }));
  ASSERT(1, ({ Shape.color.rect s; *(int *)&s.color.fig; }));
  ASSERT(3, ({ Shape.color.rect s; Shape *p=&s; *(int *)p; }));
  ASSERT(3, ({ Shape.color.color.rect s; *(int *)&s; }));
  ASSERT(1, ({ Shape.color.color.rect s; *(int *)&s.color; }));
  ASSERT(3, ({ Shape.color.color.rect s; *(int *)&s.color.fig; }));
  ASSERT(1, ({ Shape.color.color.rect s; *(int *)&s.color.fig.color; }));
  ASSERT(1, ({ Shape.color.color.rect s; *(int *)&s.color.fig.color.fig; }));
  ASSERT(3, ({ Shape.color.color.rect s; Shape *p=&s; *(int *)p; }));
  ASSERT(3, *(int *)&global_shape_color_rect);
  ASSERT(1, *(int *)&global_shape_color_rect.color);
  ASSERT(1, *(int *)&global_shape_color_rect.color.fig);
  ASSERT(3, *(int *)&global_shape_color_color_rect);
  ASSERT(1, *(int *)&global_shape_color_color_rect.color);
  ASSERT(3, *(int *)&global_shape_color_color_rect.color.fig);
  ASSERT(1, *(int *)&global_shape_color_color_rect.color.fig.color);
  ASSERT(1, *(int *)&global_shape_color_color_rect.color.fig.color.fig);
  ASSERT(6, *(int *)&global_shape_color_rect_array[0]+*(int *)&global_shape_color_rect_array[1]);
  ASSERT(2, *(int *)&global_shape_color_rect_array[0].color.fig+*(int *)&global_shape_color_rect_array[1].color.fig);
  ASSERT(6, ({
    *(int *)&global_shape_color_color_rect_array[0].color.fig+
      *(int *)&global_shape_color_color_rect_array[1].color.fig;
  }));
  ASSERT(3, ({ static Shape.color.rect s; *(int *)&s; }));
  ASSERT(1, ({ static Shape.color.rect s; *(int *)&s.color; }));
  ASSERT(1, ({ static Shape.color.rect s; *(int *)&s.color.fig; }));
  ASSERT(6, ({ static Shape.color.rect a[2]; *(int *)&a[0]+*(int *)&a[1]; }));
  ASSERT(2, ({ static Shape.color.rect a[2]; *(int *)&a[0].color.fig+*(int *)&a[1].color.fig; }));

  ASSERT(108, ({ struct Figure.triangle t={.color=7, .triangle={1, 2, 3}}; FigureCode<&t>(); }));
  ASSERT(59, ({ struct Figure f={.color=9}; FigureCode<&f>(); }));
  ASSERT(207, ({
    struct Figure.rectangle r={.color=3, .rectangle={4, 5}};
    struct Figure *pf=(struct Figure *)&r;
    FigureCode<pf>();
  }));
  ASSERT(56, ({
    struct Figure *f=create_spec(struct Figure);
    f->color=6;
    FigureCode<f>();
  }));
  ASSERT(57, ({
    struct Figure.void *f=create_spec(struct Figure.void);
    f->color=7;
    FigureCode<f>();
  }));
  ASSERT(212, ({
    struct Figure.rectangle *r=create_spec(struct Figure.rectangle);
    r->color=3;
    r->rectangle.x=9;
    FigureCode<r>();
  }));
  ASSERT(3, ({
    Shape.color.rect *s=create_spec(Shape.color.rect);
    Shape *p=s;
    *(int *)p;
  }));
  ASSERT(1, ({
    Shape.color.rect *s=create_spec(Shape.color.rect);
    *(int *)&s->color;
  }));
  ASSERT(1, ({
    Shape.color.rect *s=create_spec(Shape.color.rect);
    *(int *)&s->color.fig;
  }));
  ASSERT(18, ({
    Shape.color.rect *s=create_spec(Shape.color.rect);
    s->color.color=8;
    s->color.fig.rect.x=10;
    s->color.color+s->color.fig.rect.x;
  }));
  ASSERT(3, ({
    Shape.color.color.rect *s=create_spec(Shape.color.color.rect);
    *(int *)&s->color.fig;
  }));
  ASSERT(1, ({
    Shape.color.color.rect *s=create_spec(Shape.color.color.rect);
    *(int *)&s->color.fig.color.fig;
  }));
  ASSERT(6, ({
    Shape.color.rect *a=create_spec(Shape.color.rect[2]);
    *(int *)&a[0]+*(int *)&a[1];
  }));
  ASSERT(2, ({
    Shape.color.rect *a=create_spec(Shape.color.rect[2]);
    *(int *)&a[0].color.fig+*(int *)&a[1].color.fig;
  }));
  ASSERT(6, ({
    Shape.color.color.rect *a=create_spec(Shape.color.color.rect[2]);
    *(int *)&a[0].color.fig+*(int *)&a[1].color.fig;
  }));
  ASSERT(2, ({
    Shape.color.color.rect *a=create_spec(Shape.color.color.rect[2]);
    *(int *)&a[0].color.fig.color.fig+*(int *)&a[1].color.fig.color.fig;
  }));
  ASSERT(56, ({
    struct Figure f;
    *(int *)&f=2;
    f.color=6;
    init_spec(struct Figure, &f);
    FigureCode<&f>();
  }));
  ASSERT(57, ({
    struct Figure f;
    *(int *)&f=2;
    f.color=7;
    init_spec(struct Figure.void, &f);
    FigureCode<&f>();
  }));
  ASSERT(214, ({
    struct Figure.rectangle r;
    struct Figure *pf=(struct Figure *)&r;
    *(int *)pf=0;
    r.color=3;
    r.rectangle.x=11;
    init_spec(struct Figure.rectangle, pf);
    FigureCode<pf>();
  }));
  ASSERT(3, ({
    union { Shape.color.rect s; char buf[sizeof(Shape.color.rect)]; } u;
    init_spec(Shape.color.rect, (void *)u.buf);
    *(int *)&u.s;
  }));
  ASSERT(1, ({
    union { Shape.color.rect s; char buf[sizeof(Shape.color.rect)]; } u;
    init_spec(Shape.color.rect, (void *)u.buf);
    *(int *)&u.s.color.fig;
  }));
  ASSERT(3, ({
    union { Shape.color.color.rect s; char buf[sizeof(Shape.color.color.rect)]; } u;
    init_spec(Shape.color.color.rect, (void *)u.buf);
    *(int *)&u.s.color.fig;
  }));
  ASSERT(1, ({
    union { Shape.color.color.rect s; char buf[sizeof(Shape.color.color.rect)]; } u;
    init_spec(Shape.color.color.rect, (void *)u.buf);
    *(int *)&u.s.color.fig.color.fig;
  }));
  ASSERT(6, ({
    union { Shape.color.rect s[2]; char buf[sizeof(Shape.color.rect[2])]; } u;
    init_spec(Shape.color.rect[2], (void *)u.buf);
    *(int *)&u.s[0]+*(int *)&u.s[1];
  }));
  ASSERT(2, ({
    union { Shape.color.rect s[2]; char buf[sizeof(Shape.color.rect[2])]; } u;
    init_spec(Shape.color.rect[2], (void *)u.buf);
    *(int *)&u.s[0].color.fig+*(int *)&u.s[1].color.fig;
  }));
  ASSERT(6, ({
    union { Shape.color.color.rect s[2]; char buf[sizeof(Shape.color.color.rect[2])]; } u;
    init_spec(Shape.color.color.rect[2], (void *)u.buf);
    *(int *)&u.s[0].color.fig+*(int *)&u.s[1].color.fig;
  }));
  ASSERT(2, ({
    union { Shape.color.color.rect s[2]; char buf[sizeof(Shape.color.color.rect[2])]; } u;
    init_spec(Shape.color.color.rect[2], (void *)u.buf);
    *(int *)&u.s[0].color.fig.color.fig+*(int *)&u.s[1].color.fig.color.fig;
  }));
  ASSERT(18, ({
    struct Figure.rectangle r={.color=3, .rectangle={4, 5}};
    struct Figure *pf=(struct Figure *)&r;
    FigureColorPlus<pf>(10);
  }));
  ASSERT(17, ({
    struct Figure.triangle t={.color=7, .triangle={1, 2, 3}};
    struct Figure *pf=(struct Figure *)&t;
    FigureColorPlus<pf>(10);
  }));
  ASSERT(19, ({
    struct Figure f={.color=9};
    FigureColorPlus<&f>(10);
  }));
  ASSERT(1015, ({
    struct Figure a={.color=7};
    struct Figure b={.color=8};
    FigurePairCode<&a, &b>();
  }));
  ASSERT(1311, ({
    struct Figure a={.color=7};
    struct Figure.rectangle b={.rectangle={4, 5}};
    FigurePairCode<&a, &b>();
  }));
  ASSERT(1311, ({
    struct Figure *a=create_spec(struct Figure);
    struct Figure.rectangle *b=create_spec(struct Figure.rectangle);
    a->color=7;
    b->rectangle.x=4;
    FigurePairCode<a, b>();
  }));
  ASSERT(1106, ({
    struct Figure.triangle a={.triangle={1, 2, 3}};
    struct Figure.triangle b={.triangle={4, 5, 6}};
    FigurePairCode<&a, &b>();
  }));
  ASSERT(1205, ({
    struct Figure.triangle a={.triangle={1, 2, 3}};
    struct Figure.rectangle b={.rectangle={4, 5}};
    struct Figure *pa=(struct Figure *)&a;
    struct Figure *pb=(struct Figure *)&b;
    FigurePairCode<pa, pb>();
  }));
  ASSERT(2105, ({
    struct Figure.rectangle a={.rectangle={4, 5}};
    struct Figure.triangle b={.triangle={1, 2, 3}};
    struct Figure *pa=(struct Figure *)&a;
    struct Figure *pb=(struct Figure *)&b;
    FigurePairCode<pa, pb>();
  }));
  ASSERT(2209, ({
    struct Figure.rectangle a={.rectangle={4, 5}};
    struct Figure.rectangle b={.rectangle={6, 5}};
    FigurePairCode<&a, &b>();
  }));
  ASSERT(30, ({
    struct Figure.triangle a={.color=7, .triangle={1, 2, 3}};
    struct Figure.rectangle b={.color=5, .rectangle={4, 5}};
    struct Figure *pa=(struct Figure *)&a;
    struct Figure *pb=(struct Figure *)&b;
    FigurePairPlus<pa, pb>(10);
  }));
  ASSERT(75, ({
    struct Figure.triangle a={.color=7, .triangle={1, 2, 3}};
    struct Figure.rectangle b={.color=5, .rectangle={4, 5}};
    struct Figure *pa=(struct Figure *)&a;
    struct Figure *pb=(struct Figure *)&b;
    FigurePairDefault<pa, pb>();
  }));
  ASSERT(909, ({
    struct Figure.rectangle a={.rectangle={4, 5}};
    struct Figure.rectangle b={.rectangle={6, 5}};
    FigurePairDefault<&a, &b>();
  }));
  ASSERT(12, ({
    Figure2.number *n=create_spec(Figure2.number);
    n->number=12;
    n->number;
  }));
  ASSERT(13, ({
    InlineFigure *f=create_spec(InlineFigure);
    f->rectangle.x=6;
    f->rectangle.y=7;
    InlineFigureCode<f>();
  }));
  ASSERT(30, ({
    InlineFigure.triangle *t=create_spec(InlineFigure.triangle);
    t->triangle.a=9;
    t->triangle.b=10;
    t->triangle.c=11;
    InlineFigureCode<t>();
  }));
  ASSERT(1, ({ struct Switch.on *s=create_spec(struct Switch.on); SwitchCode<s>(); }));
  ASSERT(5, ({ struct WeekDay d={5}; WeekDayCode<&d>(); }));
  ASSERT(36, ({ struct WeekDay.Monday monday={35}; WeekDayCode<&monday>(); }));
  ASSERT(73, ({
    struct WeekDay.Monday a={11};
    struct WeekDay.Monday b={22};
    WeekDayPairCode<&a, &b>();
  }));
  ASSERT(36, ({ struct NoVoidDay.Monday monday={35}; NoVoidDayCode<&monday>(); }));
  ASSERT(125, ({ struct NoVoidDay.Tuesday tuesday={25}; NoVoidDayCode<&tuesday>(); }));
  ASSERT(35, ({ struct NoVoidDay.Unknown unknown={35}; NoVoidDayStrict<&unknown>(); }));
  ASSERT(45, ({ struct NoVoidDay.Monday monday={35}; NoVoidDayStrict<&monday>(); }));
  ASSERT(45, ({ struct NoVoidDay.Tuesday tuesday={25}; NoVoidDayStrict<&tuesday>(); }));
  ASSERT(1, ({
    struct Switch s;
    *(int *)&s=0;
    init_spec(struct Switch.on, &s);
    SwitchCode<&s>();
  }));
  ASSERT(1, ({ struct Switch.on s; SwitchCode<&s>(); }));
  ASSERT(2, ({
    struct Switch.off s;
    struct Switch *ps=(struct Switch *)&s;
    SwitchCode<ps>();
  }));
  ASSERT(307, ({
    struct TokenKind k={.group=1};
    struct TokenKind.plus plus={.group=7};
    k=plus;
    k.group+TokenKindCode<&k>();
  }));
  ASSERT(409, ({
    struct TokenKind k;
    struct TokenKind.minus minus={.group=9};
    k=minus;
    k.group+TokenKindCode<&k>();
  }));
  ASSERT(409, ({
    struct TokenKind.void any;
    struct TokenKind.minus minus={.group=9};
    any=minus;
    any.group+TokenKindCode<&any>();
  }));
  ASSERT(504, ({
    struct TokenKind.eof eof={.group=4};
    struct TokenKind.void any=eof;
    any.group+TokenKindCode<&any>();
  }));
  ASSERT(103, ({
    struct TokenKind zero={.group=3};
    struct TokenKind.void any=zero;
    any.group+TokenKindCode<&any>();
  }));
  ASSERT(104, ({
    struct TokenKind.void any={.group=4};
    struct TokenKind zero=any;
    zero.group+TokenKindCode<&zero>();
  }));
  ASSERT(1510, ({
    struct TokenKind zero={.group=0};
    struct TokenKind.number number={.group=1};
    struct TokenKind.plus plus={.group=2};
    struct TokenKind.minus minus={.group=3};
    struct TokenKind.eof eof={.group=4};
    struct TokenKind days[5]={zero, number, plus, minus, eof};
    days[0].group+TokenKindCode<&days[0]>()+
      days[1].group+TokenKindCode<&days[1]>()+
      days[2].group+TokenKindCode<&days[2]>()+
      days[3].group+TokenKindCode<&days[3]>()+
      days[4].group+TokenKindCode<&days[4]>();
  }));
  ASSERT(2, ({
    struct CaseKind k;
    struct CaseKind.second second={.group=11};
    k=second;
    *(int *)&k;
  }));
  ASSERT(11, ({
    struct CaseKind k;
    struct CaseKind.second second={.group=11};
    k=second;
    k.group;
  }));
  ASSERT(21, ({
    Light.red red={.line=11};
    Light light=red;
    LightCode<&light>();
  }));
  ASSERT(32, ({
    Light light;
    Light.blue blue={.line=2};
    light=blue;
    LightCode<&light>();
  }));

  printf("OK\n");
  return 0;
}
