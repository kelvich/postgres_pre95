SRC=	am.me smgr.me
DEST=	am.ps smgr.ps
ROFF=	groff
RFLAGS=	-te -me -Tps

all:	$(DEST)

am.ps: am.me
	$(ROFF) $(RFLAGS) am.me > am.ps

smgr.ps: smgr.me
	$(ROFF) $(RFLAGS) smgr.me > smgr.ps
