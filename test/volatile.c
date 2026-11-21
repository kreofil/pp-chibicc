#include <stdio.h>
#include <stdarg.h>
#define CHECK(x) do { if (!(x)) { printf("volatile failure: %d\n", __LINE__); return 1; } } while (0)

static volatile int counter;
static volatile unsigned char byte;
static volatile short small;
static volatile long wide;
static volatile _Bool ready;
static volatile float real32;
static volatile double real64;
static volatile long double real80;
typedef int Matrix[2][3];
static volatile Matrix matrix;
struct Pair { int x, y; };
struct Big { long x[5]; };
struct Mixed { volatile int x; int y; };
struct Bits { volatile unsigned x:3; unsigned y:5; };
static volatile struct Pair pair;
static volatile struct Big big;
static struct Mixed mixed;
static volatile struct { struct { int x; }; int a[2]; } nested;
static volatile union { int x; float y; } choice;
typedef volatile struct Forward Forward;
static Forward *forward_ptr;
struct Forward { int x; };
static volatile union ForwardUnion *forward_union_ptr;
union ForwardUnion { int x; long y; };
int volatile_forward(void) { return forward_ptr->x; }
int volatile_forward_union(void) { return forward_union_ptr->x; }

int volatile_sum(volatile int *p) { return *p + *p; }
int volatile_local_init(int n) { volatile int value=n; return value; }
int volatile_typeof_value(volatile int *p) { typeof(*p+0) value=3; return value; }
int volatile_typeof_assignment(volatile int *p) { typeof(*p=7) value=3; return value; }
int volatile_typeof_cast(void) { typeof((volatile int)0) value=3; return value; }
int volatile_typeof_lvalue(volatile int *p) { typeof(*p) value=3; return value; }
int volatile_write(volatile int *p, int n) { return (*p = n); }
int volatile_update(volatile int *p) { return ++*p; }
int volatile_post(volatile int *p) { return (*p)++; }
int volatile_discard(volatile int *p) { *p; (void)*p; return 0; }
int volatile_address(volatile int *p) { return sizeof(*p) + (&*p == p); }
int volatile_poll(volatile int *p) { while (!*p) {} return *p; }
void volatile_sequence(volatile int *p) { *p=1; *p=2; *p; *p=3; }
int pointer_to_volatile(int *p) { return *(volatile int *)p; }
int volatile_pointer(int *volatile p) { return *p + *p; }
int volatile_both(volatile int *volatile p) { return *p + *p; }
int volatile_conditional(int c, int *p, volatile int *q) { return *(c ? p : q); }
int volatile_matrix(volatile Matrix p) { return p[1][2]; }
int volatile_bound(int n, int p[volatile n]) { return p[0] + p[0]; }
int volatile_vla(int n, volatile int p[n]) { return p[n-1]; }
int volatile_member(volatile struct Pair *p) { return p->x+p->x; }
int plain_member(struct Pair *p) { return p->x+p->x; }
int volatile_global_matrix(void) { return matrix[0][0]; }
struct Pointer { int *p; };
int volatile_member_pointer(volatile struct Pointer *p) { return *p->p; }
void volatile_copy_to(volatile struct Pair *d, struct Pair *s) { *d=*s; }
void volatile_copy_from(struct Pair *d, volatile struct Pair *s) { *d=*s; }
void plain_copy(struct Pair *d, struct Pair *s) { *d=*s; }
void volatile_discard_pair(volatile struct Pair *p) { *p; (void)*p; }
void volatile_discard_comma(volatile struct Pair *p) { (0,*p); }
void volatile_discard_cond(int n, volatile struct Pair *p, volatile struct Pair *q) { n?*p:*q; }
void volatile_discard_stmt(volatile struct Pair *p) { ({ *p; }); }
void volatile_loop_step(volatile struct Pair *p, int n) { for (int i=0;i<n;i++,*p) {} }
unsigned volatile_bitfield_read(volatile struct Bits *p) { return p->x; }
unsigned volatile_bitfield_write(volatile struct Bits *p, unsigned n) { return p->x=n; }
int pair_sum(struct Pair p) { return p.x+p.y; }
struct Pair pair_return(void) { return pair; }
struct Big big_return(void) { return big; }
int mixed_sum(struct Mixed p) { return p.x+p.y; }
int vararg_sum(int n, ...) {
  va_list ap; va_start(ap,n);
  struct Mixed p=va_arg(ap,struct Mixed);
  va_end(ap); return p.x+p.y;
}
int side_effects;
volatile int *evaluated(void) { side_effects++; return &counter; }

int main(void) {
  Forward f={43}; forward_ptr=&f;
  union ForwardUnion fu={47}; forward_union_ptr=&fu;
  CHECK(volatile_forward()==43 && sizeof(Forward)==4);
  CHECK(volatile_forward_union()==47);
  volatile int local=3;
  CHECK(volatile_local_init(53)==53);
  CHECK(volatile_sum(&local)==6);
  CHECK(volatile_write(&local,7)==7);
  CHECK(volatile_update(&local)==8 && local==8);
  CHECK(volatile_post(&local)==8 && local==9);
  CHECK(volatile_discard(&local)==0 && volatile_address(&local)==5);
  CHECK(volatile_poll(&local)==9);
  int ordinary=5;
  CHECK(pointer_to_volatile(&ordinary)==5);
  CHECK(volatile_pointer(&ordinary)==10 && volatile_both(&ordinary)==10);
  CHECK(volatile_conditional(1,&ordinary,&local)==5);
  CHECK(volatile_conditional(0,&ordinary,&local)==9);
  int values[2]={11,13};
  CHECK(volatile_bound(2,values)==22 && volatile_vla(2,values)==13);
  volatile Matrix m={{1,2,3},{4,5,6}};
  CHECK(volatile_matrix(m)==6);
  matrix[1][2]=17; CHECK(matrix[1][2]==17);
  typedef int Row[3];
  int n=3; typedef Row Dynamic[n];
  volatile Dynamic dynamic;
  dynamic[2][1]=19; CHECK(dynamic[2][1]==19);
  byte=255; small=-123; wide=0x1234567890L; ready=3;
  CHECK(byte==255 && small==-123 && wide==0x1234567890L && ready==1);
  real32=1.5f; real64=2.25; real80=3.5L;
  CHECK(real32==1.5f && real64==2.25 && real80==3.5L);
  counter=10; (*evaluated())+=2;
  CHECK(counter==12 && side_effects==1);
  CHECK((*evaluated())++==12 && counter==13 && side_effects==2);
  CHECK((counter=21)==21 && (counter+=2)==23);
  CHECK((counter, counter)==23);
  pair.x=4; pair.y=7;
  struct Pair p=pair; CHECK(p.x==4 && pair_sum(pair)==11);
  p=pair_return(); CHECK(p.x==4 && p.y==7);
  pair=p; pair; (void)pair;
  big.x[0]=23; big.x[4]=29;
  struct Big b=big_return(); CHECK(b.x[0]==23 && b.x[4]==29);
  mixed.x=8; mixed.y=9;
  CHECK(mixed_sum(mixed)==17 && vararg_sum(1,mixed)==17);
  nested.x=31; nested.a[1]=37;
  CHECK(nested.x==31 && nested.a[1]==37);
  choice.x=41; CHECK(choice.x==41);
  struct Bits bits={7,23};
  CHECK(bits.x++==7 && bits.x==0 && bits.y==23);
  CHECK(++bits.x==1 && (bits.x+=9)==2 && bits.y==23);
  volatile struct Bits vb={6,19};
  vb.x++; CHECK(vb.x==7 && vb.y==19);
  puts("volatile OK");
  return 0;
}
