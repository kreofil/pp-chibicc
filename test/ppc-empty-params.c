#include <stdio.h>

typedef struct Item { int common; } case { int number; } Item;

// Interchange () and (void) between separate declarations and handlers.
int read_item<Item *p>();
int read_item<Item *p>(void) { return p->common; }
int read_item<Item.number *p>() { return p->common + p->number; }

int read_reverse<Item *p>(void);
int read_reverse<Item *p>() { return p->common; }
int read_reverse<Item.number *p>(void) { return p->number; }

int combine_items<Item *a, Item *b>();
int combine_items<Item *a, Item *b>() { return a->common + b->common; }
int combine_items<Item.number *a, Item.number *b>(void) {
  return a->number + b->number;
}

typedef struct { long a, b, c; } Result;
Result get_result<Item *p>();
Result get_result<Item *p>() { return (Result){p->common, 0, 0}; }
Result get_result<Item.number *p>(void) {
  return (Result){p->common, p->number, 5000000000L};
}

Item.number global_item = {7, 11};

typedef struct Choice { int base; } enum { one } const Choice;
int choose_empty<Choice *c>() = 0;
int choose_empty<Choice.void *c>(void) { return c->base; }
int choose_empty<Choice.one *c>() { return c->base + 1; }

int main() {
  Item base = {3};
  Item.number first = {4, 5};
  Item.number second = {6, 8};
  Item *pointer = &first;
  if (read_item<&base>() != 3 || read_item<pointer>() != 9 ||
      read_item<&global_item>() != 18) return 1;
  if (read_reverse<&base>() != 3 || read_reverse<pointer>() != 5) return 2;
  if (combine_items<&first, &second>() != 13 ||
      combine_items<&first, &base>() != 7) return 3;
  Result result = get_result<pointer>();
  if (result.a != 4 || result.b != 5 || result.c != 5000000000L) return 4;
  Choice.one choice = {2};
  if (choose_empty<&choice>() != 3) return 5;
  puts("PPC empty parameters OK");
  return 0;
}
