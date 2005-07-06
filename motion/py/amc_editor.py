#!/usr/bin/env python
#
# amc_editor.py - An editor for AMC files, based loosely on the IPO
#       interface in blender
#
# Copyright (C) 2005 David Trowbridge
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import pygtk
pygtk.require ('2.4')
import gtk

class AMCEditor:
    def __init__ (self):
        pass

    def main (self):
        gtk.main ()

if __name__ == '__main__':
    editor = AMCEditor ()
    editor.main ()
