# ScarlettOS Benchmark Runner

# Benchmark executables
MICRO_BENCH = tests/benchmarks/micro_bench
IO_BENCH = tests/benchmarks/io_bench
SYSTEM_BENCH = tests/benchmarks/system_bench
BOOT_BENCH = tests/benchmarks/boot_bench

# All benchmarks
ALL_BENCHMARKS = $(MICRO_BENCH) $(IO_BENCH) $(SYSTEM_BENCH) $(BOOT_BENCH)

# Default target
all: run

# Run all benchmarks
run: $(ALL_BENCHMARKS)
	@echo "======================================"
	@echo "Running ScarlettOS Benchmark Suite"
	@echo "======================================"
	@echo ""
	@$(MICRO_BENCH)
	@echo ""
	@$(IO_BENCH)
	@echo ""
	@$(SYSTEM_BENCH)
	@echo ""
	@$(BOOT_BENCH)
	@echo ""
	@echo "======================================"
	@echo "Benchmark Suite Complete"
	@echo "======================================"

# Run individual benchmarks
micro:
	@$(MICRO_BENCH)

io:
	@$(IO_BENCH)

system:
	@$(SYSTEM_BENCH)

boot:
	@$(BOOT_BENCH)

# Save results
save-results:
	@mkdir -p results
	@echo "Saving benchmark results..."
	@$(MICRO_BENCH) > results/micro_bench.txt
	@$(IO_BENCH) > results/io_bench.txt
	@$(SYSTEM_BENCH) > results/system_bench.txt
	@$(BOOT_BENCH) > results/boot_bench.txt
	@echo "Results saved to results/"

# Compare with baseline
compare:
	@echo "Comparing with baseline..."
	@if [ -f baseline/micro_bench.txt ]; then \
		diff baseline/micro_bench.txt results/micro_bench.txt || true; \
	fi

.PHONY: all run micro io system boot save-results compare
