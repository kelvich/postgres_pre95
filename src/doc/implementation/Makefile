SRC=	am.me
DEST=	am.ps
ROFF=	groff
RFLAGS=	-te -me -Tps

all:	$(DEST)

am.ps: am.me
	$(ROFF) $(RFLAGS) $(SRC) > $(DEST)
