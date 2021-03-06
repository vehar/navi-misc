"""A module providing classes for graphs built from a corpus of motion.

A collection of functions and classes used to create graphs built from motion
capture data.  The nodes in the graph represent a range of positions for a
single joint.  Edges in the graph have a probability associated with them,
indicating the likelihood that the transition from u to v along that edge
occured in the corpus of motion capture data.

:Authors:
    David Trowbridge
"""

from Graph.Algorithms import DotPrint
from Graph.Data import Graph, Edge, AdjacencyList, VertexMap, EdgeList
from Graph import algorithms_c, Dot
from Motion import AMC
import Numeric, MLab, math, string, gc

class ProbabilityEdge (Edge):
    """Represent an edge between nodes with a probability of following that edge.

    Members:
        - `u`            The starting node of the edge
        - `v`            The ending node of the edge
        - `dot_label`    The string to print next to the edge
        - `count`        The number of times the transition from u to v was made
        - `weight`       The probability of traversing the edge
    """

    __slots__ = ['u', 'v', 'dot_label', 'count', 'weight']

    def __init__ (self, u, v):
        """Create an edge from `u` to `v` with count 0 and weight `None`."""
        Edge.__init__ (self, u, v)
        self.count = 0
        self.weight = None

    def visit (self):
        """Increase the count of the edge by one."""
        self.count += 1

    def normalize (self, total):
        """Calculate the weight of the edge.

        The weight of the edge is equal to the number of times the edge has
        been visited divided by ``total``.
        """
        self.weight = float (self.count) / total
        self.dot_label = '%.2f' % self.weight

    def __getstate__ (self):
        return self.u, self.v, self.dotAttrs, self.count, self.weight

    def __setstate__ (self, state):
        self.u, self.v, self.dotAttrs, self.count, self.weight = state

class MotionGraph (Graph):
    """Represent a graph of motion.
   
    `MotionGraph`\s are just a `Graph` object created with a `DotPrint`
    algorithm.
    """

    def __init__ (self):
        """Initialize the graph with a `DotPrint` algorithm."""
        Graph.__init__ (self, [DotPrint])

class MotionGraphNode (Dot.Node):
    """Represent a node in a `MotionGraph`.

    A node in a `MotionGraphNode` represents a range of positions. Each node
    represents any joint position that falls within the upper and lower bounds
    of the node.

    Members:
        - ``mins``      Lower bound of this node
        - ``maxs``      Upper bound of this node
    """

    __slots__ = ['mins', 'maxs', 'dotAttrs']

    def __init__ (self, mins, maxs):
        """Initialize the node with bounds `mins` and `maxs`."""
        self.mins = mins
        self.maxs = maxs
        self.center = []

        extents = []
        for i in range (len (mins)):
            extents.append ('[%.2f, %.2f]' % (mins[i], maxs[i]))
            self.center.append (mins[i] + (maxs[i] - mins[i]) / 2.0)
        self.dotAttrs = {'label': '\\n'.join (extents)}

    def __repr__ (self):
        return '<MGNode: %s>' % self.dotAttrs['label']

    def __str__ (self):
        """Generate a string suitable for printing to a .dot file.

        Returns:
            A string representing the node formatted for a .dot file for graph
            printing using dot(1)
        """
        id = str (hash (self))
        attrs = ['%s="%s"' % (key, value) for key, value in \
                self.dotAttrs.iteritems ()]
        return '%s [%s];' % (id, string.join (attrs, ','))

    def inside (self, point):
        """Return `True` if `point` lies within the minimum and maximum bounds of
        this node.
        """
        if len (point) != len (self.mins):
            raise AttributeError ("dimension doesn't match! %d vs %d" % (len (point), len (self.mins)))
        for i in range (len (point)):
            if point[i] < self.mins[i] or point[i] > self.maxs[i]:
                return False
        return True

def search_graphs (graphs, starts, ends, depth):
    """Execute a depth limited search on multiple graphs and return paths for
    each graph, if there is one.

    `graphs`, `starts`, and `ends` should all be dictionaries with identical keys.

    Arguments:
        - `graphs`    A dictionary mapping a key to a motion graph
        - `starts`    A dictionary mapping a key to a position representing
          the start position
        - `ends`      A dictionary mapping a key to a position representing
          the goal of the search
        - `depth`     The maximum depth to run the search to
    """
    paths = {}
    for bone in graphs.keys():
        print '    searching',bone
        adjacency = graphs[bone].representations[AdjacencyList]
        edges     = graphs[bone].representations[EdgeList]
        paths[bone] = algorithms_c.depthLimitedSearch (adjacency, edges, starts[bone], ends[bone], depth)

    retpaths = None
    for i in range (len (paths['root']) - 1):
        coverage = 0
        # If all the paths at this depth are real, we're done
        match = True
        for bone in graphs.keys ():
            if paths[bone][i] is None:
                match = False
            else:
                coverage = coverage + 1

        if match:
            print '    depth = %2d, match!' % i + 1
            retpaths = {}
            for bone,path in paths.items ():
                retpaths[bone] = path[i]
            break
        else:
            print '    depth = %2d, coverage = %2d/%2d, no match' % (i + 1, coverage, len (graphs.keys ()))
    paths = None
    gc.collect ()
    return retpaths

def cp_range (dof, angle):
    """Return a list of tuples which form a cartesian product of the range
       [0, 360] in steps of `angle`.
       
       This is used to build our sea of nodes for building the graph.
       """
    if dof == 1:
        for x in range (0, 360, angle):
            yield [x]
        return

    for x in range (0, 360, angle):
        for y in cp_range (dof - 1, angle):
            yield y + [x]

def fixnegative (x):
    """If `x` is negative, add 360 to it until it is positive and return it."""
    while x < 0:
        x = x + 360
    return x

def fix360 (x):
    """If x is 360, make it 0 and return it."""
    if x == 360:
        return 0
    return x

def discretize (point, interval):
    c = [int (n / interval) * interval for n in map (fixnegative, point)]
    return tuple (map (fix360, c))

def build_graphs (key, datas, interval):
    """Build a graph using the data arrays from any number of files.
   
    Arguments:
        - `key`      The key for which the graph should be built
        - `datas`    A dictionary of motion data, the values in the
          dictionary are Numeric arrays
    """
    # If this is the root, we only want the last 3 dof, for now
    # FIXME - we really should track root position, but that's more
    # complicated.  We also don't *know* that the last 3 DOF are going
    # to be orientation, that's just convention.
    if key == 'root':
        datas = [d[:,3:6] for d in datas]
    # It's silly to have angle values outside of [0, 360]
    datas = [Numeric.remainder (d, 360.0) for d in datas]

    # This assumes the same DOF for each bone in each file!
    dof = datas[0].shape[1]

    graph          = MotionGraph   ()
    adjacency_list = AdjacencyList (graph)
    vertex_map     = VertexMap     (graph)
    edge_list      = EdgeList      (graph)

    # Create list of nodes
    nodes = {}
    for angle in cp_range (dof, interval):
        node = MotionGraphNode ([n for n in angle], [n + interval for n in angle])
        nodes[tuple(angle)] = node

    # Add edges for data from each file
    for d in datas:
        build_graph (d, graph, nodes, edge_list, interval)

    # Normalize probabilities for all edges coming out of a vertex. The sum
    # of probabilities along edges coming out of any given vertex will be 1.
    for vertex in vertex_map:
        total = 0
        edges = vertex_map.query (vertex)
        for edge in edges:
            if edge.u is vertex:
                total += edge.count
        for edge in edges:
            if edge.u is vertex:
                edge.normalize (total)

    return graph

def build_graph (d, graph, nodes, edge_list, interval):
    """Build a single graph.

    Creates a single graph from `nodes` and `edge_list`. Each node in the
    graph represents a position within an interval `interval`.

    Arguments:
        - `d` Numeric array of data from which the graph is built
        - `graph` The graph
        - `nodes` A dictionary of nodes mapping an angle to a MotionGraphNode
        - `edge_list` An empty EdgeList for graph to be filled in here
        - `interval` The interval contained within each graph node

    """
    frames = d.shape[0]

    # find edges
    data1 = d[0,:]
    bases = tuple (int (n / interval) * interval for n in tuple (map (fixnegative, data1)))
    bases = tuple (map (fix360, bases))
    node1 = nodes[bases]

    for frame in range (1, frames):
        node2 = None
        data2 = d[frame,:]
        bases = tuple (int (n / interval) * interval for n in tuple (map (fixnegative, data2)))
        bases = tuple (map (fix360, bases))
        node2 = nodes[bases]

        # check to see if it's a self-loop
        if node1 is node2:
            try:
                edge = edge_list.query (node1, node2)
                edge.visit ()
            except KeyError:
                try:
                    edge = edge_list.query (node2, node1)
                    edge.visit ()
                except KeyError:
                    edge = ProbabilityEdge (node1, node2)
                    edge.visit ()
                    graph.addList ([edge])
        else:
            try:
                edge = edge_list.query (node1, node2)
                edge.visit ()
            except KeyError:
                edge = ProbabilityEdge (node1, node2)
                edge.visit ()
                graph.addList ([edge])
            try:
                edge = edge_list.query (node2, node1)
                edge.visit ()
            except KeyError:
                edge = ProbabilityEdge (node2, node1)
                edge.visit ()
                graph.addList ([edge])
        data1 = data2
        node1 = node2

def build_bayes (asf, amcs, interval):

    nets = {}

    # Build a dictionary mapping parent bones to their list of children from
    # the ASF file.
    relationships = {}
    for rel in asf.hierarchy:
        parent = rel[0]
        children = rel[1:]
        if len (parent):
            relationships[parent] = children
            # Check each child bone to see if it's in the mocap data. If it is,
            # it has DOF and moves, so it needs a Bayes net.
            for child in children:
                if child in amcs[0].bones:
                    # Set up an empty bayes net for the bone.
                    nets[child] = {}

    print 'building bayes nets'

    # Handle root separately
    print 'building net for root'
    net = {}
    total = 0
    for amc in amcs:
        bone = amc.bones['root']
        total += len (bone)
        for frame in range(len(bone)):
            c = discretize (bone[frame,3:6], interval)
            spot = ((), c)
            if spot in net:
                net[spot] += 1
            else:
                net[spot] = 1

    for spot, count in net.iteritems ():
        net[spot] = float (count) / float (total)
    nets['root'] = net

    for parent, children in relationships.iteritems ():
        for child in children:
            print 'building net for %s' % child

            total = 0
            if child not in amcs[0].bones:
                # If the child has no DOF, we don't need to build a net at
                # all, since we won't be interpolating that bone
                continue

            if parent not in amcs[0].bones:
                # If the parent has no DOF, the bayes net for this
                # relationship simplifies to a simple histogram of the
                # bone positions
                net = nets[child]
                for amc in amcs:
                    cbone = amc.bones[child]
                    total += len (cbone)

                    # Chomp to within interval
                    for frame in range(len(cbone)):
                        c = discretize (cbone[frame,:], interval)
                        if ((), c) in net:
                            net[((), c)] = net[((), c)] + 1
                        else:
                            net[((), c)] = 1
            else:
                net = nets[child]

                for amc in amcs:
                    pbone = amc.bones[parent]
                    cbone = amc.bones[child]
                    total += len (cbone)

                    for frame in range(len(pbone)):
                        p = discretize (pbone[frame,-3:], interval)
                        c = discretize (cbone[frame,:], interval)

                        if (p, c) in net:
                            net[(p, c)] = net[(p, c)] + 1
                        else:
                            net[(p, c)] = 1

            for spot, count in net.iteritems ():
                net[spot] = float (count) / float (total)

    return nets

# vim: ts=4:sw=4:et
