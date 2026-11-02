typedef struct MFBase MFBase;

int MFValue<MFBase *x>();

int MFForwardUse(MFBase *x) {
  return MFValue<x>();
}

typedef struct MFPointer MFPointer;
int MFPointerValue<MFPointer *x>();
int MFPointerForwardUse(MFPointer *x) { return MFPointerValue<x>(); }

typedef struct MFUnion MFUnion;
int MFUnionValue<MFUnion *x>();
int MFUnionForwardUse(MFUnion *x) { return MFUnionValue<x>(); }
