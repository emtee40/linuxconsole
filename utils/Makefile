#
# $Id$
#
# Makefile for Linux input utilities
#
# © 1998-2000 Vojtech Pavlik (sponsored by SuSE)
# © 2008-2016, 2019, 2021-2022 Stephen Kitt <steve@sk2.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301 USA.
#
#
# Edit the options below to suit your needs
#

PKG_CONFIG	?= pkg-config
CFLAGS		?= -g -O2 -Wall -Wextra

ifndef DISABLE_INPUTATTACH
PROGRAMS	+= inputattach
endif

ifndef DISABLE_JOYSTICK
PROGRAMS	+= jstest jscal jscal-restore jscal-store evdev-joystick
endif

ifndef DISABLE_FORCEFEEDBACK
PROGRAMS	+= fftest ffmvforce ffset ffcfstress
endif

PREFIX          ?= /usr/local

compile: $(PROGRAMS)

distclean: clean
clean:
	$(RM) *.o *.swp $(PROGRAMS) *.orig *.rej map *~ *.rules

inputattach.o: override CFLAGS += -funsigned-char
inputattach.o: inputattach.c serio-ids.h
ifeq ($(SYSTEMD_SUPPORT),1)
inputattach.o: override CPPFLAGS += -DSYSTEMD_SUPPORT=1
inputattach.o: override CFLAGS += $(shell $(PKG_CONFIG) --cflags libsystemd)
endif

inputattach: LDLIBS = -lm
ifeq ($(SYSTEMD_SUPPORT),1)
inputattach: LDLIBS += $(shell $(PKG_CONFIG) --libs-only-l libsystemd)
inputattach: override LDFLAGS += \
	$(shell $(PKG_CONFIG) --libs-only-L --libs-only-other libsystemd)
endif

ffcfstress.o: override CFLAGS += -funsigned-char
ffcfstress.o: ffcfstress.c bitmaskros.h

ffcfstress: LDLIBS = -lm

ffmvforce.o: override CFLAGS += $(shell $(PKG_CONFIG) --cflags sdl2)
ffmvforce.o: ffmvforce.c

ffmvforce: LDLIBS = -lm $(shell $(PKG_CONFIG) --libs sdl2)
ffmvforce: ffmvforce.o

axbtnmap.o: axbtnmap.c axbtnmap.h

jscal.o: jscal.c axbtnmap.h

jscal: LDLIBS = -lm
jscal: jscal.o axbtnmap.o

jstest.o: jstest.c axbtnmap.h

jstest: jstest.o axbtnmap.o

gencodes.o: gencodes.c scancodes.h

fftest.o: fftest.c bitmaskros.h

jscal-restore: jscal-restore.in
	sed "s^@@PREFIX@@^$(PREFIX)^g" < $^ > $@

jscal-store: jscal-store.in
	sed "s^@@PREFIX@@^$(PREFIX)^g" < $^ > $@

80-stelladaptor-joystick.rules: 80-stelladaptor-joystick.rules.in
	sed "s^@@PREFIX@@^$(PREFIX)^g" < $^ > $@

INSTALL_DEP = compile
ifndef DISABLE_JOYSTICK
INSTALL_DEP += 80-stelladaptor-joystick.rules
endif

install: $(INSTALL_DEP)
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(PROGRAMS) $(DESTDIR)$(PREFIX)/bin
ifndef DISABLE_JOYSTICK
	install -d $(DESTDIR)$(PREFIX)/share/joystick
	install extract filter ident $(DESTDIR)$(PREFIX)/share/joystick
	install -d $(DESTDIR)/lib/udev/rules.d
	install js-set-enum-leds $(DESTDIR)/lib/udev
	install -m 644 80-stelladaptor-joystick.rules $(DESTDIR)/lib/udev/rules.d
endif

.PHONY: compile clean distclean install
