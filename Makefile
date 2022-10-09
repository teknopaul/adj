arch != uname -m

ifeq ($(arch),x86_64)
  LIBDIR=/usr/lib
else ifeq ($(arch),armv6l)
  LIBDIR=/usr/lib
else ifeq ($(arch),armv7l)
  LIBDIR=/usr/lib
endif

CC = gcc
CFLAGS = -Wall -Werror -Wno-format-security -fPIC -g -O3

INCS = -Isrc -I/usr/include/libusb-1.0
# pkg install libasound2-dev libusb-1.0-0-dev avahi-autoipd
LIBS = -lasound -lpthread -lusb-1.0 -ldl
VJDLIBS = -lvdj -lcdj
ADJDEPS = src/adj.h src/adj_keyb.h src/adj_midiin.h src/tui.h src/adj_vdj.h src/adj_tui.h src/adj_cli.h
ADJSRC = src/adj.c src/adj_keyb.c src/adj_vdj.c src/adj_midiin.c src/tui.c src/adj_tui.c src/adj_cli.c

OBJS = target/adj_diff.o target/adj_bpm.o target/adj_numpad.o target/adj_keyb.o target/adj_store.o target/adj_bpm_tap.o target/adj_js.o target/adj_mod.o target/adj_midiin.o target/adj_midiout.o target/adj_vdj.o target/tui.o target/adj_conf.o target/adj_tui.o target/adj_cli.o
MODS = target/mod/adj_logi.so target/mod/adj_switch.so target/mod/adj_ps3.so
SEQS = target/mod/adj_mod_seq_rideomatic.so target/mod/adj_mod_seq_bombomatic.so target/mod/adj_mod_seq_midimatic.so

all: target target/mod $(OBJS) target/libadj.so  target/libadj.a target/adj target/adj_midilearn $(MODS) $(SEQS)

target:
	mkdir -p target

target/mod:
	mkdir -p target/mod

# Applications

target/adj: $(ADJDEPS) $(ADJSRC) $(OBJS) target/adj.o
	$(CC) $(CFLAGS) -export-dynamic -o $@ $(OBJS) target/adj.o -Ltarget $(LIBS) $(VJDLIBS) -ladj

target/adj_midilearn: $(ADJDEPS) src/adj_midilearn.c
	$(CC) $(CFLAGS) -o $@ src/adj_midilearn.c -Ltarget $(LIBS) $(VJDLIBS) -ladj

target/tui_test: src/tui.c src/tui.h test/tui_test.c
	$(CC) -Wall -fPIC -g -O3 src/tui.c test/tui_test.c -Isrc -o $@
	target/tui_test

# Shared library

target/libadj.a: $(ADJDEPS) src/libadj.c
	$(CC) $(CFLAGS) -c -o $@ src/libadj.c $(LIBS)
	
target/libadj.so: $(ADJDEPS) src/libadj.c
	$(CC) $(CFLAGS) -shared -o $@ src/libadj.c $(LIBS)

target/mod/adj_logi.so: $(ADJDEPS) src/mod/adj_mod_logi.c
	$(CC) $(CFLAGS) -shared -o $@ src/mod/adj_mod_logi.c

target/mod/adj_switch.so: $(ADJDEPS) src/mod/adj_mod_switch.c
	$(CC) $(CFLAGS) -shared -o $@ src/mod/adj_mod_switch.c

target/mod/adj_ps3.so: $(ADJDEPS) src/mod/adj_mod_ps3.c
	$(CC) $(CFLAGS) -shared -o $@ src/mod/adj_mod_ps3.c


# Objects

target/adj.o: src/adj.c src/adj.h
	$(CC) -Wall -fPIC -c src/adj.c -Isrc -o $@

target/tui.o: src/tui.c src/tui.h
	$(CC) -Wall -fPIC -c src/tui.c -Isrc -o $@

target/adj_vdj.o: src/adj_vdj.c src/adj_vdj.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_vdj.c $(LIBS)

target/adj_tui.o: src/adj_tui.c src/adj_tui.h
	$(CC) -Wall -fPIC -c src/adj_tui.c -Isrc -o $@

target/adj_cli.o: src/adj_cli.c src/adj_cli.h
	$(CC) -Wall -fPIC -c src/adj_cli.c -Isrc -o $@

target/adj_keyb.o: src/adj_keyb.c src/adj_keyb.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_keyb.c $(LIBS)

target/adj_js.o: src/adj_js.c src/adj_js.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_js.c $(LIBS)

target/adj_store.o: src/adj_store.c src/adj_store.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_store.c $(LIBS)

target/adj_mod.o: src/adj_mod.c src/adj_mod.h
	$(CC) $(CFLAGS) -c $(INCS) -o $@ src/adj_mod.c $(LIBS)

target/adj_numpad.o: src/adj_numpad.c src/adj_numpad.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_numpad.c $(LIBS)

target/adj_midiin.o: src/adj_midiin.c src/adj_midiin.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_midiin.c $(LIBS)

target/adj_midiout.o: src/adj_midiout.c src/adj_midiout.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_midiout.c $(LIBS)

target/adj_conf.o: src/adj_conf.c src/adj_conf.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_conf.c $(LIBS)

target/adj_diff.o: src/adj_diff.c src/adj_diff.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_diff.c $(LIBS)

target/adj_bpm.o: src/adj_bpm.c src/adj_bpm.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_bpm.c $(LIBS)

target/adj_bpm_tap.o: src/adj_bpm_tap.c src/adj_bpm_tap.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_bpm_tap.c $(LIBS)

# sequencer utils
target/mod/adj_mod_seq.o: src/mod/adj_mod_seq.c src/mod/adj_mod_seq_api.h
	$(CC) $(CFLAGS) -c -o $@ src/mod/adj_mod_seq.c $(LIBS)
	
target/mod/adj_mod_seq_rideomatic.so: target/mod/adj_mod_seq.o src/mod/adj_mod_seq_rideomatic.c
	$(CC) $(CFLAGS) -shared -o $@ target/mod/adj_mod_seq.o src/mod/adj_mod_seq_rideomatic.c $(LIBS)
	
target/mod/adj_mod_seq_bombomatic.so: target/mod/adj_mod_seq.o src/mod/adj_mod_seq_rideomatic.c
	$(CC) $(CFLAGS) -shared -o $@ target/mod/adj_mod_seq.o src/mod/adj_mod_seq_rideomatic.c $(LIBS)
	
target/mod/adj_mod_seq_midimatic.so: target/mod/adj_mod_seq.o src/mod/adj_mod_seq_rideomatic.c
	$(CC) $(CFLAGS) -shared -o $@ target/mod/adj_mod_seq.o src/mod/adj_mod_seq_rideomatic.c $(LIBS)

	
.PHONY: clean install uninstall deb test

test:
	sniprun test/adj_conf_test.c.snip
	sniprun test/adj_keyb_bpm_test.c.snip
	sniprun test/adj_midiin_test.c.snip
	sniprun test/adj_vdj_test.c.snip
	sniprun test/adj_bpm_tap_test.c.snip
	sniprun test/adj_diff_test.c.snip
	sniprun test/adj_midiin_test.c.snip
	sniprun test/adj_util_test.c.snip
	sniprun test/adj_vdj_test.c.snip
	sniprun test/amidiclock_test.c.snip
	sniprun test/adj_mod_test.c.snip

clean:
	rm -rf target/

install:
	mkdir -p $(DESTDIR)$(LIBDIR)/adj
	install -v -o root -m 755 target/adj           $(DESTDIR)/usr/bin/
	install -v -o root -m 755 target/libadj.so     $(DESTDIR)$(LIBDIR)/libadj.so.1.0
	install -v -o root -m 755 target/mod/adj_logi.so     $(DESTDIR)$(LIBDIR)/adj/adj_logi.so
	install -v -o root -m 755 target/mod/adj_switch.so     $(DESTDIR)$(LIBDIR)/adj/adj_switch.so
	install -v -o root -m 755 target/mod/adj_ps3.so     $(DESTDIR)$(LIBDIR)/adj/adj_ps3.so
	cd $(DESTDIR)$(LIBDIR); ln -s libadj.so.1.0 libadj.so
	test -f $(DESTDIR)/etc/adj.conf && cp $(DESTDIR)/etc/adj.conf $(DESTDIR)/etc/adj.conf.orig
	cp -rv etc/* $(DESTDIR)/etc/
	test -f $(DESTDIR)/etc/adj.conf.orig && mv $(DESTDIR)/etc/adj.conf.orig $(DESTDIR)/etc/adj.conf

uninstall:
	rm $(DESTDIR)/usr/bin/adj $(DESTDIR)$(LIBDIR)/libadj.so $(DESTDIR)$(LIBDIR)/libadj.so.1.0

deb:
	sudo deploy/build-deb.sh
