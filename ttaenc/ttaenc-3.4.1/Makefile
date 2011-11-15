#
# $Id: Makefile,v 1.4 2007/04/04 00:00:00 root Exp $
#

CFLAGS	= -Wall -O3 -fomit-frame-pointer -funroll-loops \
		  -fforce-addr -falign-functions=4 -msse
TTAENC	= ttaenc
DESTDIR	=
INSDIR	= $(DESTDIR)/usr/bin

ttaenc: $(patsubst %.c, %.o, $(wildcard *.c))
	gcc $^ -o $@ $(CFLAGS)

%.o:	%.c
	gcc -c $(CFLAGS) $<

install:
	install -d $(INSDIR)
	if [ -n "$(TTAENC)" ]; then \
		strip $(TTAENC) ; \
		install -m 755 $(TTAENC) $(INSDIR) ; \
	fi

remove:
	rm -f $(INSDIR)/$(TTAENC)

clean:
	rm -f *.o $(TTAENC)
