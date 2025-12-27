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
	rm -rf $(OBJDIR) $(BINDIR)

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