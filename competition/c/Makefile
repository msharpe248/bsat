# Makefile for CDCL SAT Solver (C implementation)
# Optimized for macOS with Clang

CC = clang
CFLAGS = -std=c11 -Wall -Wextra -O3 -march=native -Iinclude
DEBUGFLAGS = -std=c11 -Wall -Wextra -g -O0 -Iinclude

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Source files
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/dimacs.c \
          $(SRC_DIR)/solver.c \
          $(SRC_DIR)/clause.c \
          $(SRC_DIR)/propagate.c \
          $(SRC_DIR)/analyze.c \
          $(SRC_DIR)/vsids.c \
          $(SRC_DIR)/memory.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Binary name
TARGET = $(BIN_DIR)/cdcl_solver

# Default target
all: $(TARGET)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Link
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Debug build
debug: CFLAGS = $(DEBUGFLAGS)
debug: clean $(TARGET)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Test on simple instances
test: $(TARGET)
	@echo "Testing on simple instances..."
	@for file in ../../dataset/simple_tests/simple_suite/*.cnf; do \
		echo "Testing $$file"; \
		./$(TARGET) "$$file"; \
	done

# Phony targets
.PHONY: all clean debug test
