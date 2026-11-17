#include <stdio.h>

int zero;
extern int zero;
int counter = 4;
static char message[] = "globals OK";
int *counter_pointer = &counter;
extern int counter;
extern int counter;
extern int *counter_pointer;

int main(void) {
  if (zero != 0 || counter != 4 || *counter_pointer != 4 ||
      counter_pointer != &counter)
    return 1;
  zero = *counter_pointer + 3;
  if (zero != 7 || message[7] != ' ')
    return 2;
  puts(message);
  return 0;
}
