#include "test.h"

typedef struct VLAWeekDay { int week_number; } enum {
  Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday
} const VLAWeekDay;

static int bound_calls;

static int runtime_bound(int n) {
  bound_calls++;
  return n;
}

int code<VLAWeekDay *w>() { return 0; }
int code<VLAWeekDay.Monday *w>() { return 1; }
int code<VLAWeekDay.Saturday *w>() { return 6; }
int code<VLAWeekDay.Sunday *w>() { return 7; }

int main() {
  int n = 10;
  VLAWeekDay days[n];

  for (int i = 0; i < n; i++) {
    days[i].week_number = i;
    ASSERT(0, get_spec_var_index(&days[i]));
    ASSERT(0, code<&days[i]>());
  }

  int sunday = get_subtype_index(VLAWeekDay.Sunday);
  init_spec(VLAWeekDay.Sunday, days + 1, n - 2);
  ASSERT(0, get_spec_var_index(&days[0]));
  for (int i = 1; i < n - 1; i++) {
    ASSERT(sunday, get_spec_var_index(&days[i]));
    ASSERT(7, code<&days[i]>());
  }
  ASSERT(0, get_spec_var_index(&days[n - 1]));

  int monday = get_subtype_index(VLAWeekDay.Monday);
  VLAWeekDay.Monday mondays[n - 3];
  for (int i = 0; i < n - 3; i++) {
    ASSERT(monday, get_spec_var_index(&mondays[i]));
    ASSERT(1, code<&mondays[i]>());
  }

  int rows = 3;
  int cols = 4;
  VLAWeekDay matrix[rows][cols];
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++)
      ASSERT(0, get_spec_var_index(&matrix[r][c]));

  bound_calls = 0;
  VLAWeekDay.Saturday saturdays[runtime_bound(5)];
  ASSERT(1, bound_calls);
  for (int i = 0; i < 5; i++) {
    ASSERT(get_subtype_index(VLAWeekDay.Saturday),
           get_spec_var_index(&saturdays[i]));
    ASSERT(6, code<&saturdays[i]>());
  }
  ASSERT(1, bound_calls);

  printf("OK\n");
  return 0;
}
