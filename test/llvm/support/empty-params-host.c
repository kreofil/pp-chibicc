// Fixed host prototypes interoperate with emitted definitions using ().
int empty_int(void);
signed char empty_char(void);
_Bool empty_bool(void);
float empty_float(void);
double empty_double(void);
int *empty_pointer(void);

int host_checks_empty(void) {
  return empty_int() == 42 && empty_char() == -7 && empty_bool() &&
         empty_float() == 1.25f && empty_double() == 2.5 && *empty_pointer() == 17;
}
double host_without_prototype(double f, int c, int u) { return f + c + u; }
