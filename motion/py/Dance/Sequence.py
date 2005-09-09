""" Sequence

Map a sequence to a chaotic attractor and shuffle it.
"""

import Motion, Numeric, math

__all__ = ["Sequence"]

class Sequence:
    """ This class represents a dance sequence mapped to an attractor.
        """
    def __init__ (self, ode, data):
        """ Each Sequence object requires an ordinary differential equation
            solver and some motion capture data (data).
            """
        self.mapping = {}

        traj = ode ()
        step = len (traj) / len (data)
        length = len (len) * step

        # Map each frame evenly over the trajectory.
        for i in range (length):
            self.mapping[traj[i * step]] = _Frame (i, data)

    def __getitem__ (self, frame):
        """ Use [] on a Sequence object to get a frame. """
        return self.mapping.items () [frame]

    def _distance (self, x, y):
        """ Calculate the Euclidean distance between the points x and y. """
        return math.sqrt (Numeric.sum ((y - x)**2))

    def shuffle (self, ics):
        """ Takes a set of initial conditions (ics) and returns a new
            sequence object containing the shuffled dance.
            """
        pass

    def save (self):
        """ Store the sequence to a file. """
        pass

class _Frame:
    """ A frame from the AMC file. """
    def __init__ (self, index, data):
        self.__bones = {}

        for bone, values in data.iteritems ():
            self.__bones[bone] = values[index]

    def __getitem__ (self, bone):
        return self.__bones[bone]

# vim:ts=4:sw=4:et:tw=80
