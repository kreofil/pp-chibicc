#include "test.h"

struct Triangle { int a, b, c; };
struct Rectangle { int x, y; };

struct Figure { int color; } case { struct Triangle triangle; };
struct Figure + { struct Rectangle rectangle; };

typedef struct Figure2 {} case {} Figure2;
Figure2 + { int number; struct Triangle trian; };

struct WeekDay { int week_number; } case { void Monday; } const;

struct NoVoidDay { int week_number; } case {
  void Monday;
  void Tuesday;
} const = 0;

struct Switch {} case { void on; };
struct Switch + { void off; };

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
int NoVoidDayStrict<struct NoVoidDay.Monday *d>() { return d->week_number + 10; }
int NoVoidDayStrict<struct NoVoidDay.Tuesday *d>() { return d->week_number + 20; }

int FigureKind<struct Figure *f>() = 0;
int FigureKind<struct Figure.void *f>() { return 10; }
int FigureKind<struct Figure.triangle *f>() { return 11; }
int FigureKind<struct Figure.rectangle *f>() { return 12; }

int FigurePairKind<struct Figure *a, struct Figure *b>() = 0;
int FigurePairKind<struct Figure.void *a, struct Figure.rectangle *b>() { return 102; }
int FigurePairKind<struct Figure.triangle *a, struct Figure.rectangle *b>() { return 112; }

int main() {
  ASSERT(20, ({ struct Figure.triangle t; sizeof(t); }));
  ASSERT(16, ({ struct Figure.rectangle r; sizeof(r); }));
  ASSERT(8, ({ struct WeekDay.Monday monday; sizeof(monday); }));
  ASSERT(4, ({ struct Switch.on s; sizeof(s); }));
  ASSERT(24, ({ Shape.color.rect s; sizeof(s); }));
  ASSERT(36, ({ Shape.color.color.rect s; sizeof(s); }));

  ASSERT(8, ({ struct Figure.triangle t; t.color=7; t.triangle.a=1; t.color+t.triangle.a; }));
  ASSERT(7, ({ struct Figure.rectangle r; r.color=3; r.rectangle.x=4; r.color+r.rectangle.x; }));
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
  ASSERT(3, get_spec_size(struct Figure));
  ASSERT(3, get_spec_size(Figure2));
  ASSERT(2, get_spec_size(struct WeekDay));
  ASSERT(3, get_spec_size(struct Switch));
  ASSERT(4, get_spec_size(Shape));
  ASSERT(2, get_spec_size(ShapeColor));
  ASSERT(0, ({ struct Figure *f=get_spec_ptr(struct Figure, 0); *(int *)f; }));
  ASSERT(1, ({ struct Figure *f=get_spec_ptr(struct Figure, 1); *(int *)f; }));
  ASSERT(2, ({ struct Figure *f=get_spec_ptr(struct Figure, 2); *(int *)f; }));
  ASSERT(10, FigureKind<get_spec_ptr(struct Figure, 0)>());
  ASSERT(11, FigureKind<get_spec_ptr(struct Figure, 1)>());
  ASSERT(12, FigureKind<get_spec_ptr(struct Figure, 2)>());
  ASSERT(112, ({
    struct Figure *a=get_spec_ptr(struct Figure, 1);
    struct Figure *b=get_spec_ptr(struct Figure, 2);
    FigurePairKind<a, b>();
  }));
  ASSERT(1, SwitchCode<get_spec_ptr(struct Switch, 1)>());
  ASSERT(2, SwitchCode<get_spec_ptr(struct Switch, 2)>());
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

  printf("OK\n");
  return 0;
}
