CFILES = \
	ast.c build.c cgen.c cgen_expr.c cgen_type.c eval.c lex.c main.c parse.c \
	parse_expr.c parse_stmt.c parse_type.c print.c utils.c

HFILES = \
	ast.h build.h eval.h cgen.h lex.h parse.h parse_utils.h print.h \
	utils.h \
	runtime.h.src.h runtime.c.src.h 

CFLAGS = \
	-std=c17 -pedantic-errors -DJA_DEBUG

SRCS = $(patsubst %.c,src/%.c,$(CFILES))
HDRS = $(patsubst %.h,src/%.h,$(HFILES))
TESTS = $(sort $(wildcard tests/*.ja))
TESTOKS = $(patsubst %.ja,%.ok,$(TESTS))

ja: $(SRCS) $(HDRS)
	gcc -o $@ $(CFLAGS) $(SRCS)

src/%.src.h: src/%
	echo \
		"#define" \
		"$(shell echo $* | tr [:lower:] [:upper:] | tr [.] [_])_SRC" \
		"\\" \
		> $@
	sed -e 's|"|\\"|g' -e 's|.*|\t"&\\n" \\|' < $^ >> $@
	echo "" >> $@

test: $(TESTOKS)

tests/%.ok: tests/%.ja ja
	./ja $<
	touch $@

.PHONY: test
