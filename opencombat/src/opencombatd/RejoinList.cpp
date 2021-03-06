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

// interface header 
#include "RejoinList.h"

// common headers
#include "TimeKeeper.h"
#include "PlayerInfo.h"    
#include "StateDatabase.h"  // for StateDatabase::BZDB_REJOINTIME

// bzfs specific headers
#include "CmdLineOptions.h" // for MaxPlayers & ReplayObservers
#include "RecordReplay.h"

// External dependencies  [from bzfs.cxx]
extern PlayerInfo player[MaxPlayers + ReplayObservers];


// it's loathsome to expose private structure in a header
typedef struct RejoinNode {
  char callsign[CallSignLen];
  TimeKeeper joinTime;
} RejoinNode;


RejoinList::RejoinList ()
{
  queue.clear(); // call me paranoid
}

RejoinList::~RejoinList ()
{
  std::list<struct RejoinNode*>::iterator it;
  for (it = queue.begin(); it != queue.end(); it++) {
    RejoinNode *rn = *it;
    delete rn;
  }
  queue.clear();
}

bool RejoinList::add (int playerIndex)
{
  RejoinNode* rn = new RejoinNode;
  strcpy (rn->callsign, player[playerIndex].getCallSign());
  rn->joinTime = TimeKeeper::getCurrent();
  queue.push_back (rn);
  return true;
}

float RejoinList::waitTime (int playerIndex)
{
  std::list<struct RejoinNode*>::iterator it;
  TimeKeeper thenTime = TimeKeeper::getCurrent();
  thenTime += -BZDB.eval(StateDatabase::BZDB_REJOINTIME);
  
  // remove old entries
  it = queue.begin();
  while (it != queue.end()) {
    RejoinNode *rn = *it;
    if (rn->joinTime <= thenTime) {
      delete rn;
      it = queue.erase(it);
      continue;
    }
    it++;
  }

  const char *callsign = player[playerIndex].getCallSign();
  float value = 0.0f;
  it = queue.begin();
  while (it != queue.end()) {
    RejoinNode *rn = *it;
    if (strcasecmp (rn->callsign, callsign) == 0) {
      value = rn->joinTime - thenTime;
    }
    it++;
  }
  
  return value;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
