#CFILES = $(filter-out src/_%.c,$(wildcard src/*.c))

CFILES = \
	src/analyze.c \
	src/arena.c \
	src/array.c \
	src/ast.c \
	src/build.c \
	src/error.c \
	src/gen.c \
	src/gen_expr.c \
	src/gen_impl.c \
	src/gen_stmt.c \
	src/gen_type.c \
	src/lex.c \
	src/list.c \
	src/main.c \
	src/parse.c \
	src/parse_expr.c \
	src/parse_impl.c \
	src/parse_stmt.c \
	src/parse_type.c \
	src/print.c \
	src/string.c \
	src/table.c \

OFILES = $(CFILES:src/%.c=build/%.o)
DEPFILES = $(CFILES:src/%.c=build/%.dep)

build/ja: $(OFILES) | build
	gcc -o $@ $^

build/%.o: src/%.c | build
	gcc -o $@ -c -I. src/$*.c -std=gnu2x

build/deps: $(DEPFILES) | build
	cat $(DEPFILES) > $@

build/%.dep: src/%.c | build
	gcc -MG -MT build/$*.o -MM -MF $@ $^

build/%.res: src/% | build
	xxd -i < src/$* > $@

include build/deps

build:
	mkdir $@

clean:
	rm -rf build/*.o
	rm -rf build/*.res
	rm -rf build/ja

clean-all:
	rm -rf build

rebuild: clean build/ja

.PHONY: clean clean-all rebuild update-deps