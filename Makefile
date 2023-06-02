CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic
LINKAGE = -lm
all: driver 

driver: clean
	$(CC) $(CFLAGS) -o driver driver.c chess.c $(LINKAGE)

debug: clean
	$(CC) -g $(CFLAGS) -o driver driver.c chess.c $(LINKAGE)
clean:
	rm -f driver
	rm -f *.o

scan-build: clean
	scan-build make

format: clean
	$(CC)-format -i -style=file *.h
	$(CC)-format -i -style=file *.c

valgrind: debug
	valgrind --leak-check=full -s ./driver
