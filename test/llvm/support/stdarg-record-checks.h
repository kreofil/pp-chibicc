static int check_record_varargs(void) {
  VrInt ints={7,11}; VrReal reals={1.5,2.5}; VrMixed mixed={7,2.5};
  if(VR_CHECK(ints)(0)!=0 || VR_CHECK(ints)(9,ints,ints,ints,ints,ints,ints,ints,ints,ints)!=810) return 1;
  if(VR_CHECK(reals)(9,reals,reals,reals,reals,reals,reals,reals,reals,reals)!=180) return 2;
  if(VR_CHECK(mixed)(8,mixed,mixed,mixed,mixed,mixed,mixed,mixed,mixed)!=342) return 3;
  VrBytes bytes={11,22,33}; VrFloats floats={1.25f,2.5f};
  VrReverse reverse={3.75,-5000000000L}; VrUnion u={.bits=0xfedcba9876543210UL};
  VrBits bits={7,-3,1}; VrNested nested={{17,29},4.5};
  if(!VR_CHECK(shapes)(91,bytes,floats,reverse,u,bits,nested,91)) return 4;
  if(VR_CHECK(gp_boundary)(1,2,3,4,5,ints,19L)!=52) return 5;
  if(VR_CHECK(fp_boundary)(1,2,3,4,5,6,7,11,reals,13.5)!=56.5) return 6;
  if(VR_CHECK(gp_rollback)(1,2,3,4,5,6,mixed,reals,19L)!=53.5) return 7;
  if(VR_CHECK(fp_rollback)(1,2,3,4,5,6,7,8,mixed,ints,23)!=86) return 8;
  VrLarge big={3,5,7};
  if(VR_CHECK(named)(big,ints,1.5,9,ints,reals)!=65.5) return 9;
  VrInt ri=VR_CHECK(return_int)(5,ints);
  if(ri.a!=12 || ri.b!=11 || ints.a!=7) return 10;
  VrLarge rl=VR_CHECK(return_large)(1,2,3,4,5,ints,big,9.5);
  if(rl.a!=18 || rl.b!=23 || rl.c!=16 || big.a!=3) return 11;
  if(!VR_CHECK(copies)(8,mixed,mixed,mixed,mixed,mixed,mixed,mixed,mixed)) return 12;
  if(VR_CHECK(bridge)(11,floats,mixed,big,17,19L)!=75.25) return 13;
  long (*indirect)(int,...)=VR_CHECK(ints);
  if(indirect(3,ints,ints,ints)!=108) return 14;
  // One million evaluations of the same va_arg must reuse its entry buffer.
  if(VR_CHECK(loop)(1000000,ints)!=7000000) return 15;
  union { double real; unsigned long bits; } nan;
  nan.bits=0x7ff8000000001234UL;
  VrReal exact_bits={nan.real,-0.0};
  if(!VR_CHECK(bits)(1,exact_bits)) return 17;
#ifndef VR_NATIVE_SAFE
  // Native retains its existing packed/X87/over-aligned/padding ABI limits.
  VrPacked packed={41,3.5}; VrAligned aligned={7,11,13};
  VrX87 x87={1.0L+0x1p-60L}; VrFloat3 f3={1.25f,2.5f,3.75f};
  VrWideUnion wide={.bits={0xfedcba9876543210UL,0x123456789abcdef0UL}};
  VrPadding padding={6.25};
  VrCrossBits cross={93,0x0123456789abcdefUL};
  if(!VR_CHECK(extended)(37,packed,aligned,x87,f3,wide,padding,cross,37)) return 16;
#endif
  return 0;
}
