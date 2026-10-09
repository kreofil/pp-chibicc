typedef struct MFBase MFBase;

int MFValue<MFBase *x>();

int MFForwardUse(MFBase *x) {
  return MFValue<x>();
}
