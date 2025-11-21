# Text Editor Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../../gui/compositor/src -I../../gui/widgets/src
LDFLAGS = -L../../gui/compositor -L../../gui/widgets -lcompositor -lwidgets

TARGET = texteditor
SOURCES = main.c texteditor.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
