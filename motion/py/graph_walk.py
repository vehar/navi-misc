#!/usr/bin/env python

"""Create an animation by traversing a motion graph.

usage: graph_walk.py [options] <graph pickle>
"""

from optparse import OptionParser
from random import random
from Dance import Sequence
from Graph import MotionGraph
from Graph.Data import AdjacencyList, VertexMap
from Motion.Interpolate import spline
import Motion, Numeric, pickle, random

def clicheWalk (start, vMap, len):
    """Find a path in graph of length len by following the edges with the
       highest probabilities.
       """
    u = start
    path = [u.center]

    for i in range (len):
        choice = None
        weight = 0
        # Check all the edges associated with this vertex.
        for edge in vMap.query (u):
            # If this vertex is the start of the edge (not the end) and the
            # edge is more likely than any other we've seen, it becomes our next
            # edge.
            if edge.u == u and edge.weight >= weight:
                weight = edge.weight
                choice = edge.v

        # Append the next vertex and set it as the current vertex.
        path.append (choice.center)
        u = choice

    return path

def weightedWalk(start, vMap, length):
    """Perform a weighted graph walk.

    Unlike the cliche walk, this walk does not choose the node with the highest
    probability. Instead, it assigns a probability range to each node based on
    its probability and then chooses the node whose probability range contains
    a randomly generated probability.
    """
    # Choose a random starting point and initialize the path with it
    u = start
    path = [u.center]

    for i in range (length):
        # Choose a random weight
        weight = random.random()
        # Keep a running sum of probabilities
        p = 0.0
        # Generate a list of edges coming out of u and sort according to weight
        edges = []
        for edge in vMap.query(u):
            if edge.u == u:
                edges.append(edge)
        edges.sort(cmp=lambda x,y: cmp(x.weight, y.weight))

        # By iterating over the edges in order of ascending weights and summing
        # the seen probabilities, we can just compare the randomly generated
        # weight to the summed probability. If the weight is less than the summed
        # probability, we know that this is the edge whose probability range
        # contains the randomly generated weight.
        for edge in edges:
            p += edge.weight
            if weight <= p:
                path.append(edge.v.center)
                continue

    return path

def randomWalk (start, adjList, len):
    """Find a path in graph of langth len by following a random edge."""

    # Choose a random starting place.
    u = start
    path = [u.center]

    for i in range (len):
        # Choose a random edge coming out of u.
        choice = random.choice ([edge for edge in adjList.query (u)])
        # Append the end of that edge to the path.
        path.append (choice.v.center)
        # Set the current vertex to this new vertex
        u = choice.v

    return path


parser = OptionParser ("%prog [options] <graph pickle> <file name>")
parser.add_option("--type", dest="type", default='all',
        help="Comma separated list of walks to perform (default: all)")
parser.add_option("-l", "--len", dest="len", default=1000,
                   type="int", help="Set length of paths")
parser.add_option("-q", "--quality", dest="quality", default=5,
                  type="int",
                  help="Set the number of points to insert during interpolation")

opts, args = parser.parse_args()

if len (args) < 1:
    parser.error("graph file required")

graphs = pickle.load(open(args[0]))
walks = {'cliche': {}, 'random': {}, 'weighted': {}}
smoothed = {}
adjacencyLists = graphs.representations[AdjacencyList]
for bone, adjList in adjacencyLists.data.items ():
    vMap = graphs.representations[VertexMap].data[bone]
    start = random.choice([v for v in vMap])
    if 'cliche' in opts.type or 'all' in opts.type:
        walks['cliche'][bone] = clicheWalk (start, vMap, opts.len)
    if 'random' in opts.type or 'all' in opts.type:
        walks['random'][bone] = randomWalk (start, adjList, opts.len)
    if 'weighted' in opts.type or 'all' in opts.type:
        walks['weighted'][bone] = weightedWalk (start, vMap, opts.len)

for name, walk in walks.iteritems():
    if len(walk) is 0:
        # Skip walks we didn't do
        continue

    # FIXME - Hack to print out the data because I had some trouble using AMC.save()
    f = open('-'.join([args[-1], name]) + '.amc', 'w')
    f.writelines([':FULLY-SPECIFIED\n', ':DEGREES\n'])

    for i in range (opts.len):
        f.write(str(i) + '\n')
        for bone, frames in walk.iteritems ():
            if bone == "root":
                s = bone + " 0 0 0"
            else:
                s = bone

            for angle in frames[i]:
                a = " %6f" % (angle)
                s += a
            f.write(s + '\n')
    f.close()

# vim:ts=4:sw=4:et
