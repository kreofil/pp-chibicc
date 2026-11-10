#if defined(FAIL_UNCOVERED)
typedef struct Family {} case { int item; } Family;
int dispatch<Family *f>(int extra) = 0;
int main(void) { Family.item f = 1; return dispatch<&f>(7); }
#elif defined(FAIL_UNCOVERED_RECORD)
typedef struct Result { long a, b, c; } Result;
typedef struct Argument { long a, b; } Argument;
typedef struct Family {} case { int item; } Family;
Result dispatch<Family *f>(Argument extra) = 0;
int main(void) {
  Family.item f = 1;
  Argument argument = {7, 11};
  Result result = dispatch<&f>(argument);
  return result.a;
}
#elif defined(FAIL_INCOMPLETE)
typedef struct Family {} enum { item } const Family;
int dispatch<Family *f>(void) = 0;
int dispatch<Family.void *f>(void) { return 0; }
int main(void) { return 0; }
#endif
