PROJ := aadrill

SRCS := aadrill.c drill_dict.c
OBJS := $(patsubst %.c,%.o,$(SRCS))

CC := gcc
CFLAGS := -Os -g
LDFLAGS := -laa -lVFlib3

all: clean $(PROJ)

$(PROJ): $(OBJS)
	$(CC) $(OBJS) -o $(PROJ) $(LDFLAGS)

$(OBJS):%.o:%.c
	$(CC) $(CFLAGS) $< -o $@ -c $(CFLAGS)

test:
	./$(PROJ)

clean:
	rm -f $(PROJ) $(OBJS)
