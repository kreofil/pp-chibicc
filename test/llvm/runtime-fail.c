#if defined(FAIL_UNCOVERED)
typedef struct Family {} case { int item; } Family;
int dispatch<Family *f>(int extra) = 0;
int main(void) { Family.item f = 1; return dispatch<&f>(7); }
#elif defined(FAIL_INCOMPLETE)
typedef struct Family {} enum { item } const Family;
int dispatch<Family *f>(void) = 0;
int dispatch<Family.void *f>(void) { return 0; }
int main(void) { return 0; }
#endif
