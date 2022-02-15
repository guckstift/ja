CFILES = \
	ast.c build.c eval.c gen.c lex.c main.c parse.c parse_expr.c parse_stmt.c \
	parse_type.c print.c utils.c

HFILES = \
	ast.h build.h eval.h gen.h lex.h parse.h parse_utils.h print.h \
	runtime.inc.h utils.h

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

