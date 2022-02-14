CFILES = \
	ast.c build.c eval.c gen.c parser.c lex.c main.c parse.c print.c \
	utils.c

HFILES = \
	ast.h build.h eval.h gen.h lex.h parse.h print.h runtime.inc.h utils.h

CFLAGS = \
	-std=c17 -pedantic-errors -DJA_DEBUG

SRCS = $(patsubst %.c,src/%.c,$(CFILES))
HDRS = $(patsubst %.h,src/%.h,$(HFILES))

ja: $(SRCS) $(HDRS) src/runtime.inc.h
	gcc -o $@ $(CFLAGS) $(SRCS)

src/%.inc.h: src/%.h
	echo "#define $(shell echo $* | tr [:lower:] [:upper:])_H_SRC \\" > $@
	sed 's|.*|\t"&\\n" \\|' < $^ >> $@
	echo "" >> $@

src/parser.c: src/autoparser.c
	touch src/parser.c

src/autoparser.c: parsergen src/syntax.txt
	./parsergen < src/syntax.txt > $@

parsergen: src/syntax.c src/parsergen.c
	gcc -o $@ $(CFLAGS) $^
