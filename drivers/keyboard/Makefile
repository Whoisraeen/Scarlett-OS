# Keyboard Driver Makefile

TARGET = keyboard_driver
CARGO = cargo
CARGO_FLAGS = --release --target-dir ../../build/drivers/keyboard

all: $(TARGET)

$(TARGET):
	@echo "[CARGO] Building keyboard driver..."
	@$(CARGO) build $(CARGO_FLAGS)
	@cp ../../build/drivers/keyboard/release/$(TARGET) $(TARGET)

clean:
	@echo "[CLEAN] Removing keyboard driver build artifacts..."
	@rm -rf ../../build/drivers/keyboard
	@rm -f $(TARGET)

.PHONY: all clean
