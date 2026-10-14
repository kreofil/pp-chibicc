typedef struct MFBase MFBase;

int MFValue<MFBase *x>();

int MFForwardUse(MFBase *x) {
  return MFValue<x>();
}

typedef struct MFPointer MFPointer;
int MFPointerValue<MFPointer *x>();
int MFPointerForwardUse(MFPointer *x) { return MFPointerValue<x>(); }
