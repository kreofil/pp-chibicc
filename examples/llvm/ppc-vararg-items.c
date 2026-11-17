#include <stdio.h>
#include <stdarg.h>
typedef struct Item { double value; int weight; } Item;
typedef struct Mode {} case { int factor; } const Mode;
static double sum_items(va_list ap,int count) {
  double sum=0;
  for(int i=0;i<count;i++) { Item item=va_arg(ap,Item); sum+=item.value*item.weight; }
  return sum;
}
double total<Mode *mode>(int count,...) {
  va_list ap; va_start(ap,count);
  double sum=sum_items(ap,count); va_end(ap); return sum;
}
double total<Mode.factor *mode>(int count,...) {
  va_list ap; va_start(ap,count);
  double sum=sum_items(ap,count); va_end(ap); return mode->factor*sum;
}
int main(void) {
  Mode normal={}; Mode.factor scaled={3};
  Item a={1.5,2},b={2.5,4};
  printf("normal=%.1f scaled=%.1f\n",total<&normal>(2,a,b),total<&scaled>(2,a,b));
  return 0;
}
