# ATA Driver Makefile

TARGET = ata_driver
CARGO = cargo
CARGO_FLAGS = --release --target-dir ../../build/drivers/ata

all: $(TARGET)

$(TARGET):
	@echo "[CARGO] Building ATA driver..."
	@$(CARGO) build $(CARGO_FLAGS)
	@cp ../../build/drivers/ata/release/$(TARGET) $(TARGET)

clean:
	@echo "[CLEAN] Removing ATA driver build artifacts..."
	@rm -rf ../../build/drivers/ata
	@rm -f $(TARGET)

.PHONY: all clean
