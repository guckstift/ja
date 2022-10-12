PROGNAME = \
	ja

CFILES = \
	analyze.c asm.c ast.c build.c cgen.c cgen_expr.c cgen_stmt.c cgen_type.c \
	elf.c lex.c main.c parse.c parse_expr.c parse_stmt.c parse_type.c print.c \
	string.c

HFILES = \
	analyze.h array.h asm.h ast.h build.h cgen.h elf.h lex.h parse.h \
	parse_internal.h print.h string.h

RESOURCES = \
	runtime.h runtime.c

BUILDDIR = \
	build

CFLAGS = \
	-std=c17 -pedantic-errors -D JA_DEBUG -g

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
	gcc -MM $(SRCS) | sed -e 's|^\([a-z].*\)|$(BUILDDIR)/\1|' > $@

clean:
	rm -rf $$(cat .gitignore)

.PHONY: test clean

ifneq (clean,$(findstring clean,$(MAKECMDGOALS)))
include $(BUILDDIR)/deps
endif
