EDJE_FLAGS = -v \
-id @THEME_IMAGE_SRC_DIR@ \
-fd .

filesdir = $(datadir)
files_DATA = \
keybuf.edj

%.edj: %.edc
	$(EDJE_CC) $(EDJE_FLAGS) $< $@

clean-local:
	rm -f *.edj
