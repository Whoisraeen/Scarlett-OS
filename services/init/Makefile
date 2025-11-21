# Init Service Makefile

TARGET = init
CARGO = cargo
CARGO_FLAGS = --release --target-dir ../../build/services/init

all: $(TARGET)

$(TARGET):
	@echo "[CARGO] Building init service..."
	@$(CARGO) build $(CARGO_FLAGS)
	@cp ../../build/services/init/release/$(TARGET) $(TARGET)

clean:
	@echo "[CLEAN] Removing init build artifacts..."
	@rm -rf ../../build/services/init
	@rm -f $(TARGET)

.PHONY: all clean
