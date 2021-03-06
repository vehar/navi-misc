""" BZEngine.Event

Event processing utilities, including the default EventLoop class
and an Event class that can transparently convert member functions
into observable and traceable events.
"""
#
# Python BZEngine Package
# Copyright (C) 2003-2006 Micah Dowty <micahjd@users.sourceforge.net>
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

from BZEngine import Errors
import select, time, sys, weakref, types


class Event:
    """An event that can be observed and triggered.
       This event can be called, and all observers will be
       called with the same arguments. Simple and effective :)
       The observers are called in an undefined order.

       The event can be constructed with a list of initial observers.
       This makes it easy to transparently make a member function
       an event with a line like:
         self.foo = Event.Event(self.foo)

       See the attach() function below, it provides a way to
       set up Event wrappers on member functions in bulk.
       """
    def __init__(self, *args):
        self.empty()

        for arg in args:
            self.observe(arg)
        self.unhandledCallback = None

    def observe(self, callback, index=-1):
        """Call the supplied calllback with the same arguments when this event is called.
           Callbacks are referenced weakly, so their observation ends when the callback
           is not used anywhere else.

           This means that lines like the following won't work, since the callback immediately
           becomes unreferenced and is removed:
               foo.observe(lambda: self.boing(5))
               foo.observe(SuperClass().frob)

           To solve these issues you must reference the callback at least one other place.
           This could be in the local namespace of your script, in your class' dictionary, etc.
           It is usually best to replace lambda expressions with class member functions.
           The same is true of functions declared inside another function's body.
           """
        if not self.clients.has_key(hash(callback)):
            if type(callback) is types.MethodType:
                # We tread bound method objects as a special case. Bound methods
                # are created when a class.method expression is evaluated, tying
                # the class and method together into a single callable object.
                # Unfortunately, these objects don't have the lifetime of the class
                # in question, they are usually discarded immediately. Here we
                # detect bound methods, and store proper weakrefs to the method
                # and the class.
                callable = EventMethodObserver(self, callback)
            else:
                # The default wrapper for weak references. This class handles
                # deleting the client list entry properly when the callback
                # is deleted.
                callable = EventObserver(self, callback)

            # Store the callable in the clients hash and in the callables list.
            # Insert it in the given position. We add the list length if our index
            # is negative, to be consistent with python indexing.
            self.clients[hash(callback)] = callable
            if index < 0:
                index += len(self.callables) + 1
            self.callables.insert(index, callable)

    def strongObserve(self, callback, index=-1):
        """Like observe(), but use a normal reference rather than a weak ref. This is
           necessary for using lambdas or other types of temporary functions as obsrvers.
           The only way for these observers to be removed is explicitly calling unobserve().
           """
        if not self.clients.has_key(hash(callback)):
            # We don't need to worry with a wrapper class since we don't need a weakref
            self.clients[hash(callback)] = callback
            if index < 0:
                index += len(self.callables) + 1
            self.callables.insert(index, callback)

    def empty(self):
        # Our client dict maps from callback IDs to callables.
        # This makes using weakref proxy objects easy without making unobserve() a pain.
        # Note that we must use the id rather than the object itself, since the whole
        # point is to avoid requiring a reference to the callable.
        self.clients = {}

        # A list of callables, to preserve insertion order
        self.callables = []

    def replace(self, callback):
        """Remove all existing observers for this event, and add the given one"""
        self.empty()
        self.observe(callback)

    def unobserve(self, callback):
        """Stop calling callback() when this event is triggered"""
        callable = self.clients[hash(callback)]
        del self.clients[hash(callback)]
        self.callables.remove(callable)

    def __call__(self, *args, **kw):
        """Trigger this event by calling it. The parameters passed to the event will
           be broadcast to all of its observers.
           """
        if self.callables:
            for client in self.callables:
                r = client(*args, **kw)
                # Allow the client to abort
                if r:
                    return r
        else:
            # No handlers- can we call the unhandled event callback?
            if self.unhandledCallback:
                self.unhandledCallback(*args, **kw)

    def trace(self, fmt):
        """A debugging aid, prints a line of text whenever this event is triggered.

           fmt can be a printf-style format string that can include references
           to both positional and keyword arguments, for example:
                %(2)s       = The second argument, as a string
                %(boing)d   = keyword argument 'boing', as an integer

           fmt can also be a callable expression (created with lambda, for example)
           that will be called with the event's arguments and the result will be print'ed.

           A reference is returned to the trace's callback function. The trace is
           referenced strongly, so it will not disappear if you discard this reference,
           but the reference can be passed to unobserve() later to cancel the trace.
           """
        def traceCallback(*args, **kw):
            if type(fmt) == str:
                # Make a dictionary with both keyword args and normal
                # args, representing normal args by their place in the
                # argument list, starting with 1.
                index = 1
                for arg in args:
                    kw[str(index)] = arg
                    index += 1
                print fmt % kw
            else:
                print fmt(*args, **kw)
        self.strongObserve(traceCallback)
        return traceCallback


def attach(cls, *args):
    """A convenience function for setting up several transparent
       Event instances. Pass this your class instance and the names
       of all the functions you'd like turned into events.
       If any of the names specified don't exist yet, they are
       still set to a new Event instance, but no callback will
       be associated with it yet.
       """
    for arg in args:
        if hasattr(cls, arg):
            setattr(cls, arg, Event(getattr(cls, arg)))
        else:
            setattr(cls, arg, Event())


class EventObserver:
    """A helper class for Event that wraps the callback in a weakref and
       automatically self-destructs when it's time.
       """
    def __init__(self, event, callback):
        self.event = event
        self.callbackHash = hash(callback)
        self.ref = weakref.ref(callback, self.unref)

    def __call__(self, *args, **kw):
        self.ref()(*args, **kw)

    def unref(self, ref):
        try:
            del self.event.clients[self.callbackHash]
            self.event.callables.remove(self)
        except KeyError:
            # Hmm, something else already deleted it
            pass


class EventMethodObserver:
    """An alternative to EventObserver that is used for bound methods.
       Since bound methods are usually one-time-use objects, this stores the
       unbound method and a weakref to the bound method's instance.
       """
    def __init__(self, event, callback):
        self.event = event
        self.callbackHash = hash(callback)
        self.im_self_ref = weakref.ref(callback.im_self, self.unref)
        self.im_func = callback.im_func

    def __call__(self, *args, **kw):
        im_self = self.im_self_ref()
        if im_self:
            self.im_func(im_self, *args, **kw)
        else:
            raise weakref.ReferenceError

    def unref(self, ref):
        try:
            del self.event.clients[self.callbackHash]
            self.event.callables.remove(self)
        except KeyError:
            # Hmm, something else already deleted it
            pass


class Timer:
    """Abstract base class for a timer that can be added to the EventLoop"""
    def poll(self, now):
        """Check whether this timer should activate, and if so, activate it.
           'now' is the current time. Return 1 if the timer's activation time changed."""
        pass

    def getNextActivation(self):
        """Return the time of next activation"""
        return self.activationTime

    def setEventLoop(self, eventLoop):
        """Called by EventLoop to notify this timer when it is added to an event loop"""
        self.eventLoop = eventLoop

    def activate(self):
        """The timer should call this function when it goes off"""
        self.handler()


class OneshotTimer(Timer):
    """A timer that goes off only once"""
    def __init__(self, period, handler):
        self.handler = handler
        self.activationTime = time.time() + period

    def poll(self, now):
        if now > self.activationTime:
            self.activate()
            self.eventLoop.remove(self)


class PeriodicTimer(Timer):
    """A timer that goes off on regular intervals. If the period
       is zero, the timer goes off once per event loop iteration.
       """
    def __init__(self, period, handler):
        self.handler = handler
        self.period = period
        self.activationTime = time.time() + period

    def poll(self, now):
        if now > self.activationTime:
            self.activate()
            while self.activationTime < now:
                self.activationTime += self.period
            return 1


class ContinuousTimer(Timer):
    """A timer that goes off every event loop iteration"""
    def __init__(self, handler):
        self.handler = handler
        self.activationTime = 0

    def poll(self, now):
        self.activate()


class EventLoop:
    def __init__(self):
        # No polling by default. This can be changed to a duration
        # between polls, or to zero to poll continuously.
        self.pollTime = None
        attach(self, 'onPoll', 'onNonfatalException')
        self.sockets = []
        self.timers  = []
        self.showNonfatalExceptions = 1
        self.nextTimerActivation = None
        self.selectDict = {}

    def updateSelectDict(self):
        """Make a dictionary for quickly detecting which socket has activity"""
        self.selectDict = {}
        for socket in self.sockets:
            selectable = socket.getSelectable()
            self.selectDict[selectable] = socket

    def add(self, item):
        # If the item supports the getSelectable interface, it's a socket
        if hasattr(item, "getSelectable"):
            self.sockets.append(item)
            self.updateSelectDict()

        elif isinstance(item, Timer):
            self.timers.append(item)
            item.setEventLoop(self)
            self.updateNextTimerActivation()

        else:
            raise TypeError("Only Sockets and Timers are supported by this event loop")

    def remove(self, item):
        # If the item supports the getSelectable interface, it's a socket
        if hasattr(item, "getSelectable"):
            self.sockets.remove(item)
            self.updateSelectDict()

        elif isinstance(item, Timer):
            self.timers.remove(item)
            self.updateNextTimerActivation()

        else:
            raise TypeError("Only Sockets and Timers are supported by this event loop")

    def updateNextTimerActivation(self):
        """Updates time of the next timer activation"""
        # There are better ways to do this...
        self.nextTimerActivation = None
        for timer in self.timers:
            x = timer.getNextActivation()
            if self.nextTimerActivation is None or x < self.nextTimerActivation:
                self.nextTimerActivation = x

    def run(self):
        self.running = 1
        try:
            while self.running:
                # The poll time we'll actually use depends on both pollTime and nextTimerActivation.
                pollTime = self.pollTime
                if self.nextTimerActivation is not None:
                    untilNextTimer = self.nextTimerActivation - time.time()
                    if untilNextTimer < 0:
                        untilNextTimer = 0
                    if pollTime is None:
                        pollTime = untilNextTimer
                    else:
                        if untilNextTimer < pollTime:
                            pollTime = untilNextTimer

                if self.selectDict:
                    # This waits until either a socket has activity, or
                    # our pollTime has expired and we need to check timers
                    try:
                        # Interrupt if we can read from any socket
                        iwtd = self.selectDict.keys()
                        # Only interrupt if we can write on sockets that need to write
                        owtd = []
                        for s in iwtd:
                            if self.selectDict[s].needsWrite():
                                owtd.append(s)
                        (iwtd, owtd, ewtd) = self.select(iwtd, owtd, [], pollTime)
                    except select.error:
                        raise Errors.ConnectionLost()

                    # Poll available sockets, for reading and for writing
                    for ready in iwtd:
                        try:
                            self.selectDict[ready].pollRead(self)
                        except Errors.NonfatalException:
                            self.onNonfatalException(sys.exc_info())
                    for ready in owtd:
                        try:
                            self.selectDict[ready].pollWrite(self)
                        except Errors.NonfatalException:
                            self.onNonfatalException(sys.exc_info())
                else:
                    # No sockets to wait on. We can't call select with three
                    # empty lists on win32, so this has to be a special case.
                    if pollTime > 0:
                        time.sleep(pollTime)

                # Poll timers, updating the time of next activation if necessary
                now = time.time()
                timesChanged = 0
                for timer in self.timers:
                    if timer.poll(now):
                        timesChanged = 1
                if timesChanged:
                    self.updateNextTimerActivation()

                # Call our generic poll event hook
                self.onPoll()
        finally:
            self.running = 0

    def stop(self):
        self.running = 0

    def select(self, iwtd, owtd, ewtd, time):
        """This is a hook for subclasses to easily override the
           select function that this event loop uses.
           """
        return select.select(iwtd, owtd, ewtd, time)

    def onNonfatalException(self, info):
        if self.showNonfatalExceptions:
            print "*** %s : %s" % (info[1].__class__.__name__, info[1])

### The End ###
