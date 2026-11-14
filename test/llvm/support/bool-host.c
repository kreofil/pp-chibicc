#include <stdbool.h>
typedef struct BoolRecord { _Bool flag; int value; } BoolRecord;
_Bool bool_emitted_flip(_Bool);
BoolRecord bool_emitted_record(BoolRecord);
_Bool bool_emitted_many(_Bool, _Bool, _Bool, _Bool, _Bool, _Bool, _Bool, _Bool, _Bool);

_Bool host_bool_flip(_Bool value) { return !value; }
BoolRecord host_bool_record(BoolRecord value) {
  value.flag = !value.flag;
  value.value += 7;
  return value;
}
_Bool host_bool_many(_Bool a, _Bool b, _Bool c, _Bool d, _Bool e,
                     _Bool f, _Bool g, _Bool h, _Bool i) {
  return a && !b && c && !d && e && !f && g && !h && i;
}
int host_bool_checks(void) {
  _Bool (*callback)(_Bool) = bool_emitted_flip;
  BoolRecord value = bool_emitted_record((BoolRecord){true, 9});
  return callback(false) && !callback(true) && !value.flag && value.value == 16 &&
         bool_emitted_many(true, false, true, false, true, false, true, false, true);
}
