#!/usr/bin/env python
#
# rebuild.py
#
# Send a command over XML-RPC to the CIA server triggering
# a rebuild of all applicable python modules using
# twisted.python.rebuild. This loads the latest code from
# disk, and replaces references to the old code with references
# to the new code.
#

import xmlrpclib
from LibCIA.Security import CapabilityDB

cap = CapabilityDB("data/security.db").get('rebuild')
s = xmlrpclib.ServerProxy("http://localhost:3910")

s.sys.rebuild(cap)

### The End ###
