# 472-MGB makefile — standalone bridge core. C11, libm only.
# No MPE headers, no ncurses: link/frame/map/file-sink build anywhere.
CC      = gcc
CFLAGS  = -O2 -g -Wall -Wextra -Werror -std=c11 -Iinclude -ffp-contract=off
OBJDIR  = build

LIB_SRCS = src/mgb_link.c src/mgb_frame.c src/mgb_map.c \
           src/mgb_sink_file.c src/mgb_sink_mpe_stub.c
LIB_OBJS = $(OBJDIR)/mgb_link.o $(OBJDIR)/mgb_frame.o $(OBJDIR)/mgb_map.o \
           $(OBJDIR)/mgb_sink_file.o $(OBJDIR)/mgb_sink_mpe_stub.o

.PHONY: all clean test probe

all: $(OBJDIR) $(OBJDIR)/mgb_probe $(OBJDIR)/test_mgb

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c include/mgb.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/mgb_probe: tools/mgb_probe.c $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ tools/mgb_probe.c $(LIB_OBJS) -lm

$(OBJDIR)/test_mgb: tests/test_mgb.c $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ tests/test_mgb.c $(LIB_OBJS) -lm

# probe: petri dish through a live s2tui (path may be absolute or relative)
probe: all
	./$(OBJDIR)/mgb_probe --s2tui ../s2tui --scene petri \
	    --out probe_petri.mgb --frame probe_petri.s2

test: all
	./$(OBJDIR)/test_mgb

clean:
	rm -rf $(OBJDIR) probe_petri.mgb probe_petri.s2
