#include <stdio.h>
#define CHECK(x) do { if (!(x)) { printf("argument failure: %d\n", __LINE__); return 1; } } while (0)
typedef struct Choice {} enum { bad, good } const Choice;
int pick<Choice *p>() { return 0; }
int pick<Choice.bad *p>() { return 1; }
int pick<Choice.good *p>() { return 2; }
Choice *identity<Choice *p>() { return p; }
int pair<Choice *a, Choice *b>(int n) { return 10*pick<a>()+pick<b>()+n; }
static int evaluations;
static Choice *evaluated(Choice *p) { evaluations++; return p; }
static Choice *choose(int n, Choice *a, Choice *b) { return n ? a : b; }
int main(void) {
  Choice.bad bad;
  Choice.good good;
  Choice zero;
  Choice *a=(Choice *)&bad, *b=(Choice *)&good, *z=&zero;
  Choice *values[3]={a,b,z};
  Choice sequence[2];
  sequence[0]=Choice.bad;
  sequence[1]=Choice.good;
  CHECK(pick<sequence+1>()==2);
  for (int n=0;n<3;n++) {
    int expected=n==0?1:n==1?2:0;
    CHECK(pick<n==0? &bad:(n==1? &good:&zero)>()==expected);
    CHECK(pick<(n==0? &bad:(n==1? &good:&zero))>()==expected);
    CHECK(pick<n==0? a:n==1? b:z>()==expected);
    CHECK(pick<values+n ? values[n] : z>()==expected);
  }
  int n=0;
  CHECK(pick<values[n++]>()==1 && n==1);
  CHECK(pick<evaluated(n==1?a:b)>()==1 && evaluations==1);
  CHECK(pick<identity<n==1?a:b>()>()==1);
  CHECK(pick<choose(n>0,a,b)>()==1);
  CHECK(pick<choose(n>=1,a,b)>()==1);
  CHECK(pick<n<2?a:b>()==1);
  CHECK(pick<n>=1?a:b>()==1);
  CHECK(pick<(n>0)?a:b>()==1);
  CHECK(pick<n && !0 ? a:b>()==1);
  CHECK(pick<(n<<1)==2?a:b>()==1);
  CHECK(pick<n>>1 ? b:a>()==1);
  Choice *saved=0;
  CHECK(pick<saved=a>()==1 && saved==a);
  CHECK((pick<n ? evaluations++,a : b>()==1 && evaluations==2));
  CHECK(pick<(evaluations++,b)>()==2 && evaluations==3);
  CHECK((pair<n==1?a:b,n==1?b:a>(n>0))==13);
  CHECK((pair<identity<a>(),identity<b>()>(n>0))==13);
  struct { int pick; } member={0};
  CHECK(pick<member.pick<1 ? a:b>()==1);
  CHECK(pick<((Choice *[]){a,b})[1]>()==2);
  puts("PPC argument expressions OK");
  return 0;
}
