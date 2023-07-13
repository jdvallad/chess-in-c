CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic
LINKAGE = -lm
all: driver 

driver:
	$(CC) $(CFLAGS) -o Driver Chess.c offset_set.c Driver.c $(LINKAGE)

run:
	./Driver

debug:
	$(CC) -g $(CFLAGS) -o Driver Chess.c offset_set.c Driver.c $(LINKAGE)
clean:
	rm -f driver
	rm -f *.o

scan-build:
	scan-build make

format:
	$(CC)-format -i -style=file *.c

valgrind: debug
	valgrind --leak-check=full -s ./Driver

test: clean format driver run