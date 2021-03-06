/* bzflag
 * Copyright (c) 1993 - 2004 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef _MSC_VER
#pragma warning( 4: 4786 )
#endif

#include "common.h"
// interface header
#include "WorldFileObject.h"


bool WorldFileObject::read(const char *cmd, std::istream& input)
{
  std::string name;
  if (strcasecmp(cmd, "name") == 0) {
    //This is currently unused, but can be used for documentation purposes
    input >> name;
    return true;
  }
  return false;
}

/** delete all of the world file objects from a vector list
 */
void emptyWorldFileObjectList(std::vector<WorldFileObject*>& wlist)
{
  const int n = wlist.size();
  for (int i = 0; i < n; ++i) {
    delete wlist[i];
  }
  wlist.clear();
}

// Local variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
