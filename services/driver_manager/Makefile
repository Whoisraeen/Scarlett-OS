# Driver Manager Service Makefile

TARGET = driver_manager
CARGO = cargo
RUSTFLAGS = --target x86_64-unknown-none -C link-arg=-nostdlib -C link-arg=-static

.PHONY: all clean

all: $(TARGET)

$(TARGET):
	$(CARGO) build --release $(RUSTFLAGS)
	cp target/x86_64-unknown-none/release/$(TARGET) $(TARGET)

clean:
	$(CARGO) clean
	rm -f $(TARGET)
