CC       := clang
CFLAGS   := -std=c23 -O3 -Wall -Wextra -Werror
TARGET   := cs
PREFIX   := /usr/local
BINDIR   := $(PREFIX)/bin
INSTALL  := install

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): cs.c
	$(CC) $(CFLAGS) $< -o $@

install: $(TARGET)
	$(INSTALL) -Dm755 $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

test: $(TARGET)
	@set -e; \
	for file in examples/*.cs; do \
		echo "RUN $$file"; \
		./$(TARGET) "$$file" > /dev/null; \
	done
	@echo "All examples passed."

clean:
	rm -f $(TARGET)
