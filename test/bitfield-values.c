int puts(const char *);
int printf(const char *, ...);
#define CHECK(x) do { if (!(x)) { printf("bitfield failure: %d\n", __LINE__); return 1; } } while (0)

struct Values {
  unsigned u:3;
  int s:5;
  _Bool flag:1;
  unsigned :7;
  unsigned :0;
  unsigned long u31:31, u32:32, u33:33;
  long s32:32, s33:33;
  unsigned long u63:63, u64:64;
  long s63:63, s64:64;
};
struct __attribute__((packed)) Cross {
  unsigned prefix:7;
  unsigned long value:64;
  int signed_value:31;
  unsigned suffix:2;
  char tail;
};
struct Padding { unsigned :3; char a; unsigned :0; char b; };
union __attribute__((packed)) Tiny { unsigned :2; unsigned value:3; };
static struct Padding padding = {17, 29};
static union Tiny tiny = {6};
static struct Values global = {.s=-13, .flag=512, .u33=0x1ffffffffUL,
  .u64=0xfedcba9876543210UL, .s63=-1234567890123L, .s64=-9223372036854775807L};
static struct Cross packed = {91, 0xfedcba9876543210UL, -7654321, 3, 77};
static struct Values *once(struct Values *v, int *calls) { ++*calls; return v; }

int main(void) {
  CHECK(sizeof(struct Padding)==5 && _Alignof(struct Padding)==1);
  CHECK(padding.a==17 && padding.b==29);
  CHECK(sizeof(union Tiny)==1 && _Alignof(union Tiny)==1 && tiny.value==6);
  CHECK(sizeof(struct Cross)==14 && _Alignof(struct Cross)==1);
  CHECK(global.s==-13 && global.flag==1 && global.u33==0x1ffffffffUL);
  CHECK(global.u64==0xfedcba9876543210UL && global.s63==-1234567890123L);
  CHECK(global.s64==-9223372036854775807L);
  CHECK(packed.prefix==91 && packed.value==0xfedcba9876543210UL);
  CHECK(packed.signed_value==-7654321 && packed.suffix==3 && packed.tail==77);
  union Tiny local_tiny = {5};
  union { unsigned :3; } empty = {};
  struct Padding local_padding = {31, 47};
  CHECK(local_tiny.value==5 && sizeof(empty)==1);
  CHECK(local_padding.a==31 && local_padding.b==47);

  struct Values v = {};
  unsigned input = 31;
  CHECK((v.u=input)==7 && v.u==7);
  CHECK(v.u++==7 && v.u==0);
  CHECK(v.u--==0 && v.u==7);
  CHECK(++v.u==0 && --v.u==7);
  CHECK((v.u+=3)==2 && (v.u*=7)==6 && (v.u^=7)==1);
  int signed_input = 31;
  CHECK((v.s=signed_input)==-1 && v.s==-1);
  v.s=15;
  CHECK(v.s++==15 && v.s==-16);
  CHECK(v.s--==-16 && v.s==15);
  CHECK(++v.s==-16 && --v.s==15);
  CHECK((v.s+=2)==-15 && (v.s>>=1)==-8);
  CHECK((v.flag=256)==1 && v.flag==1);
  CHECK((v.flag=0.25)==1 && v.flag==1);
  CHECK((v.flag=0)==0 && v.flag==0);
  int calls=0;
  CHECK(once(&v,&calls)->u++==1 && calls==1 && v.u==2);
  CHECK((once(&v,&calls)->u+=7)==1 && calls==2 && v.u==1);
  CHECK(sizeof(+v.u)==4 && sizeof(+v.u31)==4 && sizeof(+v.u32)==4);
  CHECK(_Generic(+v.u, int:1, default:0));
  CHECK(_Generic(+v.u32, unsigned int:1, default:0));
  CHECK(_Generic(+v.s32, int:1, default:0));
  CHECK(-1 < v.u && (v.u-2)==-1 && (v.u>>1)==0 && ~v.u==-2);
  v.u31=0x7fffffffU; v.u32=0xffffffffU; v.s32=-1;
  CHECK(v.u31==0x7fffffffU && v.u32==0xffffffffU && v.s32==-1);
  CHECK((v.u32>>31)==1 && (v.s32>>31)==-1);
  v.u33=0x1ffffffffUL; v.s33=-0x100000000L;
  CHECK(v.u33==0x1ffffffffUL && v.s33==-0x100000000L);
  CHECK(++v.u33==0 && --v.u33==0x1ffffffffUL);
  v.u63=0x7fffffffffffffffUL; v.u64=0xffffffffffffffffUL;
  CHECK(v.u63++==0x7fffffffffffffffUL && v.u63==0);
  CHECK(v.u64++==0xffffffffffffffffUL && v.u64==0);
  v.s63=0x3fffffffffffffffL;
  CHECK(v.s63++==0x3fffffffffffffffL && v.s63==-0x4000000000000000L);
  CHECK(v.s63--==-0x4000000000000000L && v.s63==0x3fffffffffffffffL);
  v.s64=-0x7000000000000000L;
  CHECK(v.s64==-0x7000000000000000L);

  unsigned long state=0x123456789abcdef0UL;
  for (int i=0; i<1000; i++) {
    state=state*6364136223846793005UL+1;
    packed.value=state;
    packed.signed_value=(int)(state & 0x3fffffffUL)-0x20000000;
    CHECK(packed.value==state);
    CHECK(packed.signed_value==(int)(state & 0x3fffffffUL)-0x20000000);
    CHECK(packed.prefix==91 && packed.suffix==3 && packed.tail==77);
    packed.value^=0xffffffffffffffffUL;
    CHECK(packed.value==~state);
  }
  switch (v.u) { case 1: break; default: return 2; }
  puts("bitfield values OK");
  return 0;
}
