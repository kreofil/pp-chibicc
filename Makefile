BUILD_DIR ?= build
CMAKE ?= cmake

.PHONY: all configure build test test-stage2 test-examples test-all clean distclean rebuild

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

test: configure
	$(CMAKE) --build $(BUILD_DIR) --target test

test-stage2: configure
	$(CMAKE) --build $(BUILD_DIR) --target test-stage2

test-examples: configure
	$(CMAKE) --build $(BUILD_DIR) --target test-examples

test-all: configure
	$(CMAKE) --build $(BUILD_DIR) --target test-all

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

distclean: clean
	$(CMAKE) -E rm -rf bin

rebuild:
	$(MAKE) clean
	$(MAKE) build
