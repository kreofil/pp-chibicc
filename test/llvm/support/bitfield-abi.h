#ifndef BITFIELD_ABI_H
#define BITFIELD_ABI_H
typedef struct { unsigned a:3; int b:5; _Bool c:1; unsigned :0; unsigned d:7; } Bits;
typedef struct { float f; unsigned bits:5; } FloatBits;
typedef struct { unsigned bits:5; double d; } MixedBits;
typedef struct { unsigned :32; double d; } PaddingBits;
typedef struct { double d; unsigned :3; } TailPaddingBits;
typedef struct __attribute__((packed)) { unsigned head:7; unsigned long value:64; } CrossBits;
typedef struct __attribute__((packed)) { char c; Bits bits; int ordinary; } NestedBits;
typedef union __attribute__((packed)) { unsigned :2; unsigned bits:3; } TinyBits;
typedef struct { unsigned long value:64; long signed_value:63; char bytes[9]; } LargeBits;
#define BF_DECL(P) \
  Bits P##_bits(Bits); \
  FloatBits P##_float(FloatBits); \
  MixedBits P##_mixed(MixedBits); \
  PaddingBits P##_padding(PaddingBits); \
  TailPaddingBits P##_tail(TailPaddingBits); \
  CrossBits P##_cross(CrossBits); \
  NestedBits P##_nested(NestedBits); \
  TinyBits P##_tiny(TinyBits); \
  LargeBits P##_large(LargeBits); \
  double P##_pressure(long,long,long,long,long,long,MixedBits,double); \
  double P##_rollback(double,double,double,double,double,double,double,double,MixedBits,Bits);
BF_DECL(host)
BF_DECL(emitted)
int host_bitfield_calls(void);
long host_bitfield_variadic(int count, ...);
#endif
