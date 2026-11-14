#include "../../stdarg-records.h"
#define VR_FN(name) host_##name
#define VR_PEER(name) emitted_##name
#include "stdarg-record-functions.h"
#define VR_CHECK(name) emitted_##name
#include "stdarg-record-checks.h"
int host_checks_record_varargs(void) { return check_record_varargs(); }
