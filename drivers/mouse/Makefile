# Mouse Driver Makefile

TARGET = mouse_driver
CARGO = cargo
CARGO_FLAGS = --release --target-dir ../../build/drivers/mouse

all: $(TARGET)

$(TARGET):
	@echo "[CARGO] Building mouse driver..."
	@$(CARGO) build $(CARGO_FLAGS)
	@cp ../../build/drivers/mouse/release/$(TARGET) $(TARGET)

clean:
	@echo "[CLEAN] Removing mouse driver build artifacts..."
	@rm -rf ../../build/drivers/mouse
	@rm -f $(TARGET)

.PHONY: all clean
