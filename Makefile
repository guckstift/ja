SRCS = $(wildcard src/*.c)
HDRS = $(wildcard src/*.h)
CFLAGS = -std=c17 -pedantic-errors -DJA_DEBUG

ja: $(SRCS) $(HDRS) src/runtime.inc.h
	gcc -o $@ $(CFLAGS) $(SRCS)

src/%.inc.h: src/%.h
	echo "#define $(shell echo $* | tr [:lower:] [:upper:])_H_SRC \\" > $@
	sed 's|.*|\t"&\\n" \\|' < $^ >> $@
	echo "" >> $@

