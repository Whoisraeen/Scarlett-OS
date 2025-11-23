TARGET = x86_64-unknown-none
MODE ?= release

all: build

build:
	cargo build --release --target $(TARGET)
	cp target/$(TARGET)/release/wifi_driver ../../../build/drivers/wifi_driver

clean:
	cargo clean
