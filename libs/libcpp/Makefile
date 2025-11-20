# libcpp Makefile

CXX = g++
AR = ar
CXXFLAGS = -Wall -Wextra -std=c++20 -ffreestanding -fno-builtin -nostdlib -fno-exceptions -fno-rtti
ARFLAGS = rcs

SRCDIR = src
INCDIR = include
OBJDIR = build

SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = libcpp.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

