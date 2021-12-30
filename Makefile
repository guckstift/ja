SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
CFLAGS = -std=c17 -pedantic-errors

yummy: $(SRCS) $(HDRS)
	gcc -o $@ $(CFLAGS) $(SRCS)
