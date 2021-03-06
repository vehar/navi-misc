#***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) 1998 - 2004, Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at http://curl.haxx.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# $Id: Makefile.dist,v 1.19 2004/03/17 12:46:42 bagder Exp $
###########################################################################

all:
	./configure
	make

ssl:
	./configure --with-ssl
	make

borland:
	cd lib & make -f Makefile.b32
	cd src & make -f Makefile.b32

mingw32:
	cd lib & make -f Makefile.m32 ZLIB=1
	cd src & make -f Makefile.m32 ZLIB=1

mingw32-ssl:
	cd lib & make -f Makefile.m32 SSL=1 ZLIB=1
	cd src & make -f Makefile.m32 SSL=1 ZLIB=1

vc:
	cd lib
	nmake /f Makefile.vc6 cfg=release
	cd ..\src
	nmake /f Makefile.vc6

vc-ssl:
	cd lib
	nmake /f Makefile.vc6 cfg=release-ssl
	cd ..\src
	nmake /f Makefile.vc6 cfg=release-ssl

vc-ssl-dll:
	cd lib
	nmake /f Makefile.vc6 cfg=release-ssl-dll
	cd ..\src
	nmake /f Makefile.vc6

vc-libcurl-ssl-dll:
	cd lib
	nmake /f Makefile.vc6 cfg=release-libcurl-ssl-dll
	cd ..\src
	nmake /f Makefile.vc6

djgpp:
	make -C lib -f Makefile.dj
	make -C src -f Makefile.dj

cygwin:
	./configure
	make

cygwin-ssl:
	./configure --with-ssl
	make

amiga:
	cd ./lib && make -f makefile.amiga
	cd ./src && make -f makefile.amiga

netware:
        cd lib && make -f Makefile.netware
        cd src && make -f Makefile.netware

unix: all

unix-ssl: ssl

linux: all

linux-ssl: ssl
