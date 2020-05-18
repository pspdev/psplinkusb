all:
	$(MAKE) -f Makefile.psp all
	$(MAKE) -f Makefile.tools all

release:
	$(MAKE) -f Makefile.psp release
	$(MAKE) -f Makefile.tools release

clean:
	$(MAKE) -f Makefile.psp clean
	$(MAKE) -f Makefile.tools clean
