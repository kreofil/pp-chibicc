#include "rectangle.h"
#include "square.h"
int puts(const char *s);
Figure.rectangle global_rectangle = {7, {3, 4}};

int main(void) {
  Figure.square square = {9, 5};
  Figure zero = {11};
  Figure *objects[2] = {&global_rectangle, &square};
  if (perimeter<objects[0]>() != 14 || perimeter<objects[1]>() != 20 ||
      perimeter<&zero>() != 11 || pair<objects[0], objects[1]>(8) != 16)
    return 1;
  if (get_spec_size(Figure) != 3 ||
      get_spec_var_index(objects[0]) != get_subtype_index(Figure.rectangle) ||
      get_spec_var_index(objects[1]) != get_subtype_index(Figure.square))
    return 2;
  puts("PPC multifile OK");
  return 0;
}
