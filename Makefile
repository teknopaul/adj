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

INCS = -Isrc
# pkg install libasound2-dev
LIBS = -lasound -lpthread
VJDLIBS = -lvdj -lcdj
ADJDEPS = src/adj.h src/adj_keyb.h src/adj_midiin.h src/tui.h src/adj_vdj.h src/adj_tui.h src/adj_cli.h
ADJSRC = src/adj.c src/adj_keyb.c src/adj_vdj.c src/adj_midiin.c src/tui.c src/adj_tui.c src/adj_cli.c

OBJS = target/adj_diff.o target/adj_bpm.o target/adj_numpad.o target/adj_keyb.o target/adj_midiin.o target/adj_vdj.o target/tui.o target/adj_conf.o target/adj_tui.o target/adj_cli.o

all: target $(OBJS) target/libadj.so  target/libadj.a target/adj target/adj_midilearn

target:
	mkdir -p target

# Applications

target/adj: $(ADJDEPS) $(ADJSRC) $(OBJS) target/adj.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) target/adj.o -Ltarget $(LIBS) $(VJDLIBS) -ladj

target/adj_midilearn: $(ADJDEPS) src/adj_midilearn.c
	$(CC) $(CFLAGS) -o $@ src/adj_midilearn.c -Ltarget $(LIBS) $(VJDLIBS) -ladj

target/tui_test: src/tui.c src/tui.h test/tui_test.c
	$(CC) -Wall -fPIC -g -O3 src/tui.c test/tui_test.c -Isrc -o $@
	target/tui_test

# Shared library

target/libadj.so: $(ADJDEPS) src/libadj.c
	$(CC) $(CFLAGS) -shared -o $@ src/libadj.c $(LIBS)

target/libadj.a: $(ADJDEPS) src/libadj.c
	$(CC) $(CFLAGS) -c -o $@ src/libadj.c $(LIBS)

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

target/adj_numpad.o: src/adj_numpad.c src/adj_numpad.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_numpad.c $(LIBS)

target/adj_midiin.o: src/adj_midiin.c src/adj_midiin.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_midiin.c $(LIBS)

target/adj_conf.o: src/adj_conf.c src/adj_conf.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_conf.c $(LIBS)

target/adj_diff.o: src/adj_diff.c src/adj_diff.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_diff.c $(LIBS)

target/adj_bpm.o: src/adj_bpm.c src/adj_bpm.h
	$(CC) $(CFLAGS) -c -o $@ src/adj_bpm.c $(LIBS)

.PHONY: clean install uninstall deb test

test:
	sniprun test/adj_conf_test.c.snip
	sniprun test/adj_keyb_bpm_test.c.snip
	sniprun test/adj_midiin_test.c.snip
	sniprun test/adj_vdj_test.c.snip

clean:
	rm -rf target/

install:
	install -v -o root -m 755 target/adj           $(DESTDIR)/usr/bin/
	install -v -o root -m 755 target/libadj.so     $(DESTDIR)$(LIBDIR)/libadj.so.1.0
	cd $(DESTDIR)$(LIBDIR); ln -s libadj.so.1.0 libadj.so
	cp -rv etc/* $(DESTDIR)/etc/

uninstall:
	rm $(DESTDIR)/usr/bin/adj $(DESTDIR)$(LIBDIR)/libadj.so $(DESTDIR)$(LIBDIR)/libadj.so.1.0

deb:
	sudo deploy/build-deb.sh
