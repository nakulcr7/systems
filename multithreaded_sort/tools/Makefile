SRCS := $(wildcard *.c)
BINS := $(SRCS:.c=)

all: $(BINS)

$(BINS): $(SRCS)
	gcc -Wall -Werror -g -O2 -std=gnu99 -o $@ $@.c

clean:
	rm -f $(BINS)
