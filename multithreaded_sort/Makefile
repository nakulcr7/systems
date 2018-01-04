
CFLAGS := -g -Wall -Werror -std=gnu99
SRCS   := $(wildcard *.c)

all: tools tssort

tssort: $(SRCS)
	gcc $(CFLAGS) -o tssort $(SRCS) -lm -lpthread

tools:
	(cd tools && make)

test:
	@make clean
	@make all
	tools/gen-input 10 data.dat
	tools/print-data data.dat
	/usr/bin/time -p ./tssort 1 data.dat outp.dat
	@echo
	@(tools/check-sorted outp.dat && echo "Data Sorted OK") || echo "Fail"
	@echo
	@rm -f data.dat outp.dat

clean:
	(cd tools && make clean)
	rm -f tssort data.dat *.plist valgrind.out

.PHONY: clean all tools

