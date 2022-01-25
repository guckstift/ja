SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
CFLAGS = -std=c17 -pedantic-errors -DJA_DEBUG

ja: $(SRCS) $(HDRS)
	gcc -o $@ $(CFLAGS) $(SRCS)
