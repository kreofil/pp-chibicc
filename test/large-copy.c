#include <stdio.h>

// Include an odd tail and guard bytes; packed members deliberately put the
// source and destination at addresses unsuitable for aligned wide loads.
typedef struct LargeCopy { unsigned char bytes[257]; } LargeCopy;
typedef struct __attribute__((packed)) GuardedCopy {
  unsigned char before;
  LargeCopy value;
  unsigned char after;
} GuardedCopy;
typedef union LargeUnion { unsigned char bytes[259]; long alignment; } LargeUnion;

static int valid(LargeCopy *value, unsigned seed) {
  for (unsigned i = 0; i < 257; ++i)
    if (value->bytes[i] != (unsigned char)(seed + i * 37)) return 0;
  return 1;
}

static unsigned long consume(LargeCopy value, long a, long b, long c, long d,
                             long e, long f, long g, long h) {
  if (!valid(&value, 19)) return 0;
  return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

static LargeCopy returned(LargeCopy value) {
  value.bytes[256] = 99;
  return value;
}

int main(void) {
  GuardedCopy source, destination, chained;
  source.before = 11; source.after = 12;
  destination.before = 21; destination.after = 22;
  chained.before = 31; chained.after = 32;
  for (unsigned i = 0; i < 257; ++i)
    source.value.bytes[i] = (unsigned char)(19 + i * 37);
  destination.value = source.value;
  if (!valid(&destination.value, 19)) return 1;
  if (source.before != 11 || source.after != 12 || destination.before != 21 || destination.after != 22) return 2;
  source.value = source.value;
  if (!valid(&source.value, 19)) return 3;
  chained.value = destination.value = source.value;
  if (!valid(&chained.value, 19) || chained.before != 31 || chained.after != 32) return 4;
  if ((destination.value = source.value).bytes[256] != (unsigned char)(19 + 256 * 37)) return 5;
  if (consume(destination.value = source.value, 1, 2, 3, 4, 5, 6, 7, 8) != 204) return 6;
  LargeCopy result = returned(source.value);
  for (unsigned i = 0; i < 256; ++i)
    if (result.bytes[i] != (unsigned char)(19 + i * 37)) return 7;
  if (result.bytes[256] != 99 || !valid(&source.value, 19)) return 8;
  LargeUnion ua, ub;
  for (unsigned i = 0; i < sizeof(ua.bytes); ++i) ua.bytes[i] = (unsigned char)(i * 13 + 7);
  ub = ua;
  for (unsigned i = 0; i < sizeof(ub.bytes); ++i)
    if (ub.bytes[i] != (unsigned char)(i * 13 + 7)) return 9;
  puts("large copy OK");
  return 0;
}
