CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -g
CFLAGS   += -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Iinclude -MMD -MP
LDFLAGS  ?=
BUILD    := build
SRCS     := $(wildcard src/*.c)
OBJS     := $(SRCS:src/%.c=$(BUILD)/%.o)
DEPS     := $(OBJS:.o=.d)
TARGET   := $(BUILD)/demo

.PHONY: all clean run
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET) hello

clean:
	rm -rf $(BUILD)

-include $(DEPS)
