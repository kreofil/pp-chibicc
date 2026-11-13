#include "api.h"
#include "eworld.h"
#include <stdlib.h>

typedef struct LifeContext {
  EWorld world;
  Map initial;
  unsigned population;
  unsigned seed;
  unsigned long fingerprint;
} LifeContext;

static unsigned next_number(unsigned *state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

void *life_prepare(unsigned mode, unsigned seed) {
  if (mode > 5) return NULL;
  LifeContext *context = calloc(1, sizeof(LifeContext));
  if (!context) return NULL;
  int height = mode == 4 ? 1 : mode == 5 ? 3 : mode < 2 ? 16 : mode == 2 ? 32 : 64;
  int width = mode == 5 ? 5 : height;
  context->seed = seed;
  context->initial.height = height;
  context->initial.width = width;
  unsigned state = seed;
  unsigned long fingerprint = 14695981039346656037UL;
  int shift_x = seed % width, shift_y = (seed >> 8) % height;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int live;
      int py = (y + height - shift_y) % height;
      int px = (x + width - shift_x) % width;
      if (mode == 0)
        live = py == height / 2 && px >= width / 2 - 1 && px <= width / 2 + 1;
      else if (mode == 1)
        live = (py == 0 && px == 1) || (py == 1 && px == 2) || (py == 2 && px <= 2);
      else
        live = (next_number(&state) >> 24) < 72;
      if (live) init_spec(Cell.filled, &context->initial.plane[y][x]);
      else init_spec(Cell.empty, &context->initial.plane[y][x]);
      context->population += live;
      fingerprint = (fingerprint ^ (unsigned)live) * 1099511628211UL;
    }
  }
  context->fingerprint = fingerprint;
  return context;
}

void life_release(void *context) { free(context); }
unsigned long life_fingerprint(void *context) { return ((LifeContext *)context)->fingerprint; }
unsigned life_width(void *context) { return ((LifeContext *)context)->initial.width; }
unsigned life_height(void *context) { return ((LifeContext *)context)->initial.height; }

// The benchmark runner marks this function noinline in LLVM IR. The native
// backend does not inline. The original simulation functions remain available
// for normal optimization inside this round.
unsigned long life_run(void *pointer, unsigned phase, unsigned steps, unsigned *population) {
  LifeContext *context = pointer;
  EWorld *world = &context->world;
  world->map01 = context->initial;
  world->map02 = context->initial;
  world->currentMap = &world->map01;
  world->nextMap = &world->map02;
  world->step = 0;
  world->cells = context->population;
  unsigned width = context->initial.width, height = context->initial.height;
  unsigned index = ((context->seed >> 8) + (phase & 7) * 17) % (width * height);
  Cell *changed = &world->map01.plane[index / width][index % width];
  int was_live = get_spec_var_index(changed) == get_subtype_index(Cell.filled);
  int live = phase & 1;
  if (live) init_spec(Cell.filled, changed);
  else init_spec(Cell.empty, changed);
  world->cells += live - was_live;
  for (unsigned step = 0; step < steps; ++step) {
    LifeNextState(world);
    MapExchange(world);
  }
  unsigned total = 0;
  unsigned long checksum = 14695981039346656037UL;
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      int filled = get_spec_var_index(&world->currentMap->plane[y][x]) == get_subtype_index(Cell.filled);
      total += filled;
      checksum = (checksum ^ (unsigned)filled) * 1099511628211UL;
    }
  }
  if (world->step != steps || world->cells != total) abort();
  *population = total;
  return checksum;
}
