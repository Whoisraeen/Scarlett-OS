# ScarlettOS Test Suite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I..
LDFLAGS =

# Test framework
TEST_FRAMEWORK = test_framework.c

# Unit tests
UNIT_TESTS = unit/test_memory unit/test_scheduler

# Integration tests
INTEGRATION_TESTS = integration/test_ipc

# System tests
SYSTEM_TESTS = system/test_boot

ALL_TESTS = $(UNIT_TESTS) $(INTEGRATION_TESTS) $(SYSTEM_TESTS)

.PHONY: all clean run

all: $(ALL_TESTS)

# Build unit tests
unit/%: unit/%.c $(TEST_FRAMEWORK)
	$(CC) $(CFLAGS) $< $(TEST_FRAMEWORK) -o $@ $(LDFLAGS)

# Build integration tests
integration/%: integration/%.c $(TEST_FRAMEWORK)
	$(CC) $(CFLAGS) $< $(TEST_FRAMEWORK) -o $@ $(LDFLAGS)

# Build system tests
system/%: system/%.c $(TEST_FRAMEWORK)
	$(CC) $(CFLAGS) $< $(TEST_FRAMEWORK) -o $@ $(LDFLAGS)

# Run all tests
run: all
	@./run_tests.sh

# Clean build artifacts
clean:
	rm -f $(ALL_TESTS)
	rm -f unit/*.o integration/*.o system/*.o
