PROGNAME = \
	ja

CFILES = \
	ast.c build.c cgen.c cgen_expr.c cgen_type.c eval.c lex.c main.c parse.c \
	parse_expr.c parse_stmt.c parse_type.c print.c utils.c

HFILES = \
	ast.h build.h eval.h cgen.h lex.h parse.h parse_utils.h print.h \
	utils.h \

RESOURCES = \
	runtime.h runtime.c

BUILDDIR = \
	build

CFLAGS = \
	-std=c17 -pedantic-errors -D JA_DEBUG

LDFLAGS = \
	 -ldl

PROGTARGET = ./$(BUILDDIR)/$(PROGNAME)
SRCS = $(patsubst %.c,src/%.c,$(CFILES))
HDRS = $(patsubst %.h,src/%.h,$(HFILES))
RESS = $(patsubst %,$(BUILDDIR)/%.res,$(RESOURCES))
OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(CFILES))
TESTS = $(sort $(wildcard tests/*.ja))
TESTOKS = $(patsubst tests/%.ja,$(BUILDDIR)/%.ok,$(TESTS))

$(PROGTARGET): $(OBJS) | $(BUILDDIR)
	gcc -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	gcc -o $@ -c $(CFLAGS) $<

$(BUILDDIR)/jaja: $(PROGTARGET) $(wildcard jasrc/*.ja) | $(BUILDDIR)
	$(PROGTARGET) -c jaja jasrc/ja.ja

$(BUILDDIR)/%.res: src/% | $(BUILDDIR)
	echo "#define" $(shell echo $* | tr a-z. A-Z_)_RES "\\" > $@
	sed -e 's|"|\\"|g' -e 's|.*|\t"&\\n" \\|' < $^ >> $@
	echo "" >> $@

test: $(TESTOKS)

$(BUILDDIR)/%.ok: tests/%.ja $(PROGTARGET) | $(BUILDDIR)
	./build/ja $<
	touch $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/deps: $(SRCS) $(HDRS) $(RESS) | $(BUILDDIR)
	gcc -MM $(SRCS) > $@

clean:
	rm -rf $$(cat .gitignore)

.PHONY: test clean

ifneq (clean,$(findstring clean,$(MAKECMDGOALS)))
include $(BUILDDIR)/deps
endif
