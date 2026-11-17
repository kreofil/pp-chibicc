#include "stdarg-records.h"
int puts(const char *);
int printf(const char *,...);
#define CHECK(x) do { if(!(x)) { printf("PPC record varargs failure: %d\n",__LINE__); return 1; } } while(0)
typedef struct RecordChoice { int bias; } case { int extra; } const RecordChoice;
static double record_sum(va_list ap,int n) {
  double sum=0;
  for(int i=0;i<n;i++) { VrMixed v=va_arg(ap,VrMixed); sum+=(i+1)*(v.a+v.b); }
  return sum;
}
double record_total<RecordChoice *v>(int n,...) {
  va_list ap; va_start(ap,n);
  double sum=record_sum(ap,n); va_end(ap); return v->bias+sum;
}
double record_total<RecordChoice.extra *v>(int n,...) {
  va_list ap,copy; va_start(ap,n); va_copy(copy,ap);
  double sum=record_sum(copy,n); va_end(copy); va_end(ap);
  return v->bias+v->extra+sum;
}
VrLarge record_pair<RecordChoice *a,RecordChoice *b>(int n,...) {
  va_list ap; va_start(ap,n);
  long sum=0;
  for(int i=0;i<n;i++) { VrInt v=va_arg(ap,VrInt); sum+=(i+1)*(v.a+v.b); }
  va_end(ap); return (VrLarge){a->bias,b->bias,sum};
}
VrLarge record_pair<RecordChoice.extra *a,RecordChoice.extra *b>(int n,...) {
  va_list ap; va_start(ap,n);
  long sum=0;
  for(int i=0;i<n;i++) { VrInt v=va_arg(ap,VrInt); sum+=(i+1)*(v.a+v.b); }
  va_end(ap); return (VrLarge){a->extra,b->extra,sum};
}
int main(void) {
  RecordChoice base={7}; RecordChoice.extra a={11,13},b={17,19};
  VrMixed m={7,2.5}; VrInt pair={3,5};
  CHECK(record_total<&base>(2,m,m)==35.5);
  CHECK(record_total<&a>(8,m,m,m,m,m,m,m,m)==366);
  VrLarge x=record_pair<&base,&a>(4,pair,pair,pair,pair);
  VrLarge y=record_pair<&a,&b>(8,pair,pair,pair,pair,pair,pair,pair,pair);
  CHECK(x.a==7 && x.b==11 && x.c==80);
  CHECK(y.a==13 && y.b==19 && y.c==288);
  RecordChoice *selected=&a;
  int evaluations=0;
  CHECK(record_total<(evaluations++ ,selected)>(2,m,m)==52.5 && evaluations==1);
  puts("PPC record varargs OK"); return 0;
}
