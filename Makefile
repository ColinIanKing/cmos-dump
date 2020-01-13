#
# Copyright (C) 2011-2020 Canonical
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Author: Colin Ian King <colin.king@canonical.com>
#

cmos-dump: cmos-dump.o

CFLAGS += -O2 -Wall -Wextra

BINDIR=/usr/sbin

clean:
	rm -f cmos-dump cmos-dump.o

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp cmos-dump ${DESTDIR}${BINDIR}

snap: clean
	rm -f *.snap.*
	snapcraft clean
	snapcraft
