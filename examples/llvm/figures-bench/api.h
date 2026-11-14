#ifndef LLVM_FIGURES_BENCH_API_H
#define LLVM_FIGURES_BENCH_API_H
int figures_prepare(int count);
int figures_add(int kind, int a, int b, int c);
unsigned long figures_process(void);
int figures_valid(void);
void figures_destroy(void);
#endif
