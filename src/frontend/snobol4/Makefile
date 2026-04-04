CC     = gcc
ROOT   = ../..
CFLAGS = -Wall -Wno-unused-function -Wno-unused-variable \
         -Wno-incompatible-pointer-types -g -O0 -I. -I$(ROOT)

# snobol4.tab.c, snobol4.tab.h, snobol4.lex.c are committed generated files.
# Do NOT invoke bison/flex here — they are never installed in the session env.
# To regenerate: modify snobol4.y/snobol4.l on your own machine, run bison/flex
# there, and commit the resulting C files. See RULES.md.
# Exception: if bison/flex ARE installed (e.g. after apt-get), the rules below
# will regenerate and recompile automatically when .y/.l are touched.

OBJS = snobol4.tab.o snobol4.lex.o

.PHONY: all clean

all: $(OBJS)

snobol4.tab.o: snobol4.tab.c snobol4.tab.h snobol4.h
	$(CC) $(CFLAGS) -c -o $@ $<

snobol4.lex.o: snobol4.lex.c snobol4.h snobol4.tab.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Regenerate from snobol4.y if bison is available; abort otherwise so make
# does not silently use a stale committed file when .y has changed.
snobol4.tab.c snobol4.tab.h: snobol4.y
	@if command -v bison >/dev/null 2>&1; then \
	    echo "bison -d -o snobol4.tab.c snobol4.y"; \
	    bison -d -o snobol4.tab.c snobol4.y; \
	else \
	    echo "⛔ bison not installed — snobol4.tab.c/h are committed. Regenerate on your own machine."; \
	    false; \
	fi

# flex produces lex.yy.c by default; use -o to name directly.
snobol4.lex.c: snobol4.l snobol4.tab.h
	@if command -v flex >/dev/null 2>&1; then \
	    echo "flex -o snobol4.lex.c snobol4.l"; \
	    flex -o snobol4.lex.c snobol4.l; \
	else \
	    echo "⛔ flex not installed — snobol4.lex.c is committed. Regenerate on your own machine."; \
	    false; \
	fi

clean:
	rm -f *.o
