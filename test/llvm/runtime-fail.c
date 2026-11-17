#if defined(FAIL_UNCOVERED)
typedef struct Family {} case { int item; } Family;
int dispatch<Family *f>(int extra) = 0;
int main(void) { Family.item f = 1; return dispatch<&f>(7); }
#elif defined(FAIL_UNCOVERED_MULTI)
typedef struct Closed {} enum { one } const Closed;
typedef struct Open {} enum { one, two } Open;
int dispatch<Closed *a, Open *b>(int extra) = 0;
int main(void) { Closed a = Closed.one; Open b = Open.two; return dispatch<&a, &b>(7); }
#elif defined(FAIL_UNCOVERED_FLOAT)
typedef struct Family {} case { double item; } Family;
double dispatch<Family *f>(double extra) = 0;
int main(void) { Family.item f = 1.5; return (int)dispatch<&f>(2.5); }
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
#elif defined(FAIL_UNCOVERED_REAL_RECORD)
typedef struct Result { long id; double a, b; } Result;
typedef struct Argument { double a, b; } Argument;
typedef struct Family {} case { double item; } Family;
Result dispatch<Family *f>(Argument extra) = 0;
int main(void) {
  Family.item f = 1.5;
  Argument argument = {7.25, 11.5};
  Result result = dispatch<&f>(argument);
  return result.id;
}
#elif defined(FAIL_INCOMPLETE)
typedef struct Family {} enum { item } const Family;
int dispatch<Family *f>(void) = 0;
int dispatch<Family.void *f>(void) { return 0; }
int main(void) { return 0; }
#endif
