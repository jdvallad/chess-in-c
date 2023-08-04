CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic
LINKAGE = -lm
SOURCES = chess.c static_chess.c full_chess.c legal_moves.c driver.c
TEST_SOURCES = chess.c test_driver.c
all: driver 

driver:
	$(CC) $(CFLAGS) -o driver $(SOURCES) $(LINKAGE)

prototyper:
	$(CC) $(CFLAGS) -o prototyper $(TEST_SOURCES) $(LINKAGE)

run:
	./driver

walk:
	./prototyper

clean:
	rm -f driver
	rm -f prototyper
	rm -f *.o

scan-build:
	scan-build make

format:
	$(CC)-format -i -style=file *.c

valgrind:
	valgrind --leak-check=full -s ./driver

drive: clean format driver run

prototype: clean format prototyper walk