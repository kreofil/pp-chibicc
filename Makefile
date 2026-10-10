CFLAGS=-std=c11 -g -fno-common -Wall -Wno-switch

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
PPC_MULTIFILE_DIR=test/ppc-multifile
PPC_MULTIFILE_OBJS=$(PPC_MULTIFILE_DIR)/base.o $(PPC_MULTIFILE_DIR)/spec.o $(PPC_MULTIFILE_DIR)/spec2.o $(PPC_MULTIFILE_DIR)/forward-use.o $(PPC_MULTIFILE_DIR)/main.o
PPC_MULTIFILE_TEST=$(PPC_MULTIFILE_DIR)/ppc-multifile.exe
# ADD
STAGE2_TESTS=$(TEST_SRCS:test/%.c=stage2/test/%.exe)
STAGE2_PPC_MULTIFILE_DIR=stage2/$(PPC_MULTIFILE_DIR)
STAGE2_PPC_MULTIFILE_OBJS=$(PPC_MULTIFILE_OBJS:$(PPC_MULTIFILE_DIR)/%.o=$(STAGE2_PPC_MULTIFILE_DIR)/%.o)
STAGE2_PPC_MULTIFILE_TEST=$(STAGE2_PPC_MULTIFILE_DIR)/ppc-multifile.exe
# ---
TESTS=$(TEST_SRCS:.c=.exe) $(PPC_MULTIFILE_TEST)

# Stage 1

chibicc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): chibicc.h

test/%.exe: chibicc test/%.c
	./chibicc -Iinclude -Itest -c -o test/$*.o test/$*.c
	$(CC) -pthread -o $@ test/$*.o -xc test/common

$(PPC_MULTIFILE_DIR)/%.o: chibicc $(PPC_MULTIFILE_DIR)/%.c
	./chibicc -Iinclude -Itest -I$(PPC_MULTIFILE_DIR) -c -o $@ $(PPC_MULTIFILE_DIR)/$*.c

$(PPC_MULTIFILE_TEST): $(PPC_MULTIFILE_OBJS)
	$(CC) -pthread -o $@ $^ -xc test/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh ./chibicc

test-all: test test-stage2 test-examples

# Stage 2

stage2/chibicc: $(OBJS:%=stage2/%)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

stage2/%.o: chibicc %.c
	mkdir -p stage2/test
	./chibicc -c -o $(@D)/$*.o $*.c

stage2/test/%.exe: stage2/chibicc test/%.c
	mkdir -p stage2/test
	./stage2/chibicc -Iinclude -Itest -c -o stage2/test/$*.o test/$*.c
	$(CC) -pthread -o $@ stage2/test/$*.o -xc test/common

#test-stage2: $(TESTS:test/%=stage2/test/%)
test-stage2: $(STAGE2_TESTS) $(STAGE2_PPC_MULTIFILE_TEST)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh ./stage2/chibicc

# ADD
$(STAGE2_PPC_MULTIFILE_DIR)/%.o: stage2/chibicc $(PPC_MULTIFILE_DIR)/%.c
	mkdir -p $(@D)
	./stage2/chibicc -Iinclude -Itest -I$(PPC_MULTIFILE_DIR) -c -o $@ $(PPC_MULTIFILE_DIR)/$*.c

$(STAGE2_PPC_MULTIFILE_TEST): $(STAGE2_PPC_MULTIFILE_OBJS)
	$(CC) -pthread -o $@ $^ -xc test/common


FIGURE_EVO_REUSE_STEPS = \
	examples/evolution/figures/evo-reuse-code/01-start \
	examples/evolution/figures/evo-reuse-code/02-new-spec \
	examples/evolution/figures/evo-reuse-code/03-new-func \
	examples/evolution/figures/evo-reuse-code/04-new-field \
	examples/evolution/figures/evo-reuse-code/05-rect-only-out \
	examples/evolution/figures/evo-reuse-code/06-mm \
	examples/evolution/figures/evo-reuse-code/07-mm-new-spec \
	examples/evolution/figures/evo-reuse-code/08-more-gen

test-examples: chibicc
	@set -e; \
	for dir in $(FIGURE_EVO_REUSE_STEPS); do \
		echo "testing $$dir"; \
		name=$$(basename $$dir); \
		out=/tmp/ppc-example-$$name; \
		result=/tmp/ppc-example-$$name.out; \
		srcdir=$$dir/src; \
		files=$$(awk '/set\(SOURCE_FILES/{flag=1; next} /^\)/{flag=0} flag {print}' $$srcdir/CMakeLists.txt | sed 's/#.*//' | tr -s '[:space:]' '\n' | grep '\.c$$' | sed "s|^|$$srcdir/|"); \
		incs="-Iinclude"; \
		for d in examples/evolution/figures/evo-reuse-code/*/src; do incs="$$incs -I$$d"; done; \
		./chibicc $$incs -o $$out $$files; \
		$$out $$dir/data/input1.txt $$result >/tmp/ppc-example-$$name.stdout; \
		diff -u $$dir/data/output1.txt $$result; \
	done

# test-all: test test-stage2 test-examples

# Misc.



clean:
	rm -rf chibicc tmp* $(TESTS) test/*.s test/*.exe stage2
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.PHONY: test clean test-stage2
