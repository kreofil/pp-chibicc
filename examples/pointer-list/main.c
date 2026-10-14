#include <stdio.h>

typedef struct Rectangle {
  int x;
  int y;
} Rectangle;

typedef struct Triangle {
  int a;
  int b;
  int c;
} Triangle;

typedef struct Figure {} nullptr {Rectangle* rect;} Figure;
Figure + {Triangle* trian;};

void Print<Figure* f>() {
  printf("No figure\n");
}

void Print<Figure.rect* f>() {
  printf("Rectangle (%d, %d)\n", f->rect->x, f->rect->y);
}

void Print<Figure.trian* f>() {
  printf("Triangle (%d, %d, %d)\n", f->trian->a, f->trian->b, f->trian->c);
}

int main() {
  Rectangle r1 = {3,10};
  Rectangle r2 = {13,10};
  Triangle  t1 = {3,4,5};
  Triangle  t2 = {30,40,50};

  Figure.rect fr1 = &r1;
  Figure.rect fr2 = &r2;
  Figure.trian ft1 = &t1;
  Figure.trian ft2 = &t2;
  Figure       f1 = ft2;

  Figure  figures[10] = {
    fr1,
    fr2,
    ft1,
    ft2,
    f1,
  };

  figures[5] = figures[0];
  figures[6] = figures[1];
  figures[7] = figures[2];
  figures[8] = figures[3];

  for(int i = 0; i < 10; i++) {
    Print<(figures+i)>();
  }

  return 0;
}
