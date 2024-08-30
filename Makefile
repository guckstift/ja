CFILES = $(wildcard src/*.c)
OFILES = $(CFILES:src/%.c=build/%.o)
OFILES_TEST = $(CFILES:src/%.c=build/%_test.o)

build/ja: $(OFILES) | build
	gcc -o $@ $^

build/ja_test: $(OFILES_TEST) | build
	gcc -o $@ $^

build/%.o: src/%.c | build
	gcc -o $@ -c $^

build/%_test.o: src/%.c | build
	gcc -o $@ -c $^ -D JA_TEST

build:
	mkdir $@

clean:
	rm -rf build

rebuild: clean build/ja

rebuild_test: clean build/ja_test

.PHONY: clean rebuild rebuild_test