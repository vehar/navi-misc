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

/* interface header */
#include "PlayerInfo.h"

// system headers
#include <errno.h>

/* system implementation headers */
#include <assert.h>

// implementation-specific bzflag headers
#include "TextUtils.h"

PlayerInfo::PlayerInfo() {
  state = PlayerNoExist;
  tracker = 0;
}

std::string PlayerInfo::getLastMsg() {
  return lastMsgSent;
}

TimeKeeper PlayerInfo::getLastMsgTime() {
  return lastMsgTime;
}

int PlayerInfo::getSpamWarns() {
  return spamWarns;
}

void PlayerInfo::incSpamWarns() {
  ++spamWarns;
}

void PlayerInfo::setSpamWarns() {
  spamWarns = 0;
}

void PlayerInfo::setLastMsg(std::string msg) {
  lastMsgSent = msg;
  lastMsgTime = TimeKeeper::getCurrent();
}

void PlayerInfo::initPlayer(int _playerIndex) {
  playerIndex      = _playerIndex;

  state = PlayerInLimbo;
  paused = false;
  pausedSince = TimeKeeper::getNullTime();
  flag        = -1;
}

void PlayerInfo::resetPlayer(bool ctf) {
  wasRabbit = false;

  lastupdate = TimeKeeper::getCurrent();
  lastmsg    = TimeKeeper::getCurrent();

  replayState = ReplayNone;

#ifdef TIMELIMIT
  playedEarly = false;
#endif

  restartOnBase = ctf;
}

void PlayerInfo::removePlayer() {

  callSign[0] = 0;

  state = PlayerNoExist;
}

void PlayerInfo::setRestartOnBase(bool on) {
  restartOnBase = on;
};

bool PlayerInfo::shouldRestartAtBase() {
  return restartOnBase;
};

bool PlayerInfo::isPlaying() {
  return state > PlayerInLimbo;
};

bool PlayerInfo::exist() {
  return state != PlayerNoExist;
};

void PlayerInfo::signingOn() {
  state = PlayerDead;
};

bool PlayerInfo::isAlive() {
  return state == PlayerAlive;
};

bool PlayerInfo::isDead() {
  return state == PlayerDead;
};

void PlayerInfo::setAlive() {
  state = PlayerAlive;
  flag = -1;
};

void PlayerInfo::setDead() {
  state = PlayerDead;
};

bool PlayerInfo::isPaused() {
  return paused;
};

bool PlayerInfo::isNotResponding() {
  return notResponding;
};

bool PlayerInfo::isBot() {
  return type == ComputerPlayer;
};

bool PlayerInfo::isHuman() {
  return type == TankPlayer;
};

void *PlayerInfo::packUpdate(void *buf) {
  buf = nboPackUShort(buf, uint16_t(type));
  buf = nboPackUShort(buf, uint16_t(team));
  return buf;
};

void *PlayerInfo::packId(void *buf) {
  buf = nboPackString(buf, callSign, CallSignLen);
  buf = nboPackString(buf, email, EmailLen);
  return buf;
}

void PlayerInfo::unpackEnter(void *buf) {
  // data: type, team, name, email
  uint16_t _type;
  int16_t _team;
  buf = nboUnpackUShort(buf, _type);
  buf = nboUnpackShort(buf, _team);
  type = PlayerType(_type);
  team = TeamColor(_team);
  buf = nboUnpackString(buf, callSign, CallSignLen);
  buf = nboUnpackString(buf, email, EmailLen);
  cleanCallSign();
  cleanEMail();
};

const char *PlayerInfo::getCallSign() const {
  return callSign;
};

void PlayerInfo::cleanCallSign() {
  // strip leading whitespace from callsign
  char *sp = callSign;
  char *tp = sp;
  while (isspace(*sp))
    sp++;

  // strip any non-printable characters and ' and " from callsign
  do {
    if (isprint(*sp) && (*sp != '\'') && (*sp != '"')) {
      *tp++ = *sp;
    }
  } while (*++sp);
  *tp = *sp;

  // strip trailing whitespace from callsign
  while (isspace(*--tp)) {
    *tp=0;
  }
};

bool PlayerInfo::isCallSignReadable() {
  // callsign readability filter, make sure there are more alphanum than non
  // keep a count of alpha-numerics
  int alnumCount = 0;
  const char *sp = callSign;
  do {
    if (isalnum(*sp)) {
      alnumCount++;
    }
  } while (*++sp);
  int callsignlen = strlen(callSign);
  return (callsignlen <= 4) || ((float)alnumCount / (float)callsignlen > 0.5f);
};

const char *PlayerInfo::getEMail() const {
  return email;
};

void PlayerInfo::cleanEMail() {
  // strip leading whitespace from email
  char *sp = email;
  char *tp = sp;
  while (isspace(*sp))
    sp++;

  // strip any non-printable characters and ' and " from email
  do {
    if (isprint(*sp) && (*sp != '\'') && (*sp != '"')) {
      *tp++ = *sp;
    }
  } while (*++sp);
  *tp = *sp;

  // strip trailing whitespace from email
  while (isspace(*--tp)) {
    *tp=0;
  }
};

bool PlayerInfo::isEMailReadable() {
  // email/"team" readability filter, make sure there are more
  // alphanum than non
  int emailAlnumCount = 0;
  char *sp = email;
  do {
    if (isalnum(*sp)) {
      emailAlnumCount++;
    }
  } while (*++sp);
  int emaillen = strlen(email);
  return (emaillen <= 4) || (((float)emailAlnumCount / (float)emaillen) > 0.5);
};

void *PlayerInfo::packVirtualFlagCapture(void *buf) {
  buf = nboPackUShort(buf, uint16_t(int(team) - 1));
  buf = nboPackUShort(buf, uint16_t(1 + (int(team) % 4)));
  return buf;
};

bool PlayerInfo::isTeam(TeamColor _team) const {
  return team == _team;
};

bool PlayerInfo::isObserver() const {
  return team == ObserverTeam;
};

TeamColor PlayerInfo::getTeam() {
  return team;
};

void PlayerInfo::setTeam(TeamColor _team) {
  team = _team;
};

void PlayerInfo::wasARabbit() {
  team = RogueTeam;
  wasRabbit = true;
};

void PlayerInfo::wasNotARabbit() {
  wasRabbit = false;
};

bool PlayerInfo::isARabbitKill(PlayerInfo &victim) {
  return wasRabbit || victim.team == RabbitTeam;
};

void PlayerInfo::resetFlag() {
  flag = -1;
  lastFlagDropTime = TimeKeeper::getCurrent();
};

bool PlayerInfo::haveFlag() const {
  return flag >= 0;
}
int PlayerInfo::getFlag() const {
  return flag;
};

void PlayerInfo::setFlag(int _flag) {
  flag = _flag;
};

bool PlayerInfo::isFlagTransitSafe() {
  return TimeKeeper::getCurrent() - lastFlagDropTime >= 2.0f;
};

const char *PlayerInfo::getClientVersion() {
  return clientVersion.c_str();
};

void *PlayerInfo::setClientVersion(size_t length, void *buf) {
  char *versionString = new char[length];
  buf = nboUnpackString(buf, versionString, length);
  clientVersion = std::string(versionString);
  delete[] versionString;
  DEBUG2("Player %s [%d] sent version string: %s\n", 
	 callSign, playerIndex, clientVersion.c_str());
  return buf;
}

std::string PlayerInfo::getIdleStat() {
  TimeKeeper now = TimeKeeper::getCurrent();
  std::string reply;
  if ((state > PlayerInLimbo) && (team != ObserverTeam)) {
    reply = string_util::format("%-16s : %4ds", callSign,
				int(now - lastupdate));
    if (paused) {
      reply += string_util::format("  paused %4ds", int(now - pausedSince));
    }
  }
  return reply;
};

bool PlayerInfo::canBeRabbit(bool relaxing) {
  if (paused || notResponding || (team == ObserverTeam))
    return false;
  return relaxing ? (state > PlayerInLimbo) : (state == PlayerAlive);
};

void PlayerInfo::setPaused(bool _paused) {
  paused = _paused;
  pausedSince = TimeKeeper::getCurrent();
};

bool PlayerInfo::isTooMuchIdling(TimeKeeper tm, float kickThresh) {
  bool idling = false;
  if ((state > PlayerInLimbo) && (team != ObserverTeam)) {
    int idletime = (int)(tm - lastupdate);
    int pausetime = 0;
    if (paused && tm - pausedSince > idletime)
      pausetime = (int)(tm - pausedSince);
    idletime = idletime > pausetime ? idletime : pausetime;
    if (idletime > (tm - lastmsg < kickThresh ? 3 * kickThresh : kickThresh)) {
      DEBUG1("Kicking player %s [%d] idle %d\n", callSign, playerIndex,
	     idletime);
      idling = true;
    }
  }
  return idling;
};

bool PlayerInfo::hasStartedToNotRespond() {
  float notRespondingTime = BZDB.eval(StateDatabase::BZDB_NOTRESPONDINGTIME);
  bool startingToNotRespond = false;
  if (state > PlayerInLimbo) {
    bool oldnr = notResponding;
    notResponding = (TimeKeeper::getCurrent() - lastupdate)
      > notRespondingTime;
    if (!oldnr && notResponding)
      startingToNotRespond = true;
  }
  return startingToNotRespond;
}

void PlayerInfo::hasSent(char message[]) {
  lastmsg = TimeKeeper::getCurrent();
  DEBUG1("Player %s [%d]: %s\n", callSign, playerIndex, message);
};

bool PlayerInfo::hasPlayedEarly() {
  bool returnValue = playedEarly;
  playedEarly      = false;
  return returnValue;
};

void PlayerInfo::setPlayedEarly() {
  playedEarly = true;
};

void PlayerInfo::updateIdleTime() {
  lastupdate = TimeKeeper::getCurrent();
};

void        PlayerInfo::setReplayState(PlayerReplayState state) {
  replayState = state;
}

PlayerReplayState PlayerInfo::getReplayState()
{
  return replayState;
}


void PlayerInfo::setTrackerID(unsigned short int t)
{
  tracker = t;
}


unsigned short int PlayerInfo::trackerID()
{
  return tracker;
}


// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
