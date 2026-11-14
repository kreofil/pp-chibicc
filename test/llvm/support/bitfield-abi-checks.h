static int check_bitfield_calls(void) {
  Bits b={6,-7,1,91};
  Bits out=BF_CHECK(bits)(b);
  if (out.a!=1 || out.b!=-9 || out.c!=0 || out.d!=(91^7) || b.a!=6) return 1;
  FloatBits fb={2.5f,17};
  FloatBits fo=BF_CHECK(float)(fb);
  if (fo.f!=3.75f || fo.bits!=(17^7)) return 2;
  MixedBits mb={7,11.5};
  MixedBits mo=BF_CHECK(mixed)(mb);
  if (mo.bits!=10 || mo.d!=23) return 3;
  PaddingBits pb={2.5};
  TailPaddingBits tb={4.5};
  if (BF_CHECK(padding)(pb).d!=4 || BF_CHECK(tail)(tb).d!=3) return 4;
  CrossBits cb={93,0x0123456789abcdefUL};
  CrossBits co=BF_CHECK(cross)(cb);
  if (co.head!=95 || co.value!=0xffffffffffffffffUL || cb.head!=93) return 5;
  NestedBits nb={41,{6,-7,1,91},99};
  NestedBits no=BF_CHECK(nested)(nb);
  if (no.c!=42 || no.bits.a!=1 || no.bits.b!=-9 || no.bits.d!=(91^7) || no.ordinary!=102) return 6;
  TinyBits tiny={7};
  if (BF_CHECK(tiny)(tiny).bits!=2 || sizeof(tiny)!=1) return 7;
  LargeBits lb={0x0123456789abcdefUL,-1234567890123L,{1,2,3,4,5,6,7,8,9}};
  LargeBits lo=BF_CHECK(large)(lb);
  if (lo.value!=0xfedcba9876543210UL || lo.signed_value!=-1234567890130L ||
      lo.bytes[0]!=2 || lo.bytes[8]!=12 || lb.bytes[0]!=1) return 8;
  if (BF_CHECK(pressure)(1,2,3,4,5,6,mb,13.5)!=53) return 9;
  if (BF_CHECK(rollback)(1,2,3,4,5,6,7,8,mb,b)!=144.5) return 10;
  CrossBits (*callback)(CrossBits)=BF_CHECK(cross);
  if (callback(cb).value!=0xffffffffffffffffUL) return 11;
  Bits array[2]={b,out};
  if (BF_CHECK(bits)(array[1]).a!=4 || array[0].b!=-7) return 12;
  return 0;
}
