# BSAT Competition Solver Makefile

# Compiler and flags
CC = cc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -pedantic
# Optimization flags for SAT solver performance
OPTFLAGS = -O3 -march=native -flto
DEBUGFLAGS = -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
PROFFLAGS = -pg -O2

# Directories
SRCDIR = src
INCDIR = include
MODE ?= release
OBJDIR = build/$(MODE)
BINDIR = bin
TESTDIR = tests

# Output binary
TARGET = $(BINDIR)/bsat

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINDIR = $(BINDIR)/tests/$(MODE)
TEST_BINARIES = $(patsubst $(TESTDIR)/test_%.c,$(TEST_BINDIR)/test_%,$(TEST_SOURCES))

# Build modes
MODE ?= release

ifeq ($(MODE),debug)
    CFLAGS += $(DEBUGFLAGS)
    TARGET := $(TARGET)_debug
else ifeq ($(MODE),diagnostic)
    CFLAGS += $(OPTFLAGS) -DBSAT_SEARCH_DIAGNOSTICS
    TARGET := $(TARGET)_diagnostic
else ifeq ($(MODE),tsan)
    CFLAGS += -g -O1 -fsanitize=thread
    TARGET := $(TARGET)_tsan
else ifeq ($(MODE),profile)
    CFLAGS += $(PROFFLAGS)
    TARGET := $(TARGET)_prof
else
    CFLAGS += $(OPTFLAGS)
endif

# Include paths
CFLAGS += -I$(INCDIR)

# Phony targets
.PHONY: all clean test tests benchmark help dirs check

# Use clang-format 21.1.8 for reproducible formatting.
CLANG_FORMAT ?= clang-format
FORMAT_SOURCES = $(shell find src include tests examples -type f \( -name '*.c' -o -name '*.h' \))
.PHONY: format format-check

# Default target
all: dirs $(TARGET)

format:
	$(CLANG_FORMAT) -i $(FORMAT_SOURCES)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SOURCES)

# Create necessary directories
dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Main solver binary
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | dirs
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Test binaries (match only test_* files to avoid conflict with main solver)
$(TEST_BINDIR)/test_%: $(TESTDIR)/test_%.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS))
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(CFLAGS) -o $@ $(filter %.c %.o,$^) -lm

# Build all tests
tests: dirs $(TEST_BINARIES)

# Run tests
test: tests
	@echo "Running unit tests..."
	@for test in $(TEST_BINARIES); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

# Run benchmarks
benchmark: $(TARGET)
	@echo "Running benchmarks..."
	python3 $(TESTDIR)/benchmark.py --solver bsat=$(TARGET) --help

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR) pgo_data

# Profile-Guided Optimization (PGO) targets
# Requires clang/LLVM instrumentation and llvm-profdata
.PHONY: pgo pgo-generate pgo-train pgo-merge pgo-use

# Full PGO build (generate, train, merge, use)
pgo: pgo-generate pgo-train pgo-merge pgo-use

# Step 1: Build with profiling instrumentation
pgo-generate: clean dirs
	@echo "Building with profile instrumentation..."
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/accounting.o $(SRCDIR)/accounting.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/bsat.o $(SRCDIR)/bsat.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/arena.o $(SRCDIR)/arena.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/dimacs.o $(SRCDIR)/dimacs.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/congruence.o $(SRCDIR)/congruence.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/elim.o $(SRCDIR)/elim.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/equiv.o $(SRCDIR)/equiv.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/factor.o $(SRCDIR)/factor.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/ipasir.o $(SRCDIR)/ipasir.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/local_search.o $(SRCDIR)/local_search.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/minimize.o $(SRCDIR)/minimize.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/main.o $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/solver.o $(SRCDIR)/solver.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/vmtf.o $(SRCDIR)/vmtf.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/watch.o $(SRCDIR)/watch.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -o $(BINDIR)/bsat_pgo_gen $(OBJECTS) -lm

# Step 2: Run training workload to collect profile data
pgo-train:
	@echo "Running training workload..."
	@rm -rf pgo_data && mkdir -p pgo_data
	@LLVM_PROFILE_FILE="pgo_data/bsat_%p.profraw" ; \
	export LLVM_PROFILE_FILE; \
	for f in ../../dataset/medium_tests/medium_suite/*.cnf; do \
		timeout 5s $(BINDIR)/bsat_pgo_gen "$$f" > /dev/null 2>&1 || true; \
	done
	@echo "Profile data collected."

# Step 3: Merge profile data (clang/LLVM)
pgo-merge:
	@echo "Merging profile data..."
	xcrun llvm-profdata merge -output=pgo_data/default.profdata pgo_data/*.profraw
	@echo "Profile data merged."

# Step 4: Rebuild with profile data
pgo-use: dirs
	@echo "Building with profile optimization..."
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/accounting.o $(SRCDIR)/accounting.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/bsat.o $(SRCDIR)/bsat.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/arena.o $(SRCDIR)/arena.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/dimacs.o $(SRCDIR)/dimacs.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/congruence.o $(SRCDIR)/congruence.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/elim.o $(SRCDIR)/elim.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/equiv.o $(SRCDIR)/equiv.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/factor.o $(SRCDIR)/factor.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/ipasir.o $(SRCDIR)/ipasir.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/local_search.o $(SRCDIR)/local_search.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/minimize.o $(SRCDIR)/minimize.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/main.o $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/solver.o $(SRCDIR)/solver.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/vmtf.o $(SRCDIR)/vmtf.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/watch.o $(SRCDIR)/watch.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -o $(TARGET) $(OBJECTS) -lm
	@echo "PGO build complete: $(TARGET)"

# Help message
help:
	@echo "BSAT Competition Solver - Build System"
	@echo ""
	@echo "Usage: make [target] [MODE=mode]"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the solver (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  test      - Build and run tests"
	@echo "  format    - Format C sources and headers (clang-format 21.1.8)"
	@echo "  format-check - Check C formatting without changing files"
	@echo "  benchmark - Run benchmarks"
	@echo "  pgo       - Full PGO build (generate + train + use)"
	@echo "  help      - Show this message"
	@echo ""
	@echo "Modes:"
	@echo "  release   - Optimized build (default)"
	@echo "  debug     - Debug build with sanitizers"
	@echo "  profile   - Profiling build"
	@echo "  diagnostic - Optimized build with opt-in detailed search counters"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make MODE=debug"
	@echo "  make test MODE=debug"
	@echo "  make benchmark"

# Header dependencies are emitted alongside each object.
-include $(OBJECTS:.o=.d)

check: all test
	python3 tests/validate.py --solver $(TARGET)

# Parse-only driver for loading benchmarks (does not run SAT search).
.PHONY: parse-benchmark
parse-benchmark: $(BINDIR)/parse_benchmark_$(MODE)

$(BINDIR)/parse_benchmark_$(MODE): $(TESTDIR)/parse_benchmark.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $< $(OBJDIR)/dimacs.o $(filter-out $(OBJDIR)/main.o $(OBJDIR)/dimacs.o,$(OBJECTS)) -lm

.PHONY: binary-watches-benchmark
binary-watches-benchmark: $(BINDIR)/binary_watches_benchmark_$(MODE)

$(BINDIR)/binary_watches_benchmark_$(MODE): $(TESTDIR)/benchmark_binary_watches.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $^ -lm

.PHONY: clause-locks-benchmark
clause-locks-benchmark: $(BINDIR)/clause_locks_benchmark_$(MODE)

$(BINDIR)/clause_locks_benchmark_$(MODE): $(TESTDIR)/benchmark_clause_locks.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $^ -lm

.PHONY: local-search-benchmark
local-search-benchmark: $(BINDIR)/local_search_benchmark_$(MODE)

$(BINDIR)/local_search_benchmark_$(MODE): $(TESTDIR)/benchmark_local_search.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Isolated BVE resolution benchmark (not part of normal solver builds).
ELIM_DRIVER = $(BINDIR)/elim_driver$(if $(filter release,$(MODE)),,_$(MODE))
elim-driver: dirs $(ELIM_DRIVER)

$(ELIM_DRIVER): $(TESTDIR)/elim_driver.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS))
	$(CC) $(CFLAGS) -o $@ $^ -lm

.PHONY: elim-driver

GC_DRIVER = $(BINDIR)/gc_driver$(if $(filter release,$(MODE)),,_$(MODE))
gc-driver: dirs $(GC_DRIVER)

$(GC_DRIVER): $(TESTDIR)/gc_driver.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS))
	$(CC) $(CFLAGS) -o $@ $^ -lm

.PHONY: gc-driver

# Allocation injection is confined to separate objects; normal binaries have no hook.
FAULT_OBJECTS = $(patsubst $(SRCDIR)/%.c,build/fault-$(MODE)/%.o,$(filter-out $(SRCDIR)/main.c,$(SOURCES)))
build/fault-$(MODE)/%.o: $(SRCDIR)/%.c $(TESTDIR)/fail_alloc.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -include $(TESTDIR)/fail_alloc.h -MMD -MP -c -o $@ $<
$(BINDIR)/fail_alloc_$(MODE): $(TESTDIR)/fail_alloc_driver.c $(FAULT_OBJECTS) | dirs
	$(CC) $(CFLAGS) -o $@ $^ -lm
fault-test: $(BINDIR)/fail_alloc_$(MODE)
	$<
.PHONY: fault-test
-include $(FAULT_OBJECTS:.o=.d)

# Coverage-guided fuzzers use isolated Clang/ASan/UBSan objects and allocator hooks.
FUZZ_CC ?= clang
FUZZ_ENGINE ?= -fsanitize=fuzzer
FUZZ_FLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -DDEBUG -g -O1 -I$(INCDIR) -fsanitize=fuzzer-no-link,address,undefined
FUZZ_OBJECTS := $(patsubst $(SRCDIR)/%.c,build/fuzz/%.o,$(filter-out $(SRCDIR)/main.c,$(SOURCES)))
build/fuzz/%.o: $(SRCDIR)/%.c $(TESTDIR)/fail_alloc.h
	@mkdir -p $(@D)
	$(FUZZ_CC) $(FUZZ_FLAGS) -include $(TESTDIR)/fail_alloc.h -MMD -MP -c -o $@ $<
$(BINDIR)/fuzz_%: $(TESTDIR)/fuzz_%.c $(TESTDIR)/fuzz_alloc.c $(FUZZ_OBJECTS) | dirs
	$(FUZZ_CC) $(FUZZ_FLAGS) -o $@ $^ $(FUZZ_ENGINE) -lm
fuzzers: $(BINDIR)/fuzz_api $(BINDIR)/fuzz_dimacs $(BINDIR)/fuzz_public
.PHONY: fuzzers
.SECONDARY: $(FUZZ_OBJECTS)
-include $(FUZZ_OBJECTS:.o=.d)

$(BINDIR)/incremental_benchmark_$(MODE): $(TESTDIR)/incremental_driver.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $^ -lm
incremental-benchmark: $(BINDIR)/incremental_benchmark_$(MODE)
.PHONY: incremental-benchmark

# Opaque ABI only: hide internal Solver symbols from the shared library.
LIB_OBJECTS = $(patsubst $(SRCDIR)/%.c,build/lib-$(MODE)/%.o,$(filter-out $(SRCDIR)/main.c,$(SOURCES)))
ifeq ($(shell uname -s),Darwin)
LIBRARY = $(BINDIR)/libbsat_$(MODE).dylib
SHARED_FLAGS = -dynamiclib -Wl,-install_name,@rpath/libbsat_$(MODE).dylib
else
LIBRARY = $(BINDIR)/libbsat_$(MODE).so
SHARED_FLAGS = -shared -Wl,-soname,libbsat_$(MODE).so
endif
build/lib-$(MODE)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -MMD -MP -c -o $@ $<
$(LIBRARY): $(LIB_OBJECTS) | dirs
	$(CC) $(CFLAGS) $(SHARED_FLAGS) -o $@ $^ -lm
$(BINDIR)/embedding_client_$(MODE): $(TESTDIR)/embedding_client.c $(LIBRARY)
	$(CC) $(CFLAGS) -pthread -o $@ $< -L$(BINDIR) -lbsat_$(MODE) -Wl,-rpath,$(abspath $(BINDIR))
embedding-test: $(BINDIR)/embedding_client_$(MODE)
	$<
shared: $(LIBRARY)
.PHONY: shared embedding-test
-include $(LIB_OBJECTS:.o=.d)

$(BINDIR)/embedding_soak_$(MODE): $(TESTDIR)/embedding_soak.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -L$(BINDIR) -lbsat_$(MODE) -Wl,-rpath,$(abspath $(BINDIR))
soak-test: $(BINDIR)/embedding_soak_$(MODE)
	$<
.PHONY: soak-test

# Staged installation has a stable ABI soname independent of build mode.
PREFIX ?= /usr/local
DESTDIR ?=
PACKAGE_VERSION := $(shell cat VERSION)
PACKAGE_DIR = build/package-$(MODE)
ifeq ($(shell uname -s),Darwin)
PACKAGE_LIBRARY = $(PACKAGE_DIR)/libbsat.1.dylib
PACKAGE_SHARED_FLAGS = -dynamiclib -Wl,-install_name,@rpath/libbsat.1.dylib
else
PACKAGE_LIBRARY = $(PACKAGE_DIR)/libbsat.so.1
PACKAGE_SHARED_FLAGS = -shared -Wl,-soname,libbsat.so.1
endif
$(PACKAGE_LIBRARY): $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PACKAGE_SHARED_FLAGS) -o $@ $^ -lm
install: all $(PACKAGE_LIBRARY)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/lib/pkgconfig" "$(DESTDIR)$(PREFIX)/include"
	install -d "$(DESTDIR)$(PREFIX)/share/doc/bsat"
	install -m 644 THIRD_PARTY_NOTICES "$(DESTDIR)$(PREFIX)/share/doc/bsat/THIRD_PARTY_NOTICES"
	install -m 755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/bsat"
	install -m 755 $(PACKAGE_LIBRARY) "$(DESTDIR)$(PREFIX)/lib/$(notdir $(PACKAGE_LIBRARY))"
	install -m 644 include/bsat.h "$(DESTDIR)$(PREFIX)/include/bsat.h"
	install -m 644 include/ipasir.h "$(DESTDIR)$(PREFIX)/include/ipasir.h"
ifeq ($(shell uname -s),Darwin)
	ln -sf libbsat.1.dylib "$(DESTDIR)$(PREFIX)/lib/libbsat.dylib"
else
	ln -sf libbsat.so.1 "$(DESTDIR)$(PREFIX)/lib/libbsat.so"
endif
	python3 tests/write_pkgconfig.py --template bsat.pc.in --prefix "$(PREFIX)" --version "$(PACKAGE_VERSION)" --output "$(DESTDIR)$(PREFIX)/lib/pkgconfig/bsat.pc"
package-test: all $(PACKAGE_LIBRARY)
	python3 tests/test_install.py --cc "$(CC)" --mode "$(MODE)"
.PHONY: install package-test

$(BINDIR)/query_certificate_driver_$(MODE): $(TESTDIR)/query_certificate_driver.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -L$(BINDIR) -lbsat_$(MODE) -Wl,-rpath,$(abspath $(BINDIR))
query-certificate-driver: $(BINDIR)/query_certificate_driver_$(MODE)
.PHONY: query-certificate-driver

$(BINDIR)/application_histories_$(MODE): $(TESTDIR)/application_histories.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -L$(BINDIR) -lbsat_$(MODE) -Wl,-rpath,$(abspath $(BINDIR))
application-histories: $(BINDIR)/application_histories_$(MODE)
.PHONY: application-histories

# Public-ABI service policy example (application code, not solver internals).
.PHONY: checkpoint-example
checkpoint-example: $(BINDIR)/checkpoint_service_$(MODE)
$(BINDIR)/checkpoint_service_$(MODE): examples/checkpoint_service.c examples/checkpoint_policy.h $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $< $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) -lm

$(BINDIR)/recovery_fault_$(MODE): $(TESTDIR)/recovery_fault_driver.c examples/recoverable_service.h $(FAULT_OBJECTS) | dirs
	$(CC) $(CFLAGS) -o $@ $< $(FAULT_OBJECTS) -lm
recovery-fault-test: $(BINDIR)/recovery_fault_$(MODE)
	$<
.PHONY: recovery-fault-test
$(TEST_BINDIR)/test_recovery_service: examples/recoverable_service.h

$(BINDIR)/recovery_supervisor_$(MODE): examples/recovery_supervisor.c examples/recoverable_service.h $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) | dirs
	$(CC) $(CFLAGS) -o $@ $< $(filter-out $(OBJDIR)/main.o,$(OBJECTS)) -lm
recovery-example: $(BINDIR)/recovery_supervisor_$(MODE)
.PHONY: recovery-example

# Separate test facade, never linked into LIBRARY or package targets.
DIAGNOSTIC_LIBRARY = $(subst libbsat_,libquery_diagnostics_,$(LIBRARY))
$(DIAGNOSTIC_LIBRARY): tests/query_diagnostics.c src/bsat.c $(filter-out build/lib-$(MODE)/bsat.o,$(LIB_OBJECTS)) | dirs
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden $(subst libbsat_,libquery_diagnostics_,$(SHARED_FLAGS)) -o $@ $< $(filter %.o,$^) -lm
query-diagnostics: $(DIAGNOSTIC_LIBRARY)
.PHONY: query-diagnostics
