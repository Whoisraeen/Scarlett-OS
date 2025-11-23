TARGET = x86_64-unknown-none
MODE ?= release

all: build

build:
	cargo build --release --target $(TARGET)
	cp target/$(TARGET)/release/nvme_driver ../../../build/drivers/nvme_driver

clean:
	cargo clean
