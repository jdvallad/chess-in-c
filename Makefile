CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic
LINKAGE = -lm
all: driver 

driver:
	$(CC) $(CFLAGS) -o driver chess.c static_chess.c full_chess.c driver.c $(LINKAGE)

run:
	./driver

debug:
	$(CC) -g $(CFLAGS) -o driver chess.c static_chess.c full_chess.c driver.c $(LINKAGE)
clean:
	rm -f driver
	rm -f *.o

scan-build:
	scan-build make

format:
	$(CC)-format -i -style=file *.c

valgrind: debug
	valgrind --leak-check=full -s ./driver

test: clean format driver run