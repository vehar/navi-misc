''' GTKsheet.py

This module provides classes for turning an XML character sheet into gtk
widgets for a usable interface.

Copyright (C) 2003 W. Evan Sheehan <evan@navi.picogui.org>
'''

import xml.dom, xml.dom.minidom, GTKsheetElements
from Character import Character


class GTKsheet:
  ''' Assemble the character sheet from the specified XML file using the
      objects in GTKsheetElements.
      '''
  def __init__(self, dataFile):
    self.character = Character()
    self.character.readCharacter(dataFile)
    self.readSheet(self.character.dom.getElementsByTagName('character')[0].getAttribute('layout'))

  def readSheet(self, layoutFile):
    ''' Read the XML file specified by layoutFile into a layout tree.
        '''
    editables = []
    self.dom = xml.dom.minidom.parse(layoutFile)

    # All character sheets must have atleast one <character_sheet> tag.
    if len(self.dom.getElementsByTagName('character_sheet')) < 1:
      print 'Tag Error: no <character_sheet>'
    # For each <character_sheet> create a window.
    else:
      for element in self.dom.getElementsByTagName('character_sheet'):
        self.makeObjects(element, None, editables).show()

  def makeObjects(self, newNode, parent, editList):
    ''' Make objects out of the node passed. '''
    # Tag represents a layout object.
    try:
      newObject = GTKsheetElements.__dict__[newNode.tagName](newNode, self.character)

    # Should do better error handling
    except KeyError:
      print "Unknown tag:", newNode.tagName

    else:
      newObject.addEditable(editList)
      for node in newNode.childNodes:
	if node.nodeType is xml.dom.Node.ELEMENT_NODE: newObject.packChild(self.makeObjects(node, newObject, editList))

      if hasattr(newObject, 'editables'): newObject.editables = editList

      return newObject
