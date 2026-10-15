// Отработка вариантов использования перечислений на примере дней недели

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct WeekDay {int week_number;} enum {
  Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday
} const WeekDay;

// Печать текущего дня недели
void printWeekDay<WeekDay* w>() {
  printf("Unknown Week Day {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Monday* w>() {
  printf("It is Monday {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Tuesday* w>() {
  printf("It is Tuesday {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Wednesday* w>() {
  printf("It is Wednesday {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Thursday* w>() {
  printf("It is Thursday {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Friday* w>() {
  printf("It is Friday {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Saturday* w>() {
  printf("It is Saturday {%d}\n", w->week_number);
}
void printWeekDay<WeekDay.Sunday* w>() {
  printf("It is Sunday {%d}\n", w->week_number);
}


int main() {
  // Объявление переменной для текущей даты.
  WeekDay           zero      = {0};
  WeekDay.Monday    monday    = {1};
  WeekDay.Tuesday   tuesday   = {2};
  WeekDay.Wednesday wednesday = {3};
  WeekDay.Thursday  thursday  = {3};
  WeekDay.Friday    friday    = {5};
  WeekDay.Saturday  saturday  = {6};
  WeekDay.Sunday    sunday    = {7};

  printf("-- 1st step --\n\n");
  int N = 10;
  WeekDay days2[N];
  // WeekDay days2[10];
  // init_spec(WeekDay.Sunday, days2+1, N-2);
  for(int i = 0; i < N; ++i) {
    printf("%d. ", i);
    days2[i].week_number = i;
    printWeekDay<&days2[i]>();
  }

  printf("\n-- 2nd step --\n");
  WeekDay *days3 = create_spec(WeekDay.Saturday, N);
  for(int i = 0; i < 10; ++i) {
    printf("%d. ", i);
    printWeekDay<&days3[i]>();
  }

  printf("\n-- 3rd step --\n");

  init_spec(WeekDay.Monday, days3+3, N-7);
  for(int i = 0; i < 10; ++i) {
    printf("%d. ", i);
    printWeekDay<&days3[i]>();
  }

  printf("%d\n", get_subtype_index(WeekDay));
  printf("%d\n", get_subtype_index(WeekDay.Sunday));
  printf("%d\n", get_spec_var_index(&days3[0]));
  printf("%d\n", get_spec_var_index(days3+5));

  printf("\n-- 4th step --\n");
  // WeekDay days4[N];
  WeekDay days4[10];
  printf("size = %d\n", sizeof(days4));

  // init_spec(WeekDay.Friday, &days4[N-5], N-7);
  for(int i = 0; i < 10; ++i) {
    printf("%d. ", i);
    printWeekDay<&days4[i]>();
  }


  free(days3);
  return 0;
}
