CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2

ROOT := ..
DEVKITPRO ?= /opt/devkitpro
INCLUDES := -I$(ROOT)/include -I$(DEVKITPRO)/libnx/include

NCZBLOCK_SOURCES := $(ROOT)/source/util/nczblock.cpp test_nczblock.cpp
CONTENT_META_SOURCES := $(ROOT)/source/data/byte_buffer.cpp $(ROOT)/source/nx/content_meta.cpp $(ROOT)/source/util/debug.c test_content_meta.cpp
TARGETS := test_nczblock test_content_meta

.PHONY: all run clean

all: $(TARGETS)

test_nczblock: $(NCZBLOCK_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(NCZBLOCK_SOURCES) -o $@

test_content_meta: $(CONTENT_META_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CONTENT_META_SOURCES) -o $@

run: $(TARGETS)
	./test_nczblock
	./test_content_meta

clean:
	rm -f $(TARGETS)
