#ifndef VARARG_RECORD_PPC_H
#define VARARG_RECORD_PPC_H
#include "../../stdarg-records.h"
typedef struct MixedRecordChoice { int bias; } case { int extra; } const MixedRecordChoice;
long mixed_record_total<MixedRecordChoice *v>(int n,...);
#endif
