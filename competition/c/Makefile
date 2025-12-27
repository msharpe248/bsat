# BSAT Competition Solver Makefile

# Compiler and flags
CC = cc
CFLAGS = -std=c11 -Wall -Wextra -pedantic
# Optimization flags for SAT solver performance
OPTFLAGS = -O3 -march=native -flto
DEBUGFLAGS = -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
PROFFLAGS = -pg -O2

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = build
BINDIR = bin
TESTDIR = tests

# Output binary
TARGET = $(BINDIR)/bsat

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINARIES = $(patsubst $(TESTDIR)/test_%.c,$(BINDIR)/test_%,$(TEST_SOURCES))

# Build modes
MODE ?= release

ifeq ($(MODE),debug)
    CFLAGS += $(DEBUGFLAGS)
    TARGET := $(TARGET)_debug
else ifeq ($(MODE),profile)
    CFLAGS += $(PROFFLAGS)
    TARGET := $(TARGET)_prof
else
    CFLAGS += $(OPTFLAGS)
endif

# Include paths
CFLAGS += -I$(INCDIR)

# Phony targets
.PHONY: all clean test benchmark help dirs

# Default target
all: dirs $(TARGET)

# Create necessary directories
dirs:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Main solver binary
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Test binaries (match only test_* files to avoid conflict with main solver)
$(BINDIR)/test_%: $(TESTDIR)/test_%.c $(filter-out $(OBJDIR)/main.o,$(OBJECTS))
	$(CC) $(CFLAGS) -o $@ $^ -lm

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
	@$(TESTDIR)/benchmark.sh $(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR) pgo_data

# Profile-Guided Optimization (PGO) targets
# Works with both clang (LLVM) and gcc
.PHONY: pgo pgo-generate pgo-train pgo-merge pgo-use

# Full PGO build (generate, train, merge, use)
pgo: pgo-generate pgo-train pgo-merge pgo-use

# Step 1: Build with profiling instrumentation
pgo-generate: clean dirs
	@echo "Building with profile instrumentation..."
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/arena.o $(SRCDIR)/arena.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/dimacs.o $(SRCDIR)/dimacs.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/main.o $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/solver.o $(SRCDIR)/solver.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -c -o $(OBJDIR)/watch.o $(SRCDIR)/watch.c
	$(CC) $(CFLAGS) -O3 -march=native -fprofile-instr-generate -o $(BINDIR)/bsat_pgo_gen $(OBJECTS) -lm

# Step 2: Run training workload to collect profile data
pgo-train:
	@echo "Running training workload..."
	@rm -rf pgo_data && mkdir -p pgo_data
	@LLVM_PROFILE_FILE="pgo_data/bsat_%p.profraw" ; \
	export LLVM_PROFILE_FILE; \
	for f in /Users/msharpe/python/bsat/dataset/medium_tests/medium_suite/*.cnf; do \
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
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/arena.o $(SRCDIR)/arena.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/dimacs.o $(SRCDIR)/dimacs.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/main.o $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -O3 -march=native -flto -fprofile-instr-use=pgo_data/default.profdata -c -o $(OBJDIR)/solver.o $(SRCDIR)/solver.c
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
	@echo "  benchmark - Run benchmarks"
	@echo "  pgo       - Full PGO build (generate + train + use)"
	@echo "  help      - Show this message"
	@echo ""
	@echo "Modes:"
	@echo "  release   - Optimized build (default)"
	@echo "  debug     - Debug build with sanitizers"
	@echo "  profile   - Profiling build"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make MODE=debug"
	@echo "  make test MODE=debug"
	@echo "  make benchmark"

# Dependencies (auto-generated)
-include $(OBJECTS:.o=.d)

# Generate dependencies
$(OBJDIR)/%.d: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@$(CC) -MM $(CFLAGS) $< | sed 's,\($*\)\.o[ :]*,$(OBJDIR)/\1.o $@ : ,g' > $@