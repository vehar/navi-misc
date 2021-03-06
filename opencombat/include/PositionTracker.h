/* bzflag
 * Copyright (c) 1993 - 2004 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named LICENSE that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __POSITIONTRACKER_H__
#define __POSTIIONTRACKER_H__

/* system interface headers */
#include <string>
#include <vector>
#include <map>
#include <limits.h>

/* common interface headers */
#include "TimeKeeper.h"


/** PositionTracker keeps track of where things are in a 3D Cartesian
 * coordinate space.  It can give introspective information like where
 * things are and how close other tracked objects are.  Objects may be
 * grouped into separate categories for subsearching.
 */
class PositionTracker
{
private:

  /** something that we are tracking
   */
  typedef struct item {
    std::string id;
    TimeKeeper lastUpdate;
    double position[3]; // position in Cartesian 3-space coodinates
    bool forgotten;
  } item_t;

  typedef std::vector<item_t *> TrackedItemVector;

  /** the actual collection of things being tracked are categorized
   *  into some provided group name.  using that group name (which is
   *  "" if not provided), we get back a set of tracked objects.
   */
  std::map <std::string, TrackedItemVector> _trackedItem;

protected:

public:

  PositionTracker();
  PositionTracker(const PositionTracker& tracker);
  ~PositionTracker();

  /** inform the tracker that there is intent to track something new.
   * tracking items may be categorized using a provided group name.  a
   * token is returned that must be used for subsequent updates.
   */
  unsigned short track(const std::string id, std::string group = std::string(""));

  /** update the position of a tracked item.  returns truthfully
   * whether the update could be performed (i.e. whether the id
   * existed).  the token and string id are both required for
   * validation.
   */
  bool update(unsigned short int token, const std::string id, const double position[3], std::string group = std::string(""));

  /** stop tracking something if it was being tracked.  the token and
   * string id are both required for validation.
   */
  bool forget(unsigned short int token, const std::string id, std::string group = std::string(""));

  /** compute the distance between two tracked items given their tokens.
   */
  double distanceBetween(unsigned short int from, unsigned short int to, std::string fromGroup=std::string(""), std::string toGroup=std::string("")) const;

};

#else
class PositionTracker;
#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
