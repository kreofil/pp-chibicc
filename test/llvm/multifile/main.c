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
  Report previous = {0, 0, 1};
  Report rectangle_report = report<objects[0]>(previous);
  Report square_report = report<objects[1]>(previous);
  if (rectangle_report.id != 7 || rectangle_report.perimeter != 14 ||
      rectangle_report.count != 11 || square_report.id != 9 ||
      square_report.perimeter != 20 || square_report.count != 2 || previous.count != 1)
    return 3;
  Dimensions extra = {0.5, 1.25};
  Measurement rectangle_measure = measure<objects[0]>(extra);
  Measurement square_measure = measure<objects[1]>(extra);
  Measurement zero_measure = measure<&zero>(extra);
  if (rectangle_measure.id != 7 || rectangle_measure.area != 18.375 ||
      square_measure.id != 9 || square_measure.area != 1.75 ||
      zero_measure.id != 11 || zero_measure.area != 1.75 || extra.width != 0.5)
    return 4;
  if (variable_sum<objects[0]>(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) != 757 ||
      variable_sum<objects[1]>(12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) != 659 ||
      variable_sum<&zero>(0) != 11)
    return 5;
  if (extended<objects[0]>(1.25L) != 13.25L ||
      extended<objects[1]>(2.5L) != 11.5L || extended<&zero>(0.75L) != 11.75L)
    return 6;
  MFClosedChoice choice = MFClosedChoice.first;
  choice.id = 5;
  MFClosedUnion.second closed_union = {2.5};
  MFClosedPointer.second closed_pointer = {0};
  MFClosedCase.first closed_case = {7};
  if (closed_sizes() != 12 || get_spec_var_index(&closed_union) != 2 ||
      get_spec_var_index(&closed_pointer) != 2 || get_spec_var_index(&closed_case) != 1)
    return 7;
  if (closed_pick<&choice>() != 16 || closed_opaque(&choice) != 16 ||
      closed_pair<&choice, &closed_union>() != 32)
    return 8;
  choice = MFClosedChoice.second;
  if (closed_pick<&choice>() != 17 || closed_opaque(&choice) != 17 ||
      closed_pair<&choice, &closed_union>() != 20)
    return 9;
  puts("PPC multifile OK");
  return 0;
}
