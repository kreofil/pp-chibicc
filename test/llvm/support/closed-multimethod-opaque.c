struct MultiLeft;
struct MultiRight;
int multi_remote<struct MultiLeft *a, struct MultiRight *b>(int x) = 0;
int multi_opaque(struct MultiLeft *a, struct MultiRight *b) { return multi_remote<a, b>(7); }
