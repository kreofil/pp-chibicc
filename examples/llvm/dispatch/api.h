#ifndef LLVM_DISPATCH_API_H
#define LLVM_DISPATCH_API_H
double probe_known(int count);
double probe_closed(int count);
double probe_multi(int count);
double probe_open(int count);
double probe_control(int count);
double extra_probe(int count);
#endif
