#ifndef LLVM_RECORD_H
#define LLVM_RECORD_H
struct Inner { short label; int items[2]; };
struct Record {
  char tag;
  struct Inner inner;
  long total;
  int *pointer;
};
long emitted_record_value(struct Record *record);
int host_check_record(struct Record *record);
#endif
