// VR_FN is this module, VR_PEER is the separately compiled list consumer.
long VR_FN(ints)(int n,...) {
  va_list ap; va_start(ap,n);
  long sum=0;
  for(int i=0;i<n;i++) { VrInt v=va_arg(ap,VrInt); sum+=(i+1)*(v.a+v.b); }
  va_end(ap); return sum;
}
double VR_FN(reals)(int n,...) {
  va_list ap; va_start(ap,n);
  double sum=0;
  for(int i=0;i<n;i++) { VrReal v=va_arg(ap,VrReal); sum+=(i+1)*(v.a+v.b); }
  va_end(ap); return sum;
}
double VR_FN(mixed)(int n,...) {
  va_list ap; va_start(ap,n);
  double sum=0;
  for(int i=0;i<n;i++) { VrMixed v=va_arg(ap,VrMixed); sum+=(i+1)*(v.a+v.b); }
  va_end(ap); return sum;
}
int VR_FN(shapes)(int n,...) {
  va_list ap; va_start(ap,n);
  VrBytes bytes=va_arg(ap,VrBytes);
  VrFloats floats=va_arg(ap,VrFloats);
  VrReverse reverse=va_arg(ap,VrReverse);
  VrUnion u=va_arg(ap,VrUnion);
  VrBits bits=va_arg(ap,VrBits);
  VrNested nested=va_arg(ap,VrNested);
  int marker=va_arg(ap,int);
  va_end(ap);
  return bytes.a==11 && bytes.b==22 && bytes.c==33 && floats.a==1.25f &&
    floats.b==2.5f && reverse.a==3.75 && reverse.b==-5000000000L &&
    u.bits==0xfedcba9876543210UL && bits.a==7 && bits.b==-3 && bits.c==1 &&
    nested.inner.a==17 && nested.inner.b==29 && nested.real==4.5 && marker==n;
}
int VR_FN(bits)(int n,...) {
  va_list ap; va_start(ap,n);
  VrReal value=va_arg(ap,VrReal);
  union { double real; unsigned long bits; } a,b;
  a.real=value.a; b.real=value.b;
  va_end(ap); return a.bits==0x7ff8000000001234UL && b.bits==0x8000000000000000UL;
}
long VR_FN(gp_boundary)(long a,long b,long c,long d,long e,...) {
  va_list ap; va_start(ap,e);
  VrInt v=va_arg(ap,VrInt); long tail=va_arg(ap,long);
  va_end(ap); return a+b+c+d+e+v.a+v.b+tail;
}
double VR_FN(fp_boundary)(double a,double b,double c,double d,double e,double f,
                          double g,int marker,...) {
  va_list ap; va_start(ap,marker);
  VrReal v=va_arg(ap,VrReal); double tail=va_arg(ap,double);
  va_end(ap); return a+b+c+d+e+f+g+marker+v.a+v.b+tail;
}
double VR_FN(gp_rollback)(long a,long b,long c,long d,long e,long f,...) {
  va_list ap; va_start(ap,f);
  VrMixed spill=va_arg(ap,VrMixed);
  VrReal regs=va_arg(ap,VrReal);
  long tail=va_arg(ap,long);
  va_end(ap); return a+b+c+d+e+f+spill.a+spill.b+regs.a+regs.b+tail;
}
long VR_FN(fp_rollback)(double a,double b,double c,double d,double e,double f,
                        double g,double h,...) {
  va_list ap; va_start(ap,h);
  VrMixed spill=va_arg(ap,VrMixed);
  VrInt regs=va_arg(ap,VrInt);
  int tail=va_arg(ap,int);
  va_end(ap); return (long)(a+b+c+d+e+f+g+h+spill.a+spill.b)+regs.a+regs.b+tail;
}
double VR_FN(named)(VrLarge large,VrInt pair,double real,int n,...) {
  va_list ap; va_start(ap,n);
  VrInt a=va_arg(ap,VrInt); VrReal b=va_arg(ap,VrReal);
  va_end(ap); return large.a+large.b+large.c+pair.a+pair.b+real+n+a.a+a.b+b.a+b.b;
}
VrInt VR_FN(return_int)(int n,...) {
  va_list ap; va_start(ap,n);
  VrInt value=va_arg(ap,VrInt); value.a+=n;
  va_end(ap); return value;
}
VrLarge VR_FN(return_large)(long a,long b,long c,long d,long e,...) {
  va_list ap; va_start(ap,e);
  VrInt pair=va_arg(ap,VrInt); VrLarge value=va_arg(ap,VrLarge);
  double tail=va_arg(ap,double);
  value.a+=a+b+c+d+e; value.b+=pair.a+pair.b; value.c+=(long)tail;
  va_end(ap); return value;
}
double VR_FN(consume_mixed)(va_list ap,int n) {
  double sum=0;
  for(int i=0;i<n;i++) { VrMixed v=va_arg(ap,VrMixed); sum+=(i+1)*(v.a+v.b); }
  return sum;
}
int VR_FN(copies)(int n,...) {
  va_list ap,copy,restart,lists[2];
  va_start(ap,n);
  VrMixed first=va_arg(ap,VrMixed);
  va_copy(copy,ap);
  double a=VR_FN(consume_mixed)(ap,n-1);
  double b=VR_FN(consume_mixed)(copy,n-1);
  va_start(restart,n);
  double c=VR_FN(consume_mixed)(restart,n);
  va_start(lists[0],n); va_copy(lists[1],lists[0]);
  int index=0;
  VrMixed once=va_arg(lists[index++],VrMixed);
  VrMixed untouched=va_arg(lists[1],VrMixed);
  va_end(lists[1]); va_end(lists[0]); va_end(restart); va_end(copy); va_end(ap);
  return first.a==7 && first.b==2.5 && a==b && a==9.5*(n-1)*n/2 &&
    c==9.5*n*(n+1)/2 && index==1 && once.a==7 && untouched.b==2.5;
}
double VR_FN(consume)(va_list ap) {
  VrFloats f=va_arg(ap,VrFloats);
  VrMixed m=va_arg(ap,VrMixed);
  VrLarge big=va_arg(ap,VrLarge);
  int marker=va_arg(ap,int);
  return f.a+f.b+m.a+m.b+big.a+big.b+big.c+marker;
}
double VR_FN(bridge)(int n,...) {
  va_list ap; va_start(ap,n);
  double value=VR_PEER(consume)(ap);
  long tail=va_arg(ap,long);
  va_end(ap); return value+tail+n;
}
long VR_FN(loop)(int n,...) {
  long sum=0;
  for(int i=0;i<n;i++) {
    va_list ap; va_start(ap,n);
    sum+=va_arg(ap,VrInt).a;
    va_end(ap);
  }
  return sum;
}
int VR_FN(extended)(int n,...) {
  va_list ap; va_start(ap,n);
  VrPacked packed=va_arg(ap,VrPacked);
  VrAligned aligned=va_arg(ap,VrAligned);
  VrX87 x87=va_arg(ap,VrX87);
  VrFloat3 floats=va_arg(ap,VrFloat3);
  VrWideUnion wide=va_arg(ap,VrWideUnion);
  VrPadding padding=va_arg(ap,VrPadding);
  VrCrossBits cross=va_arg(ap,VrCrossBits);
  int tail=va_arg(ap,int);
  va_end(ap);
  return packed.tag==41 && packed.real==3.5 && aligned.a==7 && aligned.b==11 &&
    aligned.c==13 && x87.real==1.0L+0x1p-60L && floats.a==1.25f && floats.b==2.5f &&
    floats.c==3.75f && wide.bits[0]==0xfedcba9876543210UL && wide.bits[1]==0x123456789abcdef0UL &&
    padding.real==6.25 && cross.head==93 && cross.value==0x0123456789abcdefUL && tail==n;
}
