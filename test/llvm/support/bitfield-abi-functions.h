Bits BF_FN(bits)(Bits v) { v.a+=3; v.b-=2; v.c=!v.c; v.d^=7; return v; }
FloatBits BF_FN(float)(FloatBits v) { v.f+=1.25f; v.bits^=7; return v; }
MixedBits BF_FN(mixed)(MixedBits v) { v.bits+=3; v.d*=2; return v; }
PaddingBits BF_FN(padding)(PaddingBits v) { v.d+=1.5; return v; }
TailPaddingBits BF_FN(tail)(TailPaddingBits v) { v.d-=1.5; return v; }
CrossBits BF_FN(cross)(CrossBits v) { v.head+=2; v.value^=0xfedcba9876543210UL; return v; }
NestedBits BF_FN(nested)(NestedBits v) {
  v.c+=1; v.bits=BF_FN(bits)(v.bits); v.ordinary+=3; return v;
}
TinyBits BF_FN(tiny)(TinyBits v) { v.bits+=3; return v; }
LargeBits BF_FN(large)(LargeBits v) {
  v.value^=0xffffffffffffffffUL; v.signed_value-=7;
  v.bytes[0]+=1; v.bytes[8]+=3; return v;
}
double BF_FN(pressure)(long a,long b,long c,long d,long e,long f,MixedBits v,double last) {
  return a+b+c+d+e+f+v.bits+v.d+last;
}
double BF_FN(rollback)(double a,double b,double c,double d,double e,double f,double g,
                       double h,MixedBits spill,Bits tail) {
  return a+b+c+d+e+f+g+h+spill.bits+spill.d+tail.a+tail.b+tail.d;
}
