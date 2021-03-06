""" MI6K Vacuum Fluorescent Display plugin for Freevo

This file contains Freevo plugins for displaying status information
on the VFD included in the Media Infrawidget 6000 set-top-box peripheral.

It only controls the VFD and related display functionality of the MI6K,
IR receive and transmit is done through the de-facto standard LIRC daemon.
This requires a recent version of the 'mi6k' and 'vfdwidgets' python
modules, included in the MI6K source.

The MI6K does support lcdproc now, so in theory you could use freevo's
standard 'lcd' plugin. However, there are  a number of problems with this.
The MI6K's lcdproc driver requires a CVS version of lcdproc whereas the
PyLCD module used by freevo's lcd plugin doesn't support this recent
a version. The 'lcd' plugin uses hardcoded layouts which aren't nearly
as flexible as the vfdwidgets module. Finally, the MI6K supports brightness
control, power management, and LED features that lcdproc doesn't.

Copyright (C) 2004 Micah Dowty <micah@navi.cx>
"""

import plugin
import config
import audio.player
from event import *

import mi6k
import vfdwidgets

import time
import threading
import thread
import re


def findPlayingItem():
    """If a video or audio item is currently playing, returns it"""
    players = plugin.getbyname('VIDEO_PLAYER', True) + plugin.getbyname("AUDIO_PLAYER", False)
    for player in players:
        if not getattr(player, 'item', None):
            continue
        if not (player.app and player.app.isAlive()):
            continue
        return player.item


class MediaWidgetMixin(object):
    """A widget mix-in that causes any widget to show
       information about the currently playing media file,
       if there is one.
       """
    def update(self, dt):
        """At each time step, try to format information from
           the currently active item. If we can't find any info
           or we aren't playing anything, hide ourselves.
           """
        item = findPlayingItem()
        if item:
            t = self.formatItem(item)
            if t:
                self.visible = True
                self.text = t
            else:
                self.visible = False
        else:
            self.visible = False
        super(MediaWidgetMixin, self).update(dt)

    def formatItem(self, item):
        """Subclasses must implement this to gather some
           bit of data from the media item and format it
           as a string.
           """
        raise NotImplementedError


class MediaTitleWidget(MediaWidgetMixin, vfdwidgets.LoopingScroller):
    """A widget that shows the title of the currently playing media file"""
    def formatItem(self, item):
        return getattr(item, 'title', None) or getattr(item, 'name', None)


class MediaTimesWidget(MediaWidgetMixin, vfdwidgets.Text):
    """A widget that shows the amount of elapsed and remaining time in
       the currently playing media file.
       """
    def formatItem(self, item):
        elapsed = self.formatElapsed(item)
        total = self.formatTotal(item)
        if elapsed and total:
            return "%s / %s" % (elapsed, total)
        elif elapsed:
            return elapsed
        elif total:
            return total

    def formatElapsed(self, item):
        try:
            elapsed = item['elapsed']
        except:
            return None

        if isinstance(elapsed, str):
            # The item already formatted it for us. Ignore it if we can't
            # find any nonzero digits in it though- freevo currently doesn't
            # get elapsed time info from xine, and it will be stuck at zero.
            if re.search("[1-9]", elapsed):
                return elapsed
            else:
                return

        # Also ignore numeric elapsed times if they're stuck at zero
        if elapsed > 3600:
            # Format it with hours
            return "%d:%02d:%02d" % (elapsed/3600, (elapsed/60)%60, elapsed%60)
        elif elapsed > 0:
            return "%02d:%02d" % (elapsed/60, elapsed%60)

    def formatTotal(self, item):
        try:
            length = item['runtime']
        except:
            length = None
        if not length:
            try:
                length = item['length']
            except:
                length = None
        return length


class UpdaterThread(threading.Thread):
    """This thread runs continuously in the background, updating our VFD"""
    def __init__(self, mi6k, surface):
        threading.Thread.__init__(self)
        self.mi6k = mi6k
        self.surface = surface
        self.stop = False
        self.lock = thread.allocate_lock()

    def run(self):
        oldFrame = None
        ledUpdater = LEDUpdater(self.mi6k.lights)
        while not self.stop:
            ledUpdater.run()
            self.surface.update()
            self.lock.acquire()
            try:
                self.surface.layout()
                frame = self.surface.draw()
            finally:
                self.lock.release()
            if frame == oldFrame:
                # Nothing changed, wait a few ticks
                time.sleep(0.01)
            else:
                # Do a blocking write of the latest frame
                self.mi6k.vfd.writeLines(frame)
            oldFrame = frame
        self.mi6k.vfd.clear()
        ledUpdater.close()


class LEDUpdater:
    """This class is responsible for doing background updates of our LEDs"""
    playingLevel = 0.004
    blueDimRate = 0.9
    whiteDimRate = 0.8

    def __init__(self, lights):
        self.lights = lights
        self.lastPlayingItem = None

    def close(self):
        self.lights.blue = 0
        self.lights.white = 0

    def run(self):
        # The blue LED flashes as we start playing something,
        # then stays dimly lit as long as it's playing
        currentItem = findPlayingItem()
        if currentItem:
            if currentItem != self.lastPlayingItem:
                self.lights.blue = 1
                self.lastPlayingItem = currentItem
            else:
                self.lights.blue = max(self.playingLevel, self.lights.blue * self.blueDimRate)
        else:
            self.lights.blue = 0

        # We flash the white LED when a new OSD message comes in, this just fades it
        self.lights.white *= self.whiteDimRate


class PluginInterface(plugin.DaemonPlugin):
    """Plugin to start and stop the VFD updater thread, and pass events to it"""
    def __init__(self, device="/dev/mi6k*"):
        plugin.DaemonPlugin.__init__(self)

        # We want OSD events even if another plugin does too
        self.event_listener = True
        self.osdWidget = None

        self.mi6k = mi6k.Device(device)
        self.initVfd()

        self.surface = vfdwidgets.Surface(self.mi6k.vfd.width,
                                          self.mi6k.vfd.lines)
        self.initWidgets()

        self.updater = UpdaterThread(self.mi6k, self.surface)
        self.updater.setDaemon(1)
        self.updater.start()

    def initVfd(self):
        """Power on the VFD, set defaults, and allocate characters"""
        self.mi6k.vfd.powerOn()
        self.mi6k.vfd.setBrightness(0.1)
        self.mi6k.lights.blue = 0
        self.mi6k.lights.white = 0

        self.ellipses = self.mi6k.vfd.allocCharacter([
            [ 0, 0, 0, 0, 0 ],
            [ 0, 0, 0, 0, 0 ],
            [ 0, 0, 0, 0, 0 ],
            [ 0, 0, 0, 0, 0 ],
            [ 0, 0, 0, 0, 0 ],
            [ 0, 0, 0, 0, 0 ],
            [ 1, 0, 1, 0, 1 ] ])

    def shutdown(self):
        while self.updater.isAlive():
            self.updater.stop = True
            time.sleep(0.1)

    def initWidgets(self):
        """Add all permanent widgets to our VFD surface"""

        # Add some low-priority widgets to fill in the space
        # if nothing else is present- a title at the top, and
        # a clock in the lower-left.
        self.surface.add(vfdwidgets.Clock(gravity=(-10,1),
                                          priority=0))
        if hasattr(config, 'VFD_TITLE'):
            self.surface.add(vfdwidgets.Text(config.VFD_TITLE,
                                             gravity=(0,-10),
                                             priority=-1))

        # The title of the currently selected item
        self.selectionTitle = vfdwidgets.LoopingScroller(visible=False,
                                                         gravity=(0,-5),
                                                         priority=1)
        self.surface.add(self.selectionTitle)

        # The status of the current media file
        self.surface.add(MediaTitleWidget(gravity=(0,-1), priority=10))
        self.surface.add(MediaTimesWidget(gravity=(10,1), priority=5))

    def drawBegin(self):
        """Called before any draw_* handler, or even if we have no handler
           for the current object type. This resets all widgets to an
           inactive state, so widgets we aren't using will go away.
           """
        self.selectionTitle.visible = False

    def draw(self, (type, object), osd):
        """This handler gives plugins a chance to draw extra information
           on the display. We don't actually draw anything or even update
           the VFD here, but we do collect information about freevo's
           current state and update our widgets with it. We dispatch
           this to draw_* handlers based on the type of object onscreen.
           """
        f = getattr(self, "draw_%s" % type, None)
        self.updater.lock.acquire()
        try:
            self.drawBegin()
            if f:
                f(object, osd)
        finally:
            self.updater.lock.release()

    def draw_menu(self, menuw, osd):
        """Update our widgets with information about the current
           menu and the currently hilighted menu item. This should
           work well enough to give us basic navigation even with
           the TV turned off.
           """
        menu = menuw.menustack[-1]
        self.selectionTitle.visible = True
        self.selectionTitle.text = menu.selected.name

    def eventhandler(self, event, menuw=None):
        """Handle special events containing messages we should display"""
        if event == OSD_MESSAGE:
            self.osdMessage(event.arg)

    def osdMessage(self, message):
        """Show an OSD message, replacing any previous OSD message
           we may have been in the middle of showing.
           """
        # Make this atomic so we don't see the old OSD momentarily
        # disappear if we're replacing one that's still active.
        self.updater.lock.acquire()
        try:
            try:
                self.surface.remove(self.osdWidget)
            except ValueError:
                pass
            w = vfdwidgets.Text(message,
                                gravity  = (0, 100),
                                priority = 100,
                                ellipses = self.ellipses)
            self.surface.add(w, lifetime = 1)
            self.osdWidget = w

            # Bright flash to draw attention to a new message
            self.mi6k.lights.white = 0.3

        finally:
            pass
            self.updater.lock.release()

### The End ###
