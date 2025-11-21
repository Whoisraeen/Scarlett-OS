# Window Manager Service Makefile

TARGET = window_manager
CARGO = cargo
CARGO_FLAGS = --release --target-dir ../../build/gui/window_manager

all: $(TARGET)

$(TARGET):
	@echo "[CARGO] Building window manager service..."
	@$(CARGO) build $(CARGO_FLAGS)
	@cp ../../build/gui/window_manager/release/$(TARGET) $(TARGET)

clean:
	@echo "[CLEAN] Removing window manager build artifacts..."
	@rm -rf ../../build/gui/window_manager
	@rm -f $(TARGET)

.PHONY: all clean
