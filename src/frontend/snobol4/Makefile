CC     = gcc
ROOT   = ../..
CFLAGS = -Wall -Wno-unused-function -Wno-unused-variable \
         -Wno-incompatible-pointer-types -g -O0 -I. -I$(ROOT)

# snobol4.tab.c, snobol4.tab.h, snobol4.lex.c are committed generated files.
# snobol4.tab.c/h are bison defaults. snobol4.lex.c is flex's lex.yy.c renamed.
# If bison/flex are available they will be regenerated from snobol4.y/snobol4.l.
# If not, the committed C files are used directly — no bison/flex required.

OBJS = snobol4.tab.o snobol4.lex.o

.PHONY: all clean

all: $(OBJS)

snobol4.tab.o: snobol4.tab.c snobol4.tab.h snobol4.h
	$(CC) $(CFLAGS) -c -o $@ $<

snobol4.lex.o: snobol4.lex.c snobol4.h snobol4.tab.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Regenerate from source if bison is available, otherwise keep committed file
snobol4.tab.c snobol4.tab.h: snobol4.y
	@if command -v bison >/dev/null 2>&1; then \
	    echo "bison -d snobol4.y"; \
	    bison -d snobol4.y; \
	else \
	    echo "bison not found — using committed snobol4.tab.c/h"; \
	    touch snobol4.tab.c snobol4.tab.h; \
	fi

# flex produces lex.yy.c by default; rename to snobol4.lex.c
snobol4.lex.c: snobol4.l snobol4.tab.h
	@if command -v flex >/dev/null 2>&1; then \
	    echo "flex snobol4.l && mv lex.yy.c snobol4.lex.c"; \
	    flex snobol4.l && mv lex.yy.c snobol4.lex.c; \
	else \
	    echo "flex not found — using committed snobol4.lex.c"; \
	    touch snobol4.lex.c; \
	fi

clean:
	rm -f *.o
