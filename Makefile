CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -O2 -Isrc
TARGET  = piko
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)

.PHONY: all clean test test-lexer

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)

test:
	@for f in tests/*.piko; do \
		echo "--- $$f ---"; \
		./$(TARGET) "$$f"; \
	done
	@$(MAKE) --no-print-directory test-lexer

test-lexer:
	@tests/run_lexer_tests.sh ./$(TARGET)
