# Simple Makefile for chirp.c
CC ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -pedantic
INCLUDES ?=
LIBS ?=

TARGET := chirp
SRC := chirp.c

all: \
	$(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CXXFLAGS) $(INCLUDES) -x c++ $< -o $@ $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
