package = sio-agent
version = 1.0.7
tarname = $(package)
distdir = $(tarname)-$(version)

all clean sio-agent:
	cd src && $(MAKE) $@ AGENT_VERSION=$(version)

dist: $(distdir).tar.gz

$(distdir).tar.gz: $(distdir)
	tar chof - $(distdir) | gzip -9 -c > $@
	rm -rf $(distdir)

$(distdir): FORCE
	mkdir -p $(distdir)/src
	cp Makefile $(distdir)
	cp src/Makefile $(distdir)/src
	cp src/sio_agent.c $(distdir)/src
	cp src/sio_agent.h $(distdir)/src
	cp src/sio_local.c $(distdir)/src
	cp src/sio_serial.c $(distdir)/src
	cp src/sio_socket.c $(distdir)/src
	cp src/logmsg.c $(distdir)/src

FORCE:
	-rm $(distdir).tar.gz > /dev/null 2>&1
	-rm -rf $(distdir) > /dev/null 2>&1
        
.PHONY: FORCE all clean dist
