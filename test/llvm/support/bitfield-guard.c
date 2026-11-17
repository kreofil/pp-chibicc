// Linux x86-64 guard pages detect loads/stores outside a packed object.
void *mmap(void *, unsigned long, int, int, int, long);
int mprotect(void *, unsigned long, int);
int munmap(void *, unsigned long);
long sysconf(int);
int puts(const char *);
struct __attribute__((packed)) TinyGuard { unsigned long value:1; };
struct __attribute__((packed)) CrossGuard { unsigned head:7; unsigned long value:64; };
static unsigned long read_tiny(struct TinyGuard *p) { return p->value; }
static unsigned long read_cross(struct CrossGuard *p) { return p->value; }
static void write_tiny(struct TinyGuard *p, unsigned long v) { p->value=v; }
static void write_cross(struct CrossGuard *p, unsigned long v) { p->value=v; }
int main(void) {
  long page=sysconf(30); // _SC_PAGESIZE on Linux
  if (page<4096) return 1;
  char *area=mmap(0,page*3,0,0x22,-1,0); // MAP_PRIVATE | MAP_ANONYMOUS
  if (area==(void *)-1 || mprotect(area+page,page,3)) return 2;
  for (int side=0;side<2;side++) {
    struct TinyGuard *tiny=(void *)(side ? area+2*page-sizeof(*tiny) : area+page);
    *(unsigned char *)tiny=0xaa;
    write_tiny(tiny,1);
    if (read_tiny(tiny)!=1 || *(unsigned char *)tiny!=0xab) return 3;
    struct CrossGuard *cross=(void *)(side ? area+2*page-sizeof(*cross) : area+page);
    cross->head=93;
    write_cross(cross,0xfedcba9876543210UL);
    if (read_cross(cross)!=0xfedcba9876543210UL || cross->head!=93) return 4;
  }
  if (munmap(area,page*3)) return 5;
  puts("bitfield guard OK");
  return 0;
}
