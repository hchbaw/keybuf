ACLCAL_AMFLAGS = -I m4
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 config.guess config.h.in \
config.sub configure depcomp install-sh ltmain.sh \
missing config.rpath mkinstalldirs \
ABOUT-NLS \
module.desktop

SUBDIRS = src data po

filesdir = $(datadir)
files_DATA = module.desktop e-module-keybuf.edj

EXTRA_DIST = \
module.desktop.in \
e-module-keybuf.edc \
data/images/module_icon.png

EDJE_FLAGS = -v -id $(top_srcdir)/data/images

%.edj: %.edc
	$(EDJE_CC) $(EDJE_FLAGS) $< $@

clean-local:
	rm -rf *.edj module.desktop *~
