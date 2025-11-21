# Settings Application Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../../gui/compositor/src -I../../gui/widgets/src
LDFLAGS = -L../../gui/compositor -L../../gui/widgets -lcompositor -lwidgets

TARGET = settings
SOURCES = main.c settings.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
