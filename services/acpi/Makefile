TARGET = x86_64-unknown-none
MODE ?= release

all: build

build:
	cargo build --release --target $(TARGET)
	cp target/$(TARGET)/release/acpi_service ../../build/services/acpi_service

clean:
	cargo clean
