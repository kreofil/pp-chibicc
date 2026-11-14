#include <stdio.h>

typedef struct Options {
  unsigned priority:3;
  unsigned enabled:1;
  int adjustment:4;
} Options;

typedef struct Job { Options options; } case { int extra; } const Job;

int score<Job *job>() {
  return job->options.enabled ? job->options.priority+job->options.adjustment : 0;
}
int score<Job.extra *job>() {
  return job->options.enabled ? job->options.priority+job->options.adjustment+job->extra : 0;
}

int main(void) {
  Job normal={{5,1,-2}};
  Job.extra extra={{7,1,-1},10};
  printf("normal=%d extra=%d\n",score<&normal>(),score<&extra>());
  ++normal.options.priority;
  extra.options.enabled=0;
  printf("normal=%d extra=%d\n",score<&normal>(),score<&extra>());
  return 0;
}
