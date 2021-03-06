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

static const char copyright[] = "Copyright (c) 1993 - 2004 Tim Riker";

#include "common.h"

// system includes
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#ifdef _WIN32
#include <shlobj.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>
#endif
#include "../zlib/zconf.h"
#include "../zlib/zlib.h"

// yikes! that's a lotsa includes!
#include "global.h"
#include "bzsignal.h"
#include "Address.h"
#include "BzfEvent.h"
#include "BzfWindow.h"
#include "BzfMedia.h"
#include "PlatformFactory.h"
#include "Protocol.h"
#include "Pack.h"
#include "ServerLink.h"
#include "SceneBuilder.h"
#include "SceneDatabase.h"
#include "BackgroundRenderer.h"
#include "RadarRenderer.h"
#include "HUDRenderer.h"
#include "HUDui.h"
#include "World.h"
#include "WorldBuilder.h"
#include "Team.h"
#include "FileManager.h"
#include "Flag.h"
#include "LocalPlayer.h"
#include "RemotePlayer.h"
#include "WorldPlayer.h"
#include "RobotPlayer.h"
#include "ControlPanel.h"
#include "ShotStrategy.h"
#include "StateDatabase.h"
#include "KeyManager.h"
#include "CommandManager.h"
#include "daylight.h"
#include "sound.h"
#include "TimeBomb.h"
#include "HUDDialogStack.h"
#include "ErrorHandler.h"
#include "ZSceneDatabase.h"
#include "QuadWallSceneNode.h"
#include "BillboardSceneNode.h"
#include "Intersect.h"
#include "BZDBCache.h"
#include "WordFilter.h"
#include "TextUtils.h"
#include "TargetingUtils.h"
#include "MainMenu.h"
#include "ComposeDefaultKey.h"
#include "SilenceDefaultKey.h"
#include "ServerCommandKey.h"
#include "Roster.h"
#include "FlashClock.h"
#include "CommandsStandard.h"
#include "commands.h"
#include "DirectoryNames.h"
#include "AnsiCodes.h"
#include "TextureManager.h"
#include "VisualElementManager.h"
#include "DrawablesManager.h"
#include "3DView.h"


// versioning that makes us recompile every time
#include "version.h"

// get our interface
#include "playing.h"


static const float	FlagHelpDuration = 60.0f;
static StartupInfo	startupInfo;
static MainMenu*	mainMenu;
ServerLink*		serverLink = NULL;
World*		world = NULL;
LocalPlayer*		myTank = NULL;
static BzfDisplay*	display = NULL;
MainWindow*	mainWindow = NULL;
static SceneRenderer*	sceneRenderer = NULL;
static SceneDatabase*	zScene = NULL;
static SceneDatabase*	bspScene = NULL;
ControlPanel*		controlPanel = NULL;
static RadarRenderer*	radar = NULL;
HUDRenderer*		hud = NULL;
static SceneDatabaseBuilder* sceneBuilder = NULL;
static Team*		teams = NULL;
int			numFlags = 0;
static JoinGameCallback	joinGameCallback = NULL;
static void*		joinGameUserData = NULL;
bool			admin = false; // am I an admin?
static bool		serverError = false;
static bool		serverDied = false;
bool		fireButton = false;
bool             roamButton = false;
static bool		firstLife = false;
static bool		showFPS = false;
static bool		showDrawTime = false;
bool		pausedByUnmap = false;
static bool		unmapped = false;
static int		preUnmapFormat = -1;
static double		epochOffset;
static double		lastEpochOffset;
float		clockAdjust = 0.0f;
float		pauseCountdown = 0.0f;
float		destructCountdown = 0.0f;
static float		testVideoFormatTimer = 0.0f;
static int		testVideoPrevFormat = -1;
static std::vector<PlayingCallbackItem>	playingCallbacks;
bool			gameOver = false;
static std::vector<BillboardSceneNode*>	explosions;
static std::vector<BillboardSceneNode*>	prototypeExplosions;
int		savedVolume = -1;
static bool		grabMouseAlways = false;
FlashClock		pulse;
static bool             wasRabbit = false;

char		messageMessage[PlayerIdPLen + MessageLen];


void		setTarget();
static void		setHuntTarget();
static void*		handleMsgSetVars(void *msg);
void			handleFlagDropped(Player* tank);
static void		handlePlayerMessage(uint16_t, uint16_t, void*);
static void		handleFlagTransferred(Player* fromTank, Player* toTank, int flagIndex);
extern void		dumpResources(BzfDisplay*, SceneRenderer&);
static void		setRobotTarget(RobotPlayer* robot);
extern void		doAutoPilot(float &rotation, float &speed);
extern void		teachAutoPilot( FlagType *, int );

void		checkEnvironmentForRobots ( void );

const float	defaultPos[3] = { 0.0f, 0.0f, 0.0f };
const float	defaultDir[3] = { 1.0f, 0.0f, 0.0f };


enum BlowedUpReason {
  GotKilledMsg,
  GotShot,
  GotRunOver,
  GotCaptured,
  GenocideEffect,
  SelfDestruct
};
static const char*	blowedUpMessage[] = {
  NULL,
  "Got shot by ",
  "Got flattened by ",
  "Team flag was captured by ",
  "Teammate hit with Genocide by ",
  "Tank Self Destructed",
};
static bool		gotBlowedUp(BaseLocalPlayer* tank,
				    BlowedUpReason reason,
				    PlayerId killer,
				    const ShotPath *hit = NULL);

#ifdef ROBOT
static void		handleMyTankKilled(int reason);
static ServerLink*	robotServer[MAX_ROBOTS];
#endif

extern struct tm	userTime;
static double		userTimeEpochOffset;

StartupInfo::StartupInfo() : hasConfiguration(false),
			     autoConnect(false),
			     serverPort(ServerPort),
			     team(AutomaticTeam),
			     listServerURL(DefaultListServerURL),
			     listServerPort(ServerPort + 1)
{
  strcpy(serverName, "");
  strcpy(callsign, "");
  strcpy(email, "default");
  joystickName = "joystick";
  joystick = false;
}

// access silencePlayers from bzflag.cxx
std::vector<std::string>& getSilenceList()
{
  return silencePlayers;
}

// try to select the next recipient in the specified direction
// eventually avoiding robots
void selectNextRecipient (bool forward, bool robotIn)
{
  LocalPlayer *myTank = LocalPlayer::getMyTank();
  const Player *recipient = myTank->getRecipient();
  int rindex;
  if (!recipient) {
    rindex = - 1;
    forward = true;
  } else {
    const PlayerId id = recipient->getId();
    rindex = lookupPlayerIndex(id);
  }
  int i = rindex;
  while (true) {
    if (forward) {
      i++;
      if (i == curMaxPlayers)
	// if no old rec id we have just ended our search
	if (recipient == NULL)
	  break;
	else
	  // wrap around
	  i = 0;
    } else {
      if (i == 0)
	// wrap around
	i = curMaxPlayers;
      i--;
    }
    if (i == rindex)
      break;
    if (player[i] && (robotIn || player[i]->getPlayerType() == TankPlayer)) {
      myTank->setRecipient(player[i]);
      break;
    }
  }
}

//
// should we grab the mouse?
//

static void		setGrabMouse(bool grab)
{
  grabMouseAlways = grab;
}

bool		shouldGrabMouse()
{
  return grabMouseAlways && !unmapped &&
    (myTank == NULL || !myTank->isPaused() || myTank->isAutoPilot());
}

//
// some simple global functions
//

void        warnAboutMainFlags()
{
  // warning message for hidden flags 
  if (!BZDBCache::displayMainFlags){
    std::string showFlagsMsg = ColorStrings[YellowColor];
    showFlagsMsg += "Flags on field hidden, to show them ";
    std::vector<std::string> keys = KEYMGR.getKeysFromCommand("toggleFlags main", true);

    if (keys.size() != 0) {
      showFlagsMsg += "hit \"";
      showFlagsMsg += ColorStrings[WhiteColor];
      showFlagsMsg += tolower(keys[0][0]);
      showFlagsMsg += ColorStrings[YellowColor];
      showFlagsMsg += "\"";
    } else {
      showFlagsMsg += " bind a key to Toggle Flags on Field";
    }
    addMessage(NULL, showFlagsMsg);
  }
}

void        warnAboutRadarFlags()
{
  if (!BZDB.isTrue("displayRadarFlags")){
    std::string showFlagsMsg = ColorStrings[YellowColor];
    showFlagsMsg += "Flags on radar hidden, to show them ";
    std::vector<std::string> keys = KEYMGR.getKeysFromCommand("toggleFlags radar", true);

    if (keys.size() != 0) {
      showFlagsMsg += "hit \"";
      showFlagsMsg += ColorStrings[WhiteColor];
      showFlagsMsg += tolower(keys[0][0]);
      showFlagsMsg += ColorStrings[YellowColor];
      showFlagsMsg += "\"";
    } else {
      showFlagsMsg += " bind a key to Toggle Flags on Radar";
    }
    addMessage(NULL, showFlagsMsg);
  }
}

BzfDisplay*		getDisplay()
{
  return display;
}

MainWindow*		getMainWindow()
{
  return mainWindow;
}

SceneRenderer*		getSceneRenderer()
{
  return sceneRenderer;
}

void			setSceneDatabase()
{
  if (BZDB.isTrue("zbuffer")) {
    sceneRenderer->setSceneDatabase(zScene);
  }
  else {
    sceneRenderer->setSceneDatabase(bspScene);
  }
}

StartupInfo*		getStartupInfo()
{
  return &startupInfo;
}

bool			setVideoFormat(int index, bool test)
{
#if defined(_WIN32)
  // give windows extra time to test format (context reloading takes a while)
  static const float testDuration = 10.0f;
#else
  static const float testDuration = 5.0f;
#endif

  // ignore bad formats or when the format test timer is running
  if (testVideoFormatTimer != 0.0f || !display->isValidResolution(index))
    return false;

  // ignore if no change
  if (display->getResolution() == index) return true;

  // change it
  testVideoPrevFormat = display->getResolution();
  if (!display->setResolution(index)) return false;

  // handle resize
  mainWindow->setFullscreen();
  mainWindow->getWindow()->callResizeCallbacks();
  mainWindow->warpMouse();
  if (test) testVideoFormatTimer = testDuration;
  else if (shouldGrabMouse()) mainWindow->grabMouse();
  return true;
}

void			addPlayingCallback(PlayingCallback cb, void* data)
{
  PlayingCallbackItem item;
  item.cb = cb;
  item.data = data;
  playingCallbacks.push_back(item);
}

void			removePlayingCallback(PlayingCallback _cb, void* data)
{
  std::vector<PlayingCallbackItem>::iterator it = playingCallbacks.begin();
  while(it != playingCallbacks.end()) {
    if(it->cb == _cb && it->data == data) {
      playingCallbacks.erase(it);
      break;
    }
    it++;
  }
}

static void		callPlayingCallbacks()
{
	std::vector<PlayingCallbackItem>::iterator	itr = playingCallbacks.begin();
	while ( itr != playingCallbacks.end())
		(*(itr)->cb)((itr++)->data);

 /* const int count = playingCallbacks.size();
  for (int i = 0; i < count; i++)
	{
    const PlayingCallbackItem& cb = playingCallbacks[i];
    (*cb.cb)(cb.data);
  } */
	mainWindow->getWindow()->yieldCurrent();
}

void			joinGame(JoinGameCallback cb, void* data)
{
  joinGameCallback = cb;
  joinGameUserData = data;
}

//
// handle joining status when server provided on command line
//

void			joinGameHandler(bool okay, void*)
{
  if (!okay) printError("Connection failed.");
}

//
// handle signals that should kill me quickly
//

static void		dying(int sig)
{
  bzSignal(sig, SIG_DFL);
  display->setDefaultResolution();
  raise(sig);
}

//
// handle signals that should kill me nicely
//

static void		suicide(int sig)
{
  bzSignal(sig, SIG_PF(suicide));
  CommandsStandard::quit();
}

//
// handle signals that should disconnect me from the server
//

static void		hangup(int sig)
{
  bzSignal(sig, SIG_PF(hangup));
  serverDied = true;
  serverError = true;
}

static ServerLink*	lookupServer(const Player* player)
{
  PlayerId id = player->getId();
  if (myTank->getId() == id) return serverLink;
#ifdef ROBOT
  for (int i = 0; i < numRobots; i++)
    if (robots[i]->getId() == id)
      return robotServer[i];
#endif
  return NULL;
}

//
// user input handling
//

#if defined(DEBUG)
#define FREEZING
#endif
#if defined(FREEZING)
static bool		motionFreeze = false;
#endif
bool		roaming = false;
enum roamingView {
  roamViewFree = 0,
  roamViewTrack,
  roamViewFollow,
  roamViewFP,
  roamViewFlag,
  roamViewCount
} roamView = roamViewFP;
int		roamTrackTank = -1, roamTrackWinner = -1, roamTrackFlag = 0;
float		roamPos[3] = { 0.0f, 0.0f, 1.57f },  /* MuzzleHeight */
  roamDPos[3] = {0.0f, 0.0f, 0.0f};
float		roamTheta = 0.0f, roamDTheta = 0.0f;
float		roamPhi = 0.0f, roamDPhi = 0.0f;
float		roamZoom = 60.0f, roamDZoom = 0.0f;

void setRoamingLabel(bool force)
{
  if (!player)
    return;
  char *winner;
  if (roamTrackTank == -1) {
    int oldWinner = roamTrackWinner;
    if (roamTrackWinner == -1) {
      // in case we don't find one
      roamTrackWinner = 0;
    }
    // FIXME find the current living winner alive
    int bestScore = -65536; // nobody should be this bad, should they?
    for (int i = 0; i < curMaxPlayers; i++) {
      if (player[i] && player[i]->isAlive() && player[i]->getScore() >= bestScore) {
	roamTrackWinner = i;
	bestScore = player[i]->getScore();
      }
    }
    if (!force && roamTrackWinner == oldWinner)
      return;
    winner="Winner ";
  } else {
    winner="";
  }
  if (player[roamTrackWinner]) {
    switch (roamView) {
    case roamViewTrack:
      hud->setRoamingLabel(std::string("Tracking ") + winner +
			   player[roamTrackWinner]->getCallSign());
      break;

    case roamViewFollow:
      hud->setRoamingLabel(std::string("Following ") + winner +
			   player[roamTrackWinner]->getCallSign());
      break;

    case roamViewFP:
      hud->setRoamingLabel(std::string("Driving with ") + winner +
			   player[roamTrackWinner]->getCallSign());
      break;

    case roamViewFlag:
      hud->setRoamingLabel(std::string("Tracking ") +
			   world->getFlag(roamTrackFlag).type->flagName +
			   " Flag");
      break;

    default:
      hud->setRoamingLabel(std::string("Roaming"));
      break;
    }
  }
  else
    hud->setRoamingLabel("Roaming");
}

static enum {None, Left, Right, Up, Down} keyboardMovement;
static int shiftKeyStatus;

static bool		doKeyCommon(const BzfKeyEvent& key, bool pressed)
{
  keyboardMovement = None;
  shiftKeyStatus   = key.shift;
  const std::string cmd = KEYMGR.get(key, pressed);
  if (key.ascii == 27) {
    if (pressed) {
      mainMenu->createControls();
      HUDDialogStack::get()->push(mainMenu);
    }
    return true;
  } else if (hud->getHunt()) {
    if (key.button == BzfKeyEvent::Down ||
	KEYMGR.get(key, true) == "identify") {
      if (pressed) {
	hud->setHuntPosition(hud->getHuntPosition()+1);
      }
      return true;
    } else if (key.button == BzfKeyEvent::Up ||
    	       KEYMGR.get(key, true) == "drop") {
      if (pressed) {
	hud->setHuntPosition(hud->getHuntPosition()-1);
      }
      return true;
    } else if (KEYMGR.get(key, true) == "fire") {
      if (pressed) {
	hud->setHuntSelection(true);
	playLocalSound(SFX_HUNT_SELECT);
      }
      return true;
    }
  }
  std::string cmdDrive = cmd;
  if (cmdDrive.empty()) {
    // Check for driving keys
    BzfKeyEvent cleanKey = key;
    cleanKey.shift = 0;
    cmdDrive = KEYMGR.get(cleanKey, pressed);
  }
  if (cmdDrive == "turn left") {
    keyboardMovement = Left;
  } else if (cmdDrive == "turn right") {
    keyboardMovement = Right;
  } else if (cmdDrive == "drive forward") {
    keyboardMovement = Up;
  } else if (cmdDrive == "drive reverse") {
    keyboardMovement = Down;
  }
  if (myTank)
    switch (keyboardMovement) {
    case Left:
      myTank->setKey(BzfKeyEvent::Left, pressed);
      break;
    case Right:
      myTank->setKey(BzfKeyEvent::Right, pressed);
      break;
    case Up:
      myTank->setKey(BzfKeyEvent::Up, pressed);
      break;
    case Down:
      myTank->setKey(BzfKeyEvent::Down, pressed);
      break;
    case None:
      break;
    }
  if (!cmd.empty()) {
    if (cmd=="fire")
      fireButton = pressed;
    roamButton = pressed;
    if (keyboardMovement == None) {
      std::string result = CMDMGR.run(cmd);
      if (!result.empty())
	std::cerr << result << std::endl;
    }
    return true;
  } else {

    // built-in unchangeable keys.  only perform if not masked.
    switch (key.ascii) {
    case 'T':
    case 't':
      // toggle frames-per-second display
      if (pressed) {
	showFPS = !showFPS;
	if (!showFPS) hud->setFPS(-1.0);
      }
      return true;

    case 'Y':
    case 'y':
      // toggle milliseconds for drawing
      if (pressed) {
	showDrawTime = !showDrawTime;
	if (!showDrawTime) hud->setDrawTime(-1.0);
      }
      return true;

      /* XXX -- for testing forced recreation of OpenGL context
	 case 'o':
	 if (pressed) {
	 // destroy OpenGL context
	 getMainWindow()->getWindow()->freeContext();

	 // recreate OpenGL context
	 getMainWindow()->getWindow()->makeContext();

	 // force a redraw (mainly for control panel)
	 getMainWindow()->getWindow()->callExposeCallbacks();

	 // cause sun/moon to be repositioned immediately
	 lastEpochOffset = epochOffset - 5.0;

	 // reload display lists and textures and initialize other state
	 OpenGLGState::initContext();
	 }
	 break;
      */

    case ']':
    case '}':
      // plus 30 seconds
      if (pressed) clockAdjust += 30.0f;
      return true;

    case '[':
    case '{':
      // minus 30 seconds
      if (pressed) clockAdjust -= 30.0f;
      return true;

    default:
      break;
    }
  }
  return false;
}

static void		doKeyNotPlaying(const BzfKeyEvent& key, bool pressed)
{
  // handle key
  bool action = false;
  if (HUDDialogStack::get()->isActive()) {
    if (pressed)
      action = HUDui::keyPress(key);
    else
      action = HUDui::keyRelease(key);
  }
  if (!action)
    doKeyCommon(key, pressed);
}

static void		doKeyPlaying(const BzfKeyEvent& key, bool pressed)
{
  static ServerCommandKey serverCommandKeyHandler;

  if (HUDui::getFocus())
    if ((pressed && HUDui::keyPress(key)) ||
	(!pressed && HUDui::keyRelease(key))) {
      return;
    }

  bool haveBinding = doKeyCommon(key, pressed);

#if defined(FREEZING)
  if (key.ascii == '`' && pressed && !haveBinding) {
    // toggle motion freeze
    motionFreeze = !motionFreeze;
  }
  //  else
#endif

  if (key.ascii == 0 &&
      key.button >= BzfKeyEvent::F1 &&
      key.button <= BzfKeyEvent::F10 &&
      (key.shift & (BzfKeyEvent::ControlKey +
		    BzfKeyEvent::AltKey)) != 0 && !haveBinding) {
    // [Ctrl]-[Fx] is message to team
    // [Alt]-[Fx] is message to all
    if (pressed) {
      char name[32];
      int msgno = (key.button - BzfKeyEvent::F1) + 1;
      void* buf = messageMessage;
      if (key.shift == BzfKeyEvent::ControlKey) {
	sprintf(name, "quickTeamMessage%d", msgno);
	buf = nboPackUByte(buf, TeamToPlayerId(myTank->getTeam()));
      } else {
	sprintf(name, "quickMessage%d", msgno);
	buf = nboPackUByte(buf, AllPlayers);
      }
      if (BZDB.isSet(name)) {
	char messageBuffer[MessageLen];
	memset(messageBuffer, 0, MessageLen);
	strncpy(messageBuffer,
		BZDB.get(name).c_str(),
		MessageLen);
	nboPackString(buf, messageBuffer, MessageLen);
	serverLink->send(MsgMessage, sizeof(messageMessage), messageMessage);
      }
    }
  }
  // Might be a direction key. Save it for later.
  else if (myTank->isAlive()) {
    if ((myTank->getInputMethod() != LocalPlayer::Keyboard) && pressed) {
      if (keyboardMovement != None)
	  if (BZDB.isTrue("allowInputChange"))
	    myTank->setInputMethod(LocalPlayer::Keyboard);
    } 
  }
}

static void doKey(const BzfKeyEvent& key, bool pressed) {
  if (!myTank)
    doKeyNotPlaying(key, pressed);
  else
    doKeyPlaying(key, pressed);
}

static void		doMotion()
{
  float rotation = 0.0f, speed = 1.0f;
  const int noMotionSize = hud->getNoMotionSize();
  const int maxMotionSize = hud->getMaxMotionSize();

  // mouse is default steering method; query mouse pos always, not doing so
  // can lead to stuttering movement with X and software rendering (uncertain why)
  int mx = 0, my = 0;
  mainWindow->getMousePosition(mx, my);

  // determine if joystick motion should be used instead of mouse motion
  // when the player bumps the mouse, LocalPlayer::getInputMethod return Mouse;
  // make it return Joystick when the user bumps the joystick
  if (mainWindow->haveJoystick()) {
    if (myTank->getInputMethod() == LocalPlayer::Joystick) {
      // if we're using the joystick right now, replace mouse coords with joystick coords
      mainWindow->getJoyPosition(mx, my);
    } else {
      // if the joystick is not active, and we're not forced to some other input method,
      // see if it's moved and autoswitch
      if (BZDB.isTrue("allowInputChange")) {
	int jx = 0, jy = 0;
	mainWindow->getJoyPosition(jx, jy);
	// if we aren't using the joystick, but it's moving, start using it
	if ((jx < -noMotionSize * 2) || (jx > noMotionSize * 2)
	    || (jy < -noMotionSize * 2) || (jy > noMotionSize * 2)) {
	  myTank->setInputMethod(LocalPlayer::Joystick);
	} // joystick motion
      } // allowInputChange
    } // getInputMethod == Joystick
  } // mainWindow->Joystick

  /* see if controls are reversed */
  if (myTank->getFlag() == Flags::ReverseControls) {
    mx = -mx;
    my = -my;
  }

#if defined(FREEZING)
  if (motionFreeze) return;
#endif

  if (myTank->isAutoPilot()) {
    doAutoPilot(rotation, speed);
  } else if (myTank->getInputMethod() == LocalPlayer::Keyboard) {

    rotation = (float)myTank->getRotation();
    speed    = (float)myTank->getSpeed();
    /* see if controls are reversed */
    if (myTank->getFlag() == Flags::ReverseControls) {
      rotation = -rotation;
      speed    = -speed;
    }
    if (speed < 0.0f)
      speed /= 2.0;

    if (BZDB.isTrue("displayBinoculars"))
      rotation *= 0.2f;
    if (BZDB.isTrue("slowKeyboard")) {
      rotation /= 2.0f;
      speed /= 2.0f;
    }
  } else { // both mouse and joystick

    rotation = 0.0f;

    // calculate desired rotation
    if (mx < -noMotionSize) {
      rotation = float(-mx - noMotionSize) / float(maxMotionSize);
      if (rotation > 1.0f) rotation = 1.0f;
    }
    else if (mx > noMotionSize) {
      rotation = -float(mx - noMotionSize) / float(maxMotionSize);
      if (rotation < -1.0f) rotation = -1.0f;
    }

    // calculate desired speed
    speed = 0.0f;
    if (my < -noMotionSize) {
      speed = float(-my - noMotionSize) / float(maxMotionSize);
      if (speed > 1.0f) speed = 1.0f;
    }
    else if (my > noMotionSize) {
      speed = -float(my - noMotionSize) / float(maxMotionSize);
      if (speed < -0.5f) speed = -0.5f;
    }
  }

  myTank->setDesiredAngVel(rotation);
  myTank->setDesiredSpeed(speed);
}


static void		doEvent(BzfDisplay* display)
{
  BzfEvent event;
  if (!display->getEvent(event)) return;

  switch (event.type) {
  case BzfEvent::Quit:
    CommandsStandard::quit();
    break;

  case BzfEvent::Redraw:
    mainWindow->getWindow()->callExposeCallbacks();
    sceneRenderer->setExposed();
    break;

  case BzfEvent::Resize:
    if (mainWindow->getWidth() != event.resize.width || mainWindow->getHeight() != event.resize.height){
    mainWindow->getWindow()->setSize(event.resize.width, event.resize.height);
    mainWindow->getWindow()->callResizeCallbacks();
    }
    break;

  case BzfEvent::Map:
    // window has been mapped.  this normally occurs when the game
    // is uniconified.  if the player was paused because of an unmap
    // then resume.
    if (pausedByUnmap) {
      pausedByUnmap = false;
      pauseCountdown = 0.0f;
      if (myTank && myTank->isAlive() && myTank->isPaused()) {
	myTank->setPause(false);
	controlPanel->addMessage("Resumed");
      }
    }

    // restore the resolution we want if full screen
    if (mainWindow->getFullscreen()) {
      if (preUnmapFormat != -1) {
	display->setResolution(preUnmapFormat);
	mainWindow->warpMouse();
      }
    }

    // restore the sound
    if (savedVolume != -1) {
      setSoundVolume(savedVolume);
      savedVolume = -1;
    }

    unmapped = false;
    if (shouldGrabMouse())
      mainWindow->grabMouse();
    break;

  case BzfEvent::Unmap:
    // begin pause countdown when unmapped if:  we're not already
    // paused because of an unmap (shouldn't happen), we're not
    // already counting down to pausing, we're alive, and we're not
    // already paused.
    if (!pausedByUnmap && pauseCountdown == 0.0f &&
	myTank && myTank->isAlive() && !myTank->isPaused() && !myTank->isAutoPilot()) {
      // get ready to pause (no cheating through instantaneous pausing)
      pauseCountdown = 5.0f;

      // set this even though we haven't really paused yet
      pausedByUnmap = true;
    }

    // ungrab the mouse if we're running full screen
    if (mainWindow->getFullscreen()) {
      preUnmapFormat = -1;
      if (display->getNumResolutions() > 1) {
	preUnmapFormat = display->getResolution();
	display->setDefaultResolution();
      }
    }

    // turn off the sound
    if (savedVolume == -1) {
      savedVolume = getSoundVolume();
      setSoundVolume(0);
    }

    unmapped = true;
    mainWindow->ungrabMouse();
    break;

  case BzfEvent::KeyUp:
    doKey(event.keyDown, false);
    break;

  case BzfEvent::KeyDown:
    doKey(event.keyUp, true);
    break;

  case BzfEvent::MouseMove:
    if (myTank && myTank->isAlive() && (myTank->getInputMethod() != LocalPlayer::Mouse) && (BZDB.isTrue("allowInputChange")))
      myTank->setInputMethod(LocalPlayer::Mouse);
    break;

  default:
    /* unset */
    break;
  }
}

void		addMessage(const Player* player,
				   const std::string& msg, bool highlight,
				   const char* oldColor)
{
  std::string fullMessage;

  if (BZDB.isTrue("colorful")) {
    if (player) {
      if (highlight) {
	if (BZDB.get("killerhighlight") == "0")
	  fullMessage += ColorStrings[BlinkColor];
	else if (BZDB.get("killerhighlight") == "1")
	  fullMessage += ColorStrings[UnderlineColor];
      }
      int color = player->getTeam();
      if (color < 0 || color > 4) color = 5;

      fullMessage += ColorStrings[color];
      fullMessage += player->getCallSign();

      if (highlight)
	fullMessage += ColorStrings[ResetColor];
#ifdef BWSUPPORT
      fullMessage += " (";
      fullMessage += Team::getName(player->getTeam());
      fullMessage += ")";
#endif
      fullMessage += ColorStrings[DefaultColor];
      fullMessage += ": ";
    }
    fullMessage += msg;
  } else {
    if (oldColor != NULL)
      fullMessage = oldColor;

    if (player) {
      fullMessage += player->getCallSign();

#ifdef BWSUPPORT
      fullMessage += " (";
      fullMessage += Team::getName(player->getTeam());
      fullMessage += ")";
#endif
      fullMessage += ": ";
    }
    fullMessage += stripAnsiCodes(msg);
  }
  controlPanel->addMessage(fullMessage);
}

static void		updateNumPlayers()
{
  int i, numPlayers[NumTeams];
  for (i = 0; i < NumTeams; i++)
    numPlayers[i] = 0;
  for (i = 0; i < curMaxPlayers; i++)
    if (player[i])
      numPlayers[player[i]->getTeam()]++;
  if (myTank)
    numPlayers[myTank->getTeam()]++;
}

static void		updateHighScores()
{
  /* check scores to see if my team and/or have the high score.  change
   * `>= bestScore' to `> bestScore' if you want to share the number
   * one spot. */
  bool anyPlayers = false;
  int i;
  for (i = 0; i < curMaxPlayers; i++)
    if (player[i]) {
      anyPlayers = true;
      break;
    }
#ifdef ROBOT
  if (!anyPlayers) {
    for (i = 0; i < numRobots; i++)
      if (robots[i]) {
	anyPlayers = true;
	break;
      }
  }
#endif
  if (!anyPlayers) {
    hud->setPlayerHasHighScore(false);
    hud->setTeamHasHighScore(false);
    return;
  }

  bool haveBest = true;
  int bestScore = myTank ? myTank->getScore() : 0;
  for (i = 0; i < curMaxPlayers; i++)
    if (player[i] && player[i]->getScore() >= bestScore) {
      haveBest = false;
      break;
    }
#ifdef ROBOT
  if (haveBest) {
    for (i = 0; i < numRobots; i++)
      if (robots[i] && robots[i]->getScore() >= bestScore) {
	haveBest = false;
	break;
      }
  }
#endif
  hud->setPlayerHasHighScore(haveBest);

  if (myTank && Team::isColorTeam(myTank->getTeam())) {
    const Team& myTeam = World::getWorld()->getTeam(int(myTank->getTeam()));
    bestScore = myTeam.won - myTeam.lost;
    haveBest = true;
    for (i = 0; i < NumTeams; i++) {
      if (i == int(myTank->getTeam())) continue;
      const Team& team = World::getWorld()->getTeam(i);
      if (team.size > 0 && team.won - team.lost >= bestScore) {
	haveBest = false;
	break;
      }
    }
    hud->setTeamHasHighScore(haveBest);
  }
  else {
    hud->setTeamHasHighScore(false);
  }
}

static void		updateFlag(FlagType* flag)
{
  if (flag == Flags::Null) {
    hud->setColor(1.0f, 0.625f, 0.125f);
    hud->setAlert(2, NULL, 0.0f);
  }
  else {
    const float* color = flag->getColor();
    hud->setColor(color[0], color[1], color[2]);
    hud->setAlert(2, flag->flagName, 3.0f, flag->endurance == FlagSticky);
  }

  if (BZDB.isTrue("displayFlagHelp"))
    hud->setFlagHelp(flag, FlagHelpDuration);

  if (!radar && !myTank || !World::getWorld()) return;

  radar->setJammed(flag == Flags::Jamming);
  hud->setAltitudeTape(flag == Flags::Jumping || World::getWorld()->allowJumping());
}


void			notifyBzfKeyMapChanged()
{
  std::string restartLabel = "Right Mouse";
  std::vector<std::string> keys = KEYMGR.getKeysFromCommand("restart", false);

  if (keys.size() == 0) {
    // found nothing on down binding, so try up
    keys = KEYMGR.getKeysFromCommand("identify", true);
    if (keys.size() == 0) {
      std::cerr << "There does not appear to be any key bound to enter the game" << std::endl;
    }
  }

  if (keys.size() >= 1) {
    // display single letter keys as a quoted lowercase letter
    if (keys[0].size() == 1) {
      restartLabel = '\"';
      restartLabel += tolower(keys[0][0]);
      restartLabel += '\"';
    } else {
      restartLabel = keys[0];
    }
  }

  // only show the first 2 keys found to keep things simple
  if (keys.size() > 1) {
    restartLabel.append(" or ");
    // display single letter keys as quoted lowercase letter
    if (keys[1].size() == 1) {
      restartLabel += '\"';
      restartLabel += tolower(keys[1][0]);
      restartLabel += '\"';
    } else {
      restartLabel.append(keys[1]);
    }
  }

  hud->setRestartKeyLabel(restartLabel);
}


//
// server message handling
//

static Player*		addPlayer(PlayerId id, void* msg, int showMessage)
{
  uint16_t team, type, wins, losses, tks;
  char callsign[CallSignLen];
  char email[EmailLen];
  msg = nboUnpackUShort(msg, type);
  msg = nboUnpackUShort(msg, team);
  msg = nboUnpackUShort(msg, wins);
  msg = nboUnpackUShort(msg, losses);
  msg = nboUnpackUShort(msg, tks);
  msg = nboUnpackString(msg, callsign, CallSignLen);
  msg = nboUnpackString(msg, email, EmailLen);

  // Strip any ANSI color codes
  strncpy(callsign, stripAnsiCodes(std::string(callsign)).c_str(), 32);

  // id is slot, check if it's empty
  const int i = id;

  // sanity check
  if (i < 0) {
    printError(string_util::format("Invalid player identification (%d)", i));
    std::cerr << "WARNING: invalid player identification when adding player with id " << i << std::endl;
    return NULL;
  }

  if (player[i]) {
    // we're not in synch with server -> help!  not a good sign, but not fatal.
    printError("Server error when adding player, player already added");
    std::cerr << "WARNING: player already exists at location with id " << i << std::endl;
    return NULL;
  }

  if (i >= curMaxPlayers) {
    curMaxPlayers = i+1;
    World::getWorld()->setCurMaxPlayers(curMaxPlayers);
  }
  // add player
  if (PlayerType(type) == TankPlayer || PlayerType(type) == ComputerPlayer) {
    player[i] = new RemotePlayer(id, TeamColor(team), callsign, email,
				 PlayerType(type));
    player[i]->changeScore(short(wins), short(losses), short(tks));
  }

#ifdef ROBOT
  if (PlayerType(type) == ComputerPlayer)
    for (int i = 0; i < numRobots; i++)
      if (robots[i] && !strncmp(robots[i]->getCallSign(), callsign, CallSignLen)) {
	robots[i]->setTeam(TeamColor(team));
	break;
      }
#endif

  if (showMessage) {
    std::string message("joining as a");
    switch (PlayerType(type)) {
    case TankPlayer:
      message += " tank";
      break;
    case ComputerPlayer:
      message += " robot tank";
      break;
    default:
      message += "n unknown type";
      break;
    }
    if (!player[i]) {
      std::string name(callsign);
      name += ": ";
      name += message;
      message = name;
    }
    addMessage(player[i], message);
  }

  return player[i];
}

static bool removePlayer (PlayerId id)
{
  int playerIndex = lookupPlayerIndex(id);

  if (playerIndex < 0) {
    return false;
  }
  
  addMessage(player[playerIndex], "signing off");
  if (myTank->getRecipient() == player[playerIndex])
    myTank->setRecipient(0);
  if (myTank->getNemesis() == player[playerIndex])
    myTank->setNemesis(0);
  delete player[playerIndex];
  player[playerIndex] = NULL;

  while ((playerIndex >= 0)
         &&     (playerIndex+1 == curMaxPlayers)
         &&     (player[playerIndex] == NULL))
    {
      playerIndex--;
      curMaxPlayers--;
    }
  World::getWorld()->setCurMaxPlayers(curMaxPlayers);

  updateNumPlayers();

  return true;
}

static void		handleServerMessage(bool human, uint16_t code,
					    uint16_t, void* msg)
{
  std::vector<std::string> args;
  bool checkScores = false;
  static WordFilter *wordfilter = (WordFilter *)BZDB.getPointer("filter");

  switch (code) {

  case MsgUDPLinkEstablished:
    // server got our initial UDP packet
    serverLink->enableOutboundUDP();
    break;

  case MsgUDPLinkRequest:
    // we got server's initial UDP packet
    serverLink->confirmIncomingUDP();
    break;

  case MsgSuperKill:
    printError("Server forced a disconnect");
    serverError = true;
    break;

  case MsgTimeUpdate: {
    uint16_t timeLeft;
    msg = nboUnpackUShort(msg, timeLeft);
    hud->setTimeLeft(timeLeft);
    if (timeLeft == 0) {
      gameOver = true;
      myTank->explodeTank();
      controlPanel->addMessage("Time Expired");
      hud->setAlert(0, "Time Expired", 10.0f, true);
#ifdef ROBOT
      for (int i = 0; i < numRobots; i++)
	robots[i]->explodeTank();
#endif
    }
    break;
  }

  case MsgScoreOver: {
    // unpack packet
    PlayerId id;
    uint16_t team;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackUShort(msg, team);
    Player* player = lookupPlayer(id);

    // make a message
    std::string msg2;
    if (team == (uint16_t)NoTeam) {
      // a player won
      if (player) {
	msg2 = player->getCallSign();
	msg2 += " (";
	msg2 += Team::getName(player->getTeam());
	msg2 += ")";
      }
      else {
	msg2 = "[unknown player]";
      }
    }
    else {
      // a team won
      msg2 = Team::getName(TeamColor(team));
    }
    msg2 += " won the game";

    gameOver = true;
    hud->setTimeLeft(-1);
    myTank->explodeTank();
    controlPanel->addMessage(msg2);
    hud->setAlert(0, msg2.c_str(), 10.0f, true);
#ifdef ROBOT
    for (int i = 0; i < numRobots; i++)
      robots[i]->explodeTank();
#endif
    break;
  }

  case MsgAddPlayer: {
    PlayerId id;
    msg = nboUnpackUByte(msg, id);
#if defined(FIXME) && defined(ROBOT)
    for (int i = 0; i < numRobots; i++) {
      void *tmpbuf = msg;
      uint16_t team, type, wins, losses, tks;
      char callsign[CallSignLen];
      char email[EmailLen];
      tmpbuf = nboUnpackUShort(tmpbuf, type);
      tmpbuf = nboUnpackUShort(tmpbuf, team);
      tmpbuf = nboUnpackUShort(tmpbuf, wins);
      tmpbuf = nboUnpackUShort(tmpbuf, losses);
      tmpbuf = nboUnpackUShort(tmpbuf, tks);
      tmpbuf = nboUnpackString(tmpbuf, callsign, CallSignLen);
      tmpbuf = nboUnpackString(tmpbuf, email, EmailLen);
      std::cerr << "id " << id.port << ':' <<
      			    id.number << ':' <<
			    callsign << ' ' <<
			    robots[i]->getId().port << ':' <<
			    robots[i]->getId().number << ':' <<
			    robots[i]->getCallsign() << std::endl;
      if (strncmp(robots[i]->getCallSign(), callsign, CallSignLen)) {
	// check for real robot id
	char buffer[100];
	snprintf(buffer, 100, "id test %p %p %p %8.8x %8.8x\n",
		robots[i], tmpbuf, msg, *(int *)tmpbuf, *((int *)tmpbuf + 1));
	std::cerr << buffer;
	if (tmpbuf < (char *)msg + len) {
	  PlayerId id;
	  tmpbuf = nboUnpackUByte(tmpbuf, id);
	  robots[i]->id.serverHost = id.serverHost;
	  robots[i]->id.port = id.port;
	  robots[i]->id.number = id.number;
	  robots[i]->server->send(MsgIdAck, 0, NULL);
	}
      }
    }
#endif
    if (id == myTank->getId()) {
      std::cerr << "WARNING: found my own id in MsgAddPlayer packet\n";
      break;		// that's odd -- it's me!
    }
    addPlayer(id, msg, true);
    updateNumPlayers();
    checkScores = true;
    break;
  }

  case MsgRemovePlayer: {
    PlayerId id;
    msg = nboUnpackUByte(msg, id);
    if (removePlayer (id)) {
      checkScores = true;
    }
    break;
  }

  case MsgFlagUpdate: {
    uint16_t count;
    uint16_t flagIndex;
    msg = nboUnpackUShort(msg, count);
    for (int i = 0; i < count; i++) {
      msg = nboUnpackUShort(msg, flagIndex);
      msg = world->getFlag(int(flagIndex)).unpack(msg);
      world->initFlag(int(flagIndex));
    }
    break;
  }

  case MsgTeamUpdate: {
    uint8_t  numTeams;
    uint16_t team;

    msg = nboUnpackUByte(msg,numTeams);
    for (int i = 0; i < numTeams; i++) {
      msg = nboUnpackUShort(msg, team);
      msg = teams[int(team)].unpack(msg);
    }
    updateNumPlayers();
    checkScores = true;
    break;
  }

  case MsgAlive: {
    PlayerId id;
    float pos[3], forward;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackVector(msg, pos);
    msg = nboUnpackFloat(msg, forward);
    int playerIndex = lookupPlayerIndex(id);

    if ((playerIndex >= 0) || (playerIndex == -2)) {
      static const float zero[3] = { 0.0f, 0.0f, 0.0f };
      Player* tank = getPlayerByIndex(playerIndex);
      if (tank == myTank) {
	wasRabbit = tank->getTeam() == RabbitTeam;
	myTank->restart(pos, forward);
	firstLife = false;
	if (!myTank->isAutoPilot())
	  mainWindow->warpMouse();
	hud->setAltitudeTape(World::getWorld()->allowJumping());
      } else if (tank->getPlayerType() == ComputerPlayer) {
	for (int r = 0; r < numRobots; r++) {
	  if (robots[r]->getId() == playerIndex) {
	    robots[r]->restart(pos,forward);
	    setRobotTarget(robots[r]);
	    break;
	  }
	}
      }

      tank->setStatus(PlayerState::Alive);
      tank->move(pos, forward);
      tank->setVelocity(zero);
      tank->setAngularVelocity(0.0f);
      tank->setDeadReckoning();
      if (tank==myTank) {
	playLocalSound(SFX_POP);
        myTank->setSpawning(false);
      }
      else
	playWorldSound(SFX_POP, pos[0], pos[1], pos[2], true);
    }

    break;
  }

  case MsgKilled: {
    PlayerId victim, killer;
    int16_t shotId, reason;
    msg = nboUnpackUByte(msg, victim);
    msg = nboUnpackUByte(msg, killer);
    msg = nboUnpackShort(msg, reason);
    msg = nboUnpackShort(msg, shotId);
    BaseLocalPlayer* victimLocal = getLocalPlayer(victim);
    BaseLocalPlayer* killerLocal = getLocalPlayer(killer);
    Player* victimPlayer = lookupPlayer(victim);
    Player* killerPlayer = lookupPlayer(killer);
#ifdef ROBOT
    if (victimPlayer == myTank) {
      // uh oh, i'm dead
      if (myTank->isAlive()) {
		serverLink->sendDropFlag(myTank->getPosition());
		handleMyTankKilled(reason);
      }

    }
#endif
    if (victimLocal) {
      // uh oh, local player is dead
      if (victimLocal->isAlive()){
	gotBlowedUp(victimLocal, GotKilledMsg, killer);
      }
    }
    else if (victimPlayer) {
      victimPlayer->setExplode(TimeKeeper::getTick());
      const float* pos = victimPlayer->getPosition();
      if (reason == GotRunOver)
	playWorldSound(SFX_RUNOVER, pos[0], pos[1], pos[2], killerLocal == myTank);
      else
	playWorldSound(SFX_EXPLOSION, pos[0], pos[1], pos[2], killerLocal == myTank);
      float explodePos[3];
      explodePos[0] = pos[0];
      explodePos[1] = pos[1];
      explodePos[2] = pos[2] + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
      addTankExplosion(explodePos);
    }
    if (killerLocal) {
      // local player did it
      if (shotId >= 0) {
	// terminate the shot
	killerLocal->endShot(shotId, true);
      }
      if (victimPlayer && killerLocal != victimPlayer) {
	if (killerPlayer == myTank && wasRabbit) {
	  // enemy
	  killerLocal->changeScore(1, 0, 0);
	} else {
	  if (victimPlayer->getTeam() == killerLocal->getTeam() &&
	      ((killerLocal->getTeam() != RogueTeam)
	       || (World::getWorld()->allowRabbit()))) {
	  } else {
	    // enemy
	    killerLocal->changeScore(1, 0, 0);
		if (myTank->isAutoPilot()) {
		  if (killerPlayer) {
			const ShotPath* shot = killerPlayer->getShot(int(shotId));
            if (shot != NULL)
		      teachAutoPilot( shot->getFlag(), 1 );
		  }
		}
	  }
	}
      }
    }

    // handle my personal score against other players
    if ((killerPlayer == myTank || victimPlayer == myTank) &&
	!(killerPlayer == myTank && victimPlayer == myTank)) {
      if (killerLocal == myTank) {
	if (victimPlayer)
	  victimPlayer->changeLocalScore(1, 0, 0);
	myTank->setNemesis(victimPlayer);
      }
      else {
	if (killerPlayer)
	  killerPlayer->changeLocalScore(0, 1, killerPlayer->getTeam() == victimPlayer->getTeam() ? 1 : 0);
	myTank->setNemesis(killerPlayer);
      }
    }

    // add message
    if (human && victimPlayer) {
      if (killerPlayer == victimPlayer) {
	std::string message(ColorStrings[WhiteColor]);
	message += "blew myself up";
	addMessage(victimPlayer, message);
      }
      else if (!killerPlayer) {
	addMessage(victimPlayer, "destroyed by (UNKNOWN)");
      }
      else if ((shotId == -1) || (killerPlayer->getShot(int(shotId)) == NULL)) {
	std::string message(ColorStrings[WhiteColor]);
	message += "destroyed by ";
	if (killerPlayer->getTeam() == victimPlayer->getTeam() &&
	    killerPlayer->getTeam() != RogueTeam)
	  message += "teammate ";
	message += ColorStrings[killerPlayer->getTeam()];
	message += killerPlayer->getCallSign();
	addMessage(victimPlayer, message);
      }
      else {
	const ShotPath* shot = killerPlayer->getShot(int(shotId));
	std::string message (ColorStrings[WhiteColor]);
	std::string playerStr;
	if (killerPlayer->getTeam() == victimPlayer->getTeam() &&
	    killerPlayer->getTeam() != RogueTeam)
	  playerStr += "teammate ";

	if (victimPlayer == myTank) {
	  if (BZDB.get("killerhighlight") == "0")
	    playerStr += ColorStrings[BlinkColor];
	  else if (BZDB.get("killerhighlight") == "1")
	    playerStr += ColorStrings[UnderlineColor];
	}
	playerStr += ColorStrings[killerPlayer->getTeam()];
	playerStr += killerPlayer->getCallSign();

	if (victimPlayer == myTank)
	  playerStr += ColorStrings[ResetColor];
	playerStr += ColorStrings[WhiteColor];

	// Give more informative kill messages
	FlagType* shotFlag = shot->getFlag();
	if (shotFlag == Flags::Laser) {
	  message += "was fried by ";
	  message += playerStr;
	  message += "'s laser";
	}
	else if (shotFlag == Flags::GuidedMissile) {
	  message += "was destroyed by ";
	  message += playerStr;
	  message += "'s guided missile";
	}
	else if (shotFlag == Flags::ShockWave) {
	  message += "felt the effects of ";
	  message += playerStr;
	  message += "'s shockwave";
	}
	else if (shotFlag == Flags::InvisibleBullet) {
	  message += "didn't see ";
	  message += playerStr;
	  message += "'s bullet";
	}
	else if (shotFlag == Flags::MachineGun) {
	  message += "was turned into swiss cheese by ";
	  message += playerStr;
	  message += "'s machine gun";
	}
	else if (shotFlag == Flags::SuperBullet) {
	  message += "got skewered by ";
	  message += playerStr;
	  message += "'s super bullet";
	}
	else {
	  message += "killed by ";
	  message += playerStr;
	}
	addMessage(victimPlayer, message, killerPlayer==myTank);
      }
    }

    // blow up if killer has genocide flag and i'm on same team as victim
    // (and we're not rogues, unless in rabbit mode)
    if (human && killerPlayer && victimPlayer && victimPlayer != myTank &&
	victimPlayer->getTeam() == myTank->getTeam() &&
	((myTank->getTeam() != RogueTeam) || World::getWorld()->allowRabbit()) && shotId >= 0) {
      // now see if shot was fired with a GenocideFlag
      const ShotPath* shot = killerPlayer->getShot(int(shotId));
      if (shot && shot->getFlag() == Flags::Genocide) {
	gotBlowedUp(myTank, GenocideEffect, killerPlayer->getId());
      }
    }

#ifdef ROBOT
    // blow up robots on victim's team if shot was genocide
    if (killerPlayer && victimPlayer && shotId >= 0) {
      const ShotPath* shot = killerPlayer->getShot(int(shotId));
      if (shot && shot->getFlag() == Flags::Genocide)
	for (int i = 0; i < numRobots; i++)
	  if (victimPlayer != robots[i] &&
	      victimPlayer->getTeam() == robots[i]->getTeam() &&
	      robots[i]->getTeam() != RogueTeam)
	    gotBlowedUp(robots[i], GenocideEffect, killerPlayer->getId());
    }
#endif

    checkScores = true;
    break;
  }

  case MsgGrabFlag: {
    // ROBOT -- FIXME -- robots don't grab flag at the moment
    PlayerId id;
    uint16_t flagIndex;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackUShort(msg, flagIndex);
    msg = world->getFlag(int(flagIndex)).unpack(msg);
    Player* tank = lookupPlayer(id);
    if (!tank) break;

    // player now has flag
    tank->setFlag(world->getFlag(flagIndex).type);
    if (tank == myTank) {
	// grabbed flag
	playLocalSound(myTank->getFlag()->endurance != FlagSticky ?
		       SFX_GRAB_FLAG : SFX_GRAB_BAD);
	updateFlag(myTank->getFlag());
    }
    else if (myTank->getTeam() != RabbitTeam && tank &&
	     tank->getTeam() != myTank->getTeam() &&
	     world->getFlag(flagIndex).type->flagTeam == myTank->getTeam()) {
      hud->setAlert(1, "Flag Alert!!!", 3.0f, true);
      playLocalSound(SFX_ALERT);
    }
    else {
      FlagType* fd = world->getFlag(flagIndex).type;
      if ( fd->flagTeam != NoTeam
	   && fd->flagTeam != tank->getTeam()
	   && ((tank && (tank->getTeam() == myTank->getTeam())))
	   && (Team::isColorTeam(myTank->getTeam()))) {
	hud->setAlert(1, "Team Grab!!!", 3.0f, false);
	const float* pos = tank->getPosition();
	playWorldSound(SFX_TEAMGRAB, pos[0], pos[1], pos[2], false);
      }
    }
    std::string message("grabbed ");
    message += tank->getFlag()->flagName;
    message += " flag";
    addMessage(tank, message);
    break;
  }

  case MsgDropFlag: {
    PlayerId id;
    uint16_t flagIndex;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackUShort(msg, flagIndex);
    msg = world->getFlag(int(flagIndex)).unpack(msg);
    Player* tank = lookupPlayer(id);
    if (!tank) break;
    handleFlagDropped(tank);
    break;
  }

  case MsgCaptureFlag: {
    PlayerId id;
    uint16_t flagIndex, team;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackUShort(msg, flagIndex);
    msg = nboUnpackUShort(msg, team);
    Player* capturer = lookupPlayer(id);
    int capturedTeam = world->getFlag(int(flagIndex)).type->flagTeam;

    // player no longer has flag
    if (capturer) {
      capturer->setFlag(Flags::Null);
      if (capturer == myTank) {
	updateFlag(Flags::Null);
      }

      // add message
      if (int(capturer->getTeam()) == capturedTeam) {
	std::string message("took my flag into ");
	message += Team::getName(TeamColor(team));
	message += " territory";
	addMessage(capturer, message);
 	if (capturer == myTank) {
   hud->setAlert(1, "Don't capture your own flag!!!", 3.0f, true);
   if (myTank->isAutoPilot()) {
   	char meaculpa[MessageLen];
   	memset(meaculpa, 0, MessageLen);
   	strncpy(meaculpa, "sorry, i'm just a silly machine", MessageLen);
		char *buf = messageMessage;
		buf = (char*)nboPackUShort(buf, myTank->getTeam());
    nboPackString(buf, meaculpa, MessageLen-1);
    serverLink->send(MsgMessage, sizeof(messageMessage), messageMessage);
   }
 }
      }
      else {
	std::string message("captured ");
	message += Team::getName(TeamColor(capturedTeam));
	message += "'s flag";
	addMessage(capturer, message);
      }
    }

    // play sound -- if my team is same as captured flag then my team lost,
    // but if I'm on the same team as the capturer then my team won.
    if (capturedTeam == int(myTank->getTeam()))
      playLocalSound(SFX_LOSE);
    else if (capturer->getTeam() == myTank->getTeam())
      playLocalSound(SFX_CAPTURE);

    // blow up if my team flag captured
    if (capturedTeam == int(myTank->getTeam())) {
      gotBlowedUp(myTank, GotCaptured, id);
    }

    //kill all my robots if they are on the captured team
    for (int r = 0; r < numRobots; r++) {
      if (robots[r]->getTeam() == capturedTeam) {
	gotBlowedUp(robots[r], GotCaptured, robots[r]->getId());
      }
    }


    // everybody who's alive on capture team will be blowing up
    // but we're not going to get an individual notification for
    // each of them, so add an explosion for each now.  don't
    // include me, though;  I already blew myself up.
    for (int i = 0; i < curMaxPlayers; i++) {
      if (player[i] &&
	  player[i]->isAlive() &&
	  player[i]->getTeam() == capturedTeam) {
	const float* pos = player[i]->getPosition();
	playWorldSound(SFX_EXPLOSION, pos[0], pos[1], pos[2], false);
	float explodePos[3];
	explodePos[0] = pos[0];
	explodePos[1] = pos[1];
	explodePos[2] = pos[2] + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
	addTankExplosion(explodePos);
      }
    }

    checkScores = true;
    break;
  }

  case MsgNewRabbit: {
    PlayerId id;
    msg = nboUnpackUByte(msg, id);
    Player *rabbit = lookupPlayer(id);

    for (int i = 0; i < curMaxPlayers; i++) {
      if (player[i])
	player[i]->setHunted(false);
      if (i != id && player[i] && player[i]->getTeam() != RogueTeam
	  && player[i]->getTeam() != ObserverTeam) {
	player[i]->changeTeam(RogueTeam);
      }
    }

    if (rabbit != NULL) {
      rabbit->changeTeam(RabbitTeam);
      if (rabbit == myTank) {
	wasRabbit = true;
	if (myTank->isPaused())
	  serverLink->sendNewRabbit();
	else {
	  hud->setAlert(0, "You are now the rabbit.", 10.0f, false);
	  playLocalSound(SFX_HUNT_SELECT);
	}
	hud->setHunting(false);
      } else if (myTank->getTeam() != ObserverTeam) {
	myTank->changeTeam(RogueTeam);
	if (myTank->isPaused() || myTank->isAlive())
	  wasRabbit = false;
	rabbit->setHunted(true);
	hud->setHunting(true);
      }

      addMessage(rabbit, "is now the rabbit", true);
    }

#ifdef ROBOT
    for (int r = 0; r < numRobots; r++)
      if (robots[r]->getId() == id)
	robots[r]->changeTeam(RabbitTeam);
      else
	robots[r]->changeTeam(RogueTeam);
#endif
    break;
  }

  case MsgShotBegin: {
    FiringInfo firingInfo;
    msg = firingInfo.unpack(msg);

    const int shooterid = firingInfo.shot.player;
    if (shooterid != ServerPlayer) {
      if (player[shooterid] && player[shooterid]->getId() == shooterid)
	player[shooterid]->addShot(firingInfo);
      else
	break;
    }
    else
      World::getWorld()->getWorldWeapons()->addShot(firingInfo);

    if (human) {
      const float* pos = firingInfo.shot.pos;
      if (firingInfo.flagType == Flags::ShockWave)
	playWorldSound(SFX_SHOCK, pos[0], pos[1], pos[2]);
      else if (firingInfo.flagType == Flags::Laser)
	playWorldSound(SFX_LASER, pos[0], pos[1], pos[2]);
      else if (firingInfo.flagType == Flags::GuidedMissile)
	playWorldSound(SFX_MISSILE, pos[0], pos[1], pos[2]);
      else if (firingInfo.flagType == Flags::Thief)
	playWorldSound(SFX_THIEF, pos[0], pos[1], pos[2]);
      else
	playWorldSound(SFX_FIRE, pos[0], pos[1], pos[2]);
    }
    break;
  }

  case MsgShotEnd: {
    PlayerId id;
    int16_t shotId;
    uint16_t reason;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackShort(msg, shotId);
    msg = nboUnpackUShort(msg, reason);
    BaseLocalPlayer* localPlayer = getLocalPlayer(id);

    if (localPlayer)
      localPlayer->endShot(int(shotId), false, reason == 0);
    else {
      Player *pl = lookupPlayer(id);
      if (pl)
	pl->endShot(int(shotId), false, reason == 0);
    }
    break;
  }

  case MsgScore: {
    uint8_t numScores;
    PlayerId id;
    uint16_t wins, losses, tks;
    msg = nboUnpackUByte(msg, numScores);

    for (uint8_t s = 0; s < numScores; s++) {
      msg = nboUnpackUByte(msg, id);
      msg = nboUnpackUShort(msg, wins);
      msg = nboUnpackUShort(msg, losses);
      msg = nboUnpackUShort(msg, tks);

      int i = lookupPlayerIndex(id);
      if (i >= 0)
	player[i]->changeScore(wins - player[i]->getWins(),
			       losses - player[i]->getLosses(),
			       tks - player[i]->getTeamKills());
    }
    break;
  }

  case MsgSetVar: {
    msg = handleMsgSetVars(msg);
    break;
  }

  case MsgTeleport: {
    PlayerId id;
    uint16_t from, to;
    msg = nboUnpackUByte(msg, id);
    msg = nboUnpackUShort(msg, from);
    msg = nboUnpackUShort(msg, to);
    Player* tank = lookupPlayer(id);
    if (tank && tank != myTank) {
      int face;
      const Teleporter* teleporter = world->getTeleporter(int(to), face);
      const float* pos = teleporter->getPosition();
      tank->setTeleport(TimeKeeper::getTick(), short(from), short(to));
      playWorldSound(SFX_TELEPORT, pos[0], pos[1], pos[2]);
    }
    break;
  }

  case MsgTransferFlag:
    {
      PlayerId fromId, toId;
      unsigned short flagIndex;
      msg = nboUnpackUByte(msg, fromId);
      msg = nboUnpackUByte(msg, toId);
      msg = nboUnpackUShort(msg, flagIndex);
      msg = world->getFlag(int(flagIndex)).unpack(msg);
      Player* fromTank = lookupPlayer(fromId);
      Player* toTank = lookupPlayer(toId);
      handleFlagTransferred( fromTank, toTank, flagIndex);
      break;
    }


  case MsgMessage: {
    PlayerId src;
    PlayerId dst;
    msg = nboUnpackUByte(msg, src);
    msg = nboUnpackUByte(msg, dst);
    Player* srcPlayer = lookupPlayer(src);
    Player* dstPlayer = lookupPlayer(dst);
    TeamColor dstTeam = PlayerIdToTeam(dst);
    bool toAll = (dst == AllPlayers);
    bool fromServer = (src == ServerPlayer);
		bool toAdmin = (dst == AdminPlayers);
		std::string dstName;

    const std::string srcName = fromServer ? "SERVER" : (srcPlayer ? srcPlayer->getCallSign() : "(UNKNOWN)");
    if (dstPlayer){
			dstName = dstPlayer->getCallSign();
		} else if (toAdmin){
			dstName = "Admin";
		} else {
      dstName = "(UNKNOWN)";
		}
    std::string fullMsg;

    bool ignore = false;
    unsigned int i;
    for (i = 0; i < silencePlayers.size(); i++) {
      const std::string &silenceCallSign = silencePlayers[i];
      if (srcName == silenceCallSign || "*" == silenceCallSign) {
				ignore = true;
				break;
      }
    }

    if (ignore) {
#ifdef DEBUG
      // to verify working
      std::string msg2 = "Ignored Msg";
      if (silencePlayers[i] != "*") {
	msg2 = msg2 + " from " + silencePlayers[i];
      } else {
	//if * just echo a generic Ignored
      }
      addMessage(NULL,msg2);
#endif
      break;
    }

    if (fromServer) {
      /* if the server tells us that we need to identify, and we have
       * already stored a password key for this server -- send it on
       * over back to auto-identify.
       */
      static const char passwdRequest[] = "Identify with /identify";
      if (!strncmp((char*)msg, passwdRequest, strlen(passwdRequest))) {
        const std::string passwdKeys[] = {
          string_util::format("%s@%s:%d", startupInfo.callsign, startupInfo.serverName, startupInfo.serverPort),
          string_util::format("%s:%d", startupInfo.serverName, startupInfo.serverPort),
          string_util::format("%s@%s", startupInfo.callsign, startupInfo.serverName),
          string_util::format("%s", startupInfo.serverName),
          string_util::format("%s", startupInfo.callsign),
          "@" // catch-all for all callsign/server/ports
        };

        for (size_t i = 0; i < countof(passwdKeys); i++) {
          if (BZDB.isSet(passwdKeys[i])) {
            std::string passwdResponse = "/identify " + BZDB.get(passwdKeys[i]);
            addMessage(0, ("Autoidentifying with password stored for " + passwdKeys[i]).c_str(), false);
            void *buf = messageMessage;
            buf = nboPackUByte(buf, ServerPlayer);
            nboPackString(buf, (void*) passwdResponse.c_str(), MessageLen);
            serverLink->send(MsgMessage, sizeof(messageMessage), messageMessage);
            break;
          }
        }
      }
    }

    // if filtering is turned on, filter away the goo
    if (wordfilter != NULL) {
      wordfilter->filter((char *)msg);
    }
    
    std::string origText = stripAnsiCodes(std::string((char*)msg));
    std::string text = BundleMgr::getCurrentBundle()->getLocalString(origText);

    if (toAll || toAdmin || srcPlayer == myTank  || dstPlayer == myTank ||
      dstTeam == myTank->getTeam()) {
      // message is for me
      std::string colorStr;

      if (srcPlayer && srcPlayer->getTeam() != NoTeam)
	colorStr += ColorStrings[srcPlayer->getTeam()];
      else
	colorStr += ColorStrings[RogueTeam];

      fullMsg += colorStr;

      // direct message to or from me
      if (dstPlayer) {
	if (fromServer && (origText == "You are now an administrator!"
	    || origText == "Password Accepted, welcome back."))
	  admin = true;
	  // talking to myself? that's strange
	if (dstPlayer == myTank && srcPlayer == myTank) {
    	  fullMsg = text;
	} else {
	  if (BZDB.get("killerhighlight") == "0")
	    fullMsg += ColorStrings[BlinkColor];
	  else if (BZDB.get("killerhighlight") == "1")
	    fullMsg += ColorStrings[UnderlineColor];
	  fullMsg += "[";
	  if (srcPlayer == myTank) {
	    fullMsg += "->";
	    fullMsg += dstName;
	    fullMsg += colorStr;
	  } else {
	    fullMsg += srcName;
	    fullMsg += colorStr;
	    fullMsg += "->";
	    if (srcPlayer)
	      myTank->setRecipient(srcPlayer);

	    // play a sound on a private message not from self or server
	    if (!fromServer) {
	      static TimeKeeper lastMsg = TimeKeeper::getSunGenesisTime();
	      if (TimeKeeper::getTick() - lastMsg > 2.0f)
		playLocalSound( SFX_MESSAGE_PRIVATE );
	      lastMsg = TimeKeeper::getTick();
	    }
	  } 
	  fullMsg += "]";
	  fullMsg += ColorStrings[ResetColor];
	  fullMsg += " ";
	  fullMsg += ColorStrings[CyanColor];
	  fullMsg += text;
	} 
      } else {
	// team / admin message
	if (toAdmin) {
	  fullMsg += "[Admin] ";
	}

	if (dstTeam != NoTeam) {
#ifdef BWSUPPORT
	  fullMsg = "[to ";
	  fullMsg += Team::getName(TeamColor(dstTeam));
	  fullMsg += "] ";
#else
	  fullMsg += "[Team] ";
#endif

	  // play a sound if I didn't send the message
	  if (srcPlayer != myTank) {
	    static TimeKeeper lastMsg = TimeKeeper::getSunGenesisTime();
	    if (TimeKeeper::getTick() - lastMsg > 2.0f)
	      playLocalSound(SFX_MESSAGE_TEAM);
	    lastMsg = TimeKeeper::getTick();
	  }
	}
	fullMsg += srcName;
	fullMsg += colorStr;
	fullMsg += ": ";
	fullMsg += ColorStrings[CyanColor];
	fullMsg += text;
      }
      std::string oldcolor = "";
      if (!srcPlayer || srcPlayer->getTeam() == NoTeam)
        oldcolor = ColorStrings[RogueTeam];
      else if (srcPlayer->getTeam() == ObserverTeam)
        oldcolor = ColorStrings[CyanColor];
      else
        oldcolor = ColorStrings[srcPlayer->getTeam()];
      addMessage(NULL, fullMsg, false, oldcolor.c_str());

      if (!srcPlayer || srcPlayer!=myTank)
	hud->setAlert(0, fullMsg.c_str(), 3.0f, false);
    }
    break;
  }

  case MsgReplayReset: {
    int i;
    unsigned char lastPlayer;
    msg = nboUnpackUByte(msg, lastPlayer);
    
    // remove players up to 'lastPlayer'
    // any PlayerId above lastPlayer is a replay observers
    for (i=0 ; i<lastPlayer ; i++) {
      if (removePlayer (i)) {
        checkScores = true;
      }
    }
    
    // remove all of the flags
    for (i=0 ; i<numFlags; i++) {
      Flag& flag = world->getFlag(i);
      flag.owner = (PlayerId) -1;
      flag.status = FlagNoExist;
      world->initFlag (i);
    }
    break;
  }
  
    // inter-player relayed message
  case MsgPlayerUpdate:
  case MsgPlayerUpdateSmall:
  case MsgGMUpdate:
  case MsgAudio:
  case MsgVideo:
  case MsgLagPing:
    handlePlayerMessage(code, 0, msg);
    break;
  }

  if (checkScores) updateHighScores();
}

//
// player message handling
//

static void		handlePlayerMessage(uint16_t code, uint16_t,
					    void* msg)
{
  switch (code) {
  case MsgPlayerUpdate: 
  case MsgPlayerUpdateSmall: {
    float timestamp; // could be used to enhance deadreckoning, but isn't for now
    PlayerId id;
    int32_t order;
    void *buf = msg;
    buf = nboUnpackFloat(buf, timestamp);
    buf = nboUnpackUByte(buf, id);
    Player* tank = lookupPlayer(id);
    if (!tank || tank == myTank) break;
    nboUnpackInt(buf, order); // peek! don't update the msg pointer
    if (order <= tank->getOrder()) break;
    short oldStatus = tank->getStatus();
    tank->unpack(msg, code);
    short newStatus = tank->getStatus();
    if ((oldStatus & short(PlayerState::Paused)) !=
	(newStatus & short(PlayerState::Paused)))
      addMessage(tank, (tank->getStatus() & PlayerState::Paused) ?
		 "Paused" : "Resumed");
    if ((oldStatus & short(PlayerState::Exploding)) == 0 &&
	(newStatus & short(PlayerState::Exploding)) != 0) {
      // player has started exploding and we haven't gotten killed
      // message yet -- set explosion now, play sound later (when we
      // get killed message).  status is already !Alive so make player
      // alive again, then call setExplode to kill him.
      tank->setStatus(newStatus | short(PlayerState::Alive));
      tank->setExplode(TimeKeeper::getTick());
      // ROBOT -- play explosion now
    }
    break;
  }

  case MsgGMUpdate: {
    ShotUpdate shot;
    msg = shot.unpack(msg);
    Player* tank = lookupPlayer(shot.player);
    if (!tank || tank == myTank) break;
    RemotePlayer* remoteTank = (RemotePlayer*)tank;
    RemoteShotPath* shotPath =
      (RemoteShotPath*)remoteTank->getShot(shot.id);
    if (shotPath) shotPath->update(shot, code, msg);
    PlayerId targetId;
    msg = nboUnpackUByte(msg, targetId);
    Player* targetTank = lookupPlayer(targetId);
    if (targetTank && (targetTank == myTank) && (myTank->isAlive())) {
      static TimeKeeper lastLockMsg;
      if (TimeKeeper::getTick() - lastLockMsg > 0.75) {
	playWorldSound(SFX_LOCK, shot.pos[0], shot.pos[1], shot.pos[2]);
	lastLockMsg=TimeKeeper::getTick();
	addMessage(tank, "locked on me");
      }
    }
    break;
  }

    // just echo lag ping message
  case MsgLagPing:
    serverLink->send(MsgLagPing,2,msg);
    break;
  }
}

//
// message handling
//

static void		doMessages()
{
  char msg[MaxPacketLen];
  uint16_t code, len;
  int e = 0;

  // handle server messages
  if (serverLink) {
    while (!serverError && (e = serverLink->read(code, len, msg, 0)) == 1)
      handleServerMessage(true, code, len, msg);
    if (e == -2) {
      printError("Server communication error");
      serverError = true;
      return;
    }
  }

#ifdef ROBOT
  for (int i = 0; i < numRobots; i++) {
    while ((e = robotServer[i]->read(code, len, msg, 0)) == 1);
    if (code == MsgKilled || code == MsgShotBegin || code == MsgShotEnd)
      handleServerMessage(false, code, len, msg);
  }
#endif
}

static void		updateFlags(float dt)
{
  for (int i = 0; i < numFlags; i++) {
    Flag& flag = world->getFlag(i);
    if (flag.status == FlagOnTank) {
      // position flag on top of tank
      Player* tank = lookupPlayer(flag.owner);
      if (tank) {
	const float* pos = tank->getPosition();
	flag.position[0] = pos[0];
	flag.position[1] = pos[1];
	flag.position[2] = pos[2] + BZDBCache::tankHeight;
      }
    }
    world->updateFlag(i, dt);
  }
}

bool			addExplosion(const float* _pos,
				     float size, float duration)
{
  // ignore if no prototypes available;
  if (prototypeExplosions.size() == 0) return false;

  // pick a random prototype explosion
  const int index = (int)(bzfrand() * (float)prototypeExplosions.size());

  // make a copy and initialize it
  BillboardSceneNode* newExplosion = prototypeExplosions[index]->copy();
  GLfloat pos[3];
  pos[0] = _pos[0];
  pos[1] = _pos[1];
  pos[2] = _pos[2];
  newExplosion->move(pos);
  newExplosion->setSize(size);
  newExplosion->setDuration(duration);
  newExplosion->setAngle(2.0f * M_PI * (float)bzfrand());
  newExplosion->setLightScaling(size / BZDB.eval(StateDatabase::BZDB_TANKLENGTH));
  newExplosion->setLightFadeStartTime(0.7f * duration);

  // add copy to list of current explosions
  explosions.push_back(newExplosion);

  if (size < (3.0f * BZDB.eval(StateDatabase::BZDB_TANKLENGTH))) return true; // shot explosion

  int boom = (int) (bzfrand() * 8.0) + 3;
  while (boom--) {
    // pick a random prototype explosion
    const int index = (int)(bzfrand() * (float)prototypeExplosions.size());

    // make a copy and initialize it
    BillboardSceneNode* newExplosion = prototypeExplosions[index]->copy();
    GLfloat pos[3];
    pos[0] = _pos[0]+(float)(bzfrand()*12.0 - 6.0);
    pos[1] = _pos[1]+(float)(bzfrand()*12.0 - 6.0);
    pos[2] = _pos[2]+(float)(bzfrand()*10.0);
    newExplosion->move(pos);
    newExplosion->setSize(size);
    newExplosion->setDuration(duration);
    newExplosion->setAngle(2.0f * M_PI * (float)bzfrand());
    newExplosion->setLightScaling(size / BZDB.eval(StateDatabase::BZDB_TANKLENGTH));
    newExplosion->setLightFadeStartTime(0.7f * duration);

    // add copy to list of current explosions
    explosions.push_back(newExplosion);
  }

  return true;
}

void			addTankExplosion(const float* pos)
{
  addExplosion(pos, BZDB.eval(StateDatabase::BZDB_TANKEXPLOSIONSIZE), 1.2f);
}

void			addShotExplosion(const float* pos)
{
  // only play explosion sound if you see an explosion
  if (addExplosion(pos, 1.2f * BZDB.eval(StateDatabase::BZDB_TANKLENGTH), 0.8f))
    playWorldSound(SFX_SHOT_BOOM, pos[0], pos[1], pos[2]);
}

void			addShotPuff(const float* pos)
{
  addExplosion(pos, 0.3f * BZDB.eval(StateDatabase::BZDB_TANKLENGTH), 0.8f);
}

// update events from outside if they should be checked
void                   updateEvents()
{
  if (mainWindow && display) {
    while (display->isEventPending() && !CommandsStandard::isQuit())
      doEvent(display);
  }
}

static void		updateExplosions(float dt)
{
  // update time of all explosions
  int i;
  const int count = explosions.size();
  for (i = 0; i < count; i++)
    explosions[i]->updateTime(dt);

  // reap expired explosions
  for (i = count - 1; i >= 0; i--)
    if (explosions[i]->isAtEnd()) {
      delete explosions[i];
      std::vector<BillboardSceneNode*>::iterator it = explosions.begin();
      for(int j = 0; j < i; j++) it++;
      explosions.erase(it);
    }
}

static void		addExplosions(SceneDatabase* scene)
{
  const int count = explosions.size();
  for (int i = 0; i < count; i++)
    scene->addDynamicNode(explosions[i]);
}

void addDeadUnder (SceneDatabase *db, float dt)
{
  static float texShift = 0.0f;
  static GLfloat black[3] = {0.0f, 0.0f, 0.0f};
  static OpenGLMaterial material(black, black, 0.0f);
  
  float deadUnder = BZDB.eval(StateDatabase::BZDB_DEADUNDER);
  if (deadUnder < 0.0f) {
    return;
  }
  
  float size = BZDB.eval (StateDatabase::BZDB_WORLDSIZE);
  GLfloat base[3] =  {-size/2.0f, -size/2.0f, deadUnder};
  GLfloat sEdge[3] = {0.0f, size, 0.0f};
  GLfloat tEdge[3] = {size, 0.0f, 0.0f};

  texShift = fmodf (texShift + (dt / 30.0f), 1.0f);
  QuadWallSceneNode* node = new QuadWallSceneNode (base, tEdge, sEdge, 
                                                   texShift, 0.0f, 2.0, 2.0, false);

  TextureManager &tm = TextureManager::instance();
  int texture = tm.getTextureID(BZDB.get("deadUnderTexture").c_str(),true);

  GLfloat color[4] = {1.0f, 1.0f, 1.0f, 0.94f};
  if ((!BZDBCache::texture) || (texture < 0)) {
    color[0] = 0.65f;
    color[1] = 1.0f;
    color[2] = 0.5f;
  }
  if (BZDB.isTrue("lighting")) {
    // needs a little tweak
    color[3] = 0.9f;
  }

  node->setColor(color);
  node->setModulateColor(color);
  node->setLightedColor(color);
  node->setLightedModulateColor(color);
  node->setMaterial(material);
  node->setTexture(texture);
  node->setUseColorTexture(false);

  db->addDynamicNode(node);
  
  return;
} 

#ifdef ROBOT
static void		handleMyTankKilled(int reason)
{
  // blow me up
  myTank->explodeTank();
  if (reason == GotRunOver)
    playLocalSound(SFX_RUNOVER);
  else
    playLocalSound(SFX_DIE);

  // i lose a point
  myTank->changeScore(0, 1, 0);
}
#endif

static void *handleMsgSetVars(void *msg)
{
  uint16_t numVars;
  uint8_t nameLen, valueLen;

  char name[MaxPacketLen];
  char value[MaxPacketLen];

  msg = nboUnpackUShort(msg, numVars);
  for (int i = 0; i < numVars; i++) {
    msg = nboUnpackUByte(msg, nameLen);
    msg = nboUnpackString(msg, name, nameLen);
    name[nameLen] = '\0';

    msg = nboUnpackUByte(msg, valueLen);
    msg = nboUnpackString(msg, value, valueLen);
    value[valueLen] = '\0';

    BZDB.set(name, value);
    BZDB.setPersistent(name, false);
    BZDB.setPermission(name, StateDatabase::Locked);
  }
  return msg;
}

void handleFlagDropped(Player* tank)
{
  // skip it if player doesn't actually have a flag
  if (tank->getFlag() == Flags::Null) return;

  if (tank == myTank) {

    // make sure the player must reload after theft
    if (tank->getFlag() == Flags::Thief) {
      myTank->forceReload(BZDB.eval(StateDatabase::BZDB_THIEFDROPTIME));
    }

    // update display and play sound effects
    playLocalSound(SFX_DROP_FLAG);
    updateFlag(Flags::Null);

  }


  // add message
  std::string message("dropped ");
  message += tank->getFlag()->flagName;
  message += " flag";
  addMessage(tank, message);

  // player no longer has flag
  tank->setFlag(Flags::Null);
}

static void	handleFlagTransferred( Player *fromTank, Player *toTank, int flagIndex)
{
  Flag f = world->getFlag(flagIndex);

  fromTank->setFlag(Flags::Null);
  toTank->setFlag(f.type);

  if ((fromTank == myTank) || (toTank == myTank))
    updateFlag(myTank->getFlag());

  const float *pos = toTank->getPosition();
  if (f.type->flagTeam != ::NoTeam) {
    if ((toTank->getTeam() == myTank->getTeam()) && (f.type->flagTeam != myTank->getTeam()))
      playWorldSound(SFX_TEAMGRAB, pos[0], pos[1], pos[2]);
    else if ((fromTank->getTeam() == myTank->getTeam()) && (f.type->flagTeam == myTank->getTeam())) {
      hud->setAlert(1, "Flag Alert!!!", 3.0f, true);
      playLocalSound(SFX_ALERT);
    }
  }

  std::string message(toTank->getCallSign());
  message += " stole ";
  message += fromTank->getCallSign();
  message += "'s flag";
  addMessage(toTank, message);
}

static bool		gotBlowedUp(BaseLocalPlayer* tank,
				    BlowedUpReason reason,
				    PlayerId killer,
				    const ShotPath* hit)
{
  if (tank->getTeam() == ObserverTeam || !tank->isAlive())
    return false;

  int shotId = -1;
  if(hit)
    shotId = hit->getShotId();

  // you can't take it with you
  const FlagType* flag = tank->getFlag();
  if (flag != Flags::Null) {
	if (myTank->isAutoPilot())
	  teachAutoPilot( myTank->getFlag(), -1 );

    // tell other players I've dropped my flag
    lookupServer(tank)->sendDropFlag(tank->getPosition());

    // drop it
    handleFlagDropped(tank);
  }

  // restore the sound, this happens when paused tank dies
  // (genocide or team flag captured)
  if (savedVolume != -1) {
    setSoundVolume(savedVolume);
    savedVolume = -1;
  }

  // take care of explosion business -- don't want to wait for
  // round trip of killed message.  waiting would simplify things,
  // but the delay (2-3 frames usually) can really fool and irritate
  // the player.  we have to be careful to ignore our own Killed
  // message when it gets back to us -- do this by ignoring killed
  // message if we're already dead.
  // don't die if we had the shield flag and we've been shot.
  if (reason != GotShot || flag != Flags::Shield) {
    // blow me up
    tank->explodeTank();
    if (tank == myTank) {
      if (reason == GotRunOver)
	playLocalSound(SFX_RUNOVER);
      else
	playLocalSound(SFX_DIE);
    }
    else {
      const float* pos = tank->getPosition();
      if (reason == GotRunOver)
	playWorldSound(SFX_RUNOVER, pos[0], pos[1], pos[2],
		       getLocalPlayer(killer) == myTank);
      else
	playWorldSound(SFX_EXPLOSION, pos[0], pos[1], pos[2],
		       getLocalPlayer(killer) == myTank);

      float explodePos[3];
      explodePos[0] = pos[0];
      explodePos[1] = pos[1];
      explodePos[2] = pos[2] + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
      addTankExplosion(explodePos);
    }

    // i lose a point
    if (reason != GotCaptured)
      tank->changeScore(0, 1, 0);

    // tell server I'm dead if it won't already know
    if (reason == GotShot || reason == GotRunOver ||
        reason == GenocideEffect || reason == SelfDestruct)
      lookupServer(tank)->sendKilled(killer, reason, shotId);
  }

  // print reason if it's my tank
  if (tank == myTank && blowedUpMessage[reason]) {
    std::string blowedUpNotice = blowedUpMessage[reason];
    // first, check if i'm the culprit
    if (reason == GotShot && getLocalPlayer(killer) == myTank)
      blowedUpNotice = "Shot myself";
    else {
      // 1-4 are messages sent when the player dies because of someone else
      if (reason >= GotShot && reason <= GenocideEffect) {
	// matching the team-display style of other kill messages
	TeamColor team = lookupPlayer(killer)->getTeam();
	if (hit)
	  team = hit->getTeam();
	if (myTank->getTeam() == team && team != RogueTeam) {
	  blowedUpNotice += "teammate " ;
	  blowedUpNotice += lookupPlayer(killer)->getCallSign();
	}
	else {
	  blowedUpNotice += lookupPlayer(killer)->getCallSign();
	  blowedUpNotice += " (";
          if (World::getWorld()->allowRabbit() && lookupPlayer(killer)->getTeam() != RabbitTeam)
            blowedUpNotice+= "Hunter";
          else
	    blowedUpNotice += Team::getName(lookupPlayer(killer)->getTeam());
	  blowedUpNotice += ")";
	}
      }
    }
    hud->setAlert(0, blowedUpNotice.c_str(), 4.0f, true);
    controlPanel->addMessage(blowedUpNotice);
  }

  // make sure shot is terminated locally (if not globally) so it can't
  // hit me again if I had the shield flag.  this is important for the
  // shots that aren't stopped by a hit and so may stick around to hit
  // me on the next update, making the shield useless.
  return (reason == GotShot && flag == Flags::Shield && shotId != -1);
}

static void		checkEnvironment()
{
  if (!myTank || myTank->getTeam() == ObserverTeam) return;

  // skip this if i'm dead or paused
  if (!myTank->isAlive() || myTank->isPaused()) return;

  FlagType* flagd = myTank->getFlag();
  if (flagd->flagTeam != NoTeam) {
    // have I captured a flag?
    TeamColor base = world->whoseBase(myTank->getPosition());
    TeamColor team = myTank->getTeam();
    if ((base != NoTeam) &&
	(flagd->flagTeam == team && base != team) ||
	(flagd->flagTeam != team && base == team))
      serverLink->sendCaptureFlag(base);
  }
  else if (flagd == Flags::Null && (myTank->getLocation() == LocalPlayer::OnGround ||
				    myTank->getLocation() == LocalPlayer::OnBuilding)) {
    // Don't grab too fast
    static TimeKeeper lastGrabSent;
    if (TimeKeeper::getTick()-lastGrabSent > 0.2) {
      // grab any and all flags i'm driving over
      const float* tpos = myTank->getPosition();
      const float radius = myTank->getRadius();
      const float radius2 = (radius + BZDBCache::flagRadius) * (radius + BZDBCache::flagRadius);
      for (int i = 0; i < numFlags; i++) {
	if (world->getFlag(i).type == Flags::Null || world->getFlag(i).status != FlagOnGround)
	  continue;
	const float* fpos = world->getFlag(i).position;
	if ((fabs(tpos[2] - fpos[2]) < 0.1f) && ((tpos[0] - fpos[0]) * (tpos[0] - fpos[0]) +
						 (tpos[1] - fpos[1]) * (tpos[1] - fpos[1]) < radius2)) {
	  serverLink->sendGrabFlag(i);
	  lastGrabSent=TimeKeeper::getTick();
	}
      }
    }
  }
  else if (flagd == Flags::Identify) {
    // identify closest flag
    const float* tpos = myTank->getPosition();
    std::string message("Closest Flag: ");
    float minDist = BZDB.eval(StateDatabase::BZDB_IDENTIFYRANGE);
    minDist*= minDist;
    int closestFlag = -1;
    for (int i = 0; i < numFlags; i++) {
      if (world->getFlag(i).type == Flags::Null ||
	  world->getFlag(i).status != FlagOnGround) continue;
      const float* fpos = world->getFlag(i).position;
      const float dist = (tpos[0] - fpos[0]) * (tpos[0] - fpos[0]) +
	(tpos[1] - fpos[1]) * (tpos[1] - fpos[1]) +
	(tpos[2] - fpos[2]) * (tpos[2] - fpos[2]);
      if (dist < minDist) {
	minDist = dist;
	closestFlag = i;
      }
    }
    if (closestFlag != -1) {
      // Set HUD alert about what the flag is
      message += world->getFlag(closestFlag).type->flagName;
      hud->setAlert(2, message.c_str(), 0.5f,
		    world->getFlag(closestFlag).type->endurance == FlagSticky);
    }
  }

  // see if i've been shot
  const ShotPath* hit = NULL;
  float minTime = Infinity;
  float deadUnder = BZDB.eval(StateDatabase::BZDB_DEADUNDER);
  

  if (myTank->getFlag() != Flags::Thief)
    myTank->checkHit(myTank, hit, minTime);
  int i;
  for (i = 0; i < curMaxPlayers; i++)
    if (player[i])
      myTank->checkHit(player[i], hit, minTime);

  // Check Server Shots
  myTank->checkHit( World::getWorld()->getWorldWeapons(), hit, minTime);

  if (hit) {
    // i got shot!  terminate the shot that hit me and blow up.
    // force shot to terminate locally immediately (no server round trip);
    // this is to ensure that we don't get shot again by the same shot
    // after dropping our shield flag.
    if (hit->isStoppedByHit())
      serverLink->sendEndShot(hit->getPlayer(), hit->getShotId(), 1);

    FlagType* killerFlag = hit->getFlag();
    bool stopShot;

    if (killerFlag == Flags::Thief) {
      if (myTank->getFlag() != Flags::Null) {
	serverLink->sendTransferFlag(myTank->getId(), hit->getPlayer());
      }
      stopShot = true;
    }
    else {
      stopShot = gotBlowedUp(myTank, GotShot, hit->getPlayer(), hit);
    }

    if (stopShot || hit->isStoppedByHit()) {
      Player* hitter = lookupPlayer(hit->getPlayer());
      if (hitter) hitter->endShot(hit->getShotId());
    }
  }

  // if not dead yet, see if i've dropped below the death level
  else if ((deadUnder > 0.0f) && (myTank->getPosition()[2] <= deadUnder)) {
    gotBlowedUp(myTank, SelfDestruct, ServerPlayer);
  }

  // if not dead yet, see if i got run over by the steamroller
  else {
    const float* myPos = myTank->getPosition();
    const float myRadius = myTank->getRadius();
    for (i = 0; i < curMaxPlayers; i++) {
      if (player[i] && !player[i]->isPaused() &&
	  (player[i]->getFlag() == Flags::Steamroller ||
	   (myPos[2] < 0.0f && player[i]->isAlive() &&
	    player[i]->getFlag() != Flags::PhantomZone))) {
	const float* pos = player[i]->getPosition();
	if (pos[2] < 0.0f) continue;
	if (!(flagd == Flags::PhantomZone && myTank->isFlagActive())) {
	  const float radius = myRadius + BZDB.eval(StateDatabase::BZDB_SRRADIUSMULT) * player[i]->getRadius();
	  if (hypot(hypot(myPos[0] - pos[0], myPos[1] - pos[1]), (myPos[2] - pos[2]) * 2.0f) < radius) {
	    gotBlowedUp(myTank, GotRunOver, player[i]->getId());
	  }
	}
      }
    }
  }

#ifdef ROBOT
	checkEnvironmentForRobots();
#endif
}

void setTarget()
{
  // get info about my tank
  const float c = cosf(-myTank->getAngle());
  const float s = sinf(-myTank->getAngle());
  const float x0 = myTank->getPosition()[0];
  const float y0 = myTank->getPosition()[1];

  // initialize best target
  Player* bestTarget = NULL;
  float bestDistance = Infinity;
  bool lockedOn = false;

  // figure out which tank is centered in my sights
  for (int i = 0; i < curMaxPlayers; i++) {
    if (!player[i] || !player[i]->isAlive()) continue;

    // compute position in my local coordinate system
    const float* pos = player[i]->getPosition();
    const float x = c * (pos[0] - x0) - s * (pos[1] - y0);
    const float y = s * (pos[0] - x0) + c * (pos[1] - y0);

    // ignore things behind me
    if (x < 0.0f) continue;

    // get distance and sin(angle) from directly forward
    const float d = hypotf(x, y);
    const float a = fabsf(y / d);

    // see if it's inside lock-on angle (if we're trying to lock-on)
    if (a < BZDB.eval(StateDatabase::BZDB_LOCKONANGLE) &&					// about 8.5 degrees
	myTank->getFlag() == Flags::GuidedMissile &&	// am i locking on?
	player[i]->getFlag() != Flags::Stealth &&		// can't lock on stealth
	!player[i]->isPaused() &&			// can't lock on paused
	!player[i]->isNotResponding() &&		// can't lock on not responding
	d < bestDistance) {				// is it better?
      bestTarget = player[i];
      bestDistance = d;
      lockedOn = true;
    }
    else if (a < BZDB.eval(StateDatabase::BZDB_TARGETINGANGLE) &&				// about 17 degrees
	     ((player[i]->getFlag() != Flags::Stealth) || (myTank->getFlag() == Flags::Seer)) &&	// can't "see" stealth unless have seer
	     d < bestDistance && !lockedOn) {		// is it better?
      bestTarget = player[i];
      bestDistance = d;
    }
  }
  if (!lockedOn) myTank->setTarget(NULL);
  if (!bestTarget) return;

  if (lockedOn) {
    myTank->setTarget(bestTarget);
    myTank->setNemesis(bestTarget);

    std::string msg("Locked on ");
    msg += bestTarget->getCallSign();
    msg += " (";
    if (World::getWorld()->allowRabbit() && bestTarget->getTeam() != RabbitTeam)
      msg+= "Hunter";
    else
      msg += Team::getName(bestTarget->getTeam());
    if (bestTarget->getFlag() != Flags::Null) {
      msg += ") with ";
      msg += bestTarget->getFlag()->flagName;
    }
    else {
      msg += ")";
    }
    hud->setAlert(1, msg.c_str(), 2.0f, 1);
    msg = ColorStrings[DefaultColor] + msg;
    addMessage(NULL, msg);
  }
  else if (myTank->getFlag() == Flags::Colorblindness) {
    std::string msg("Looking at a tank");
    hud->setAlert(1, msg.c_str(), 2.0f, 0);
    msg = ColorStrings[DefaultColor] + msg;
    addMessage(NULL, msg);
  }
  else {
    std::string msg("Looking at ");
    msg += bestTarget->getCallSign();
    msg += " (";
    if (World::getWorld()->allowRabbit() && bestTarget->getTeam() != RabbitTeam)
      msg+= "Hunter";
    else
      msg += Team::getName(bestTarget->getTeam());
    if (bestTarget->getFlag() != Flags::Null) {
      msg += ") with ";
      msg += bestTarget->getFlag()->flagName;
    }
    else {
      msg += ")";
    }
    hud->setAlert(1, msg.c_str(), 2.0f, 0);
    msg = ColorStrings[DefaultColor] + msg;
    addMessage(NULL, msg);
    myTank->setNemesis(bestTarget);
  }
}

static void		setHuntTarget()
{
  // get info about my tank
  const float c = cosf(-myTank->getAngle());
  const float s = sinf(-myTank->getAngle());
  const float x0 = myTank->getPosition()[0];
  const float y0 = myTank->getPosition()[1];

  // initialize best target
  Player* bestTarget = NULL;
  float bestDistance = Infinity;
  bool lockedOn = false;

  // figure out which tank is centered in my sights
  for (int i = 0; i < curMaxPlayers; i++) {
    if (!player[i] || !player[i]->isAlive()) continue;

    // compute position in my local coordinate system
    const float* pos = player[i]->getPosition();
    const float x = c * (pos[0] - x0) - s * (pos[1] - y0);
    const float y = s * (pos[0] - x0) + c * (pos[1] - y0);

    // ignore things behind me
    if (x < 0.0f) continue;

    // get distance and sin(angle) from directly forward
    const float d = hypotf(x, y);
    const float a = fabsf(y / d);

    // see if it's inside lock-on angle (if we're trying to lock-on)
    if (a < BZDB.eval(StateDatabase::BZDB_LOCKONANGLE) &&					// about 8.5 degrees
	myTank->getFlag() == Flags::GuidedMissile &&	// am i locking on?
	player[i]->getFlag() != Flags::Stealth &&	// can't lock on stealth
	!player[i]->isPaused() &&			// can't lock on paused
	!player[i]->isNotResponding() &&		// can't lock on not responding
	d < bestDistance) {				// is it better?
      bestTarget = player[i];
      bestDistance = d;
      lockedOn = true;
    }
    else if (a < BZDB.eval(StateDatabase::BZDB_TARGETINGANGLE) &&				// about 17 degrees
	     ((player[i]->getFlag() != Flags::Stealth) || (myTank->getFlag() == Flags::Seer)) &&	// can't "see" stealth unless have seer
	     d < bestDistance && !lockedOn) {		// is it better?
      bestTarget = player[i];
      bestDistance = d;
    }
  }
  if (!bestTarget) return;


  if (bestTarget->isHunted() && myTank->getFlag() != Flags::Blindness) {
    if (myTank->getTarget() == NULL) { // Don't interfere with GM lock display
      std::string msg("SPOTTED: ");
      msg += bestTarget->getCallSign();
      msg += " (";
      msg += Team::getName(bestTarget->getTeam());
      if (bestTarget->getFlag() != Flags::Null) {
	msg += ") with ";
	msg += bestTarget->getFlag()->flagName;
      } else {
	msg += ")";
      }
      hud->setAlert(1, msg.c_str(), 2.0f, 0);
    }
    if (!pulse.isOn()) {
      const float* bestTargetPosition = bestTarget->getPosition();
      playWorldSound(SFX_HUNT, bestTargetPosition[0], bestTargetPosition[1], bestTargetPosition[2]);
      pulse.setClock(1.0f);
    }
  }
}

static void		updateDaylight(double offset, SceneRenderer& renderer)
{
  static const double SecondsInDay = 86400.0;

  // update sun, moon & sky
  renderer.setTimeOfDay(unixEpoch + offset / SecondsInDay);
}

#ifdef ROBOT

//
// some robot stuff
//

static std::vector<BzfRegion*>	obstacleList;

static void		addObstacle(std::vector<BzfRegion*>& rgnList, const Obstacle& obstacle)
{
  float p[4][2];
  const float* c = obstacle.getPosition();
  const float tankRadius = BZDBCache::tankRadius;

  if (BZDBCache::tankHeight < c[2])
    return;

  const float a = obstacle.getRotation();
  const float w = obstacle.getWidth() + tankRadius;
  const float h = obstacle.getBreadth() + tankRadius;
  const float xx =  w * cosf(a);
  const float xy =  w * sinf(a);
  const float yx = -h * sinf(a);
  const float yy =  h * cosf(a);
  p[0][0] = c[0] - xx - yx;
  p[0][1] = c[1] - xy - yy;
  p[1][0] = c[0] + xx - yx;
  p[1][1] = c[1] + xy - yy;
  p[2][0] = c[0] + xx + yx;
  p[2][1] = c[1] + xy + yy;
  p[3][0] = c[0] - xx + yx;
  p[3][1] = c[1] - xy + yy;

  int numRegions = rgnList.size();
  for (int k = 0; k < numRegions; k++) {
    BzfRegion* region = rgnList[k];
    int side[4];
    if ((side[0] = region->classify(p[0], p[1])) == 1 ||
	(side[1] = region->classify(p[1], p[2])) == 1 ||
	(side[2] = region->classify(p[2], p[3])) == 1 ||
	(side[3] = region->classify(p[3], p[0])) == 1)
      continue;
    if (side[0] == -1 && side[1] == -1 && side[2] == -1 && side[3] == -1) {
      rgnList[k] = rgnList[numRegions-1];
      rgnList[numRegions-1] = rgnList[rgnList.size()-1];
      rgnList.pop_back();
      numRegions--;
      k--;
      delete region;
      continue;
    }
    for (int j = 0; j < 4; j++) {
      if (side[j] == -1) continue;		// to inside
      // split
      const float* p1 = p[j];
      const float* p2 = p[(j+1)&3];
      BzfRegion* newRegion = region->orphanSplitRegion(p2, p1);
      if (!newRegion) continue;		// no split
      if (region != rgnList[k]) rgnList.push_back(region);
      region = newRegion;
    }
    if (region != rgnList[k]) delete region;
  }
}

static void		makeObstacleList()
{
  const float tankRadius = BZDBCache::tankRadius;
  int i;
  const int count = obstacleList.size();
  for (i = 0; i < count; i++)
    delete obstacleList[i];
  obstacleList.clear();

  // FIXME -- shouldn't hard code game area
  float gameArea[4][2];
  float worldSize = BZDB.eval(StateDatabase::BZDB_WORLDSIZE);
  gameArea[0][0] = -0.5f * worldSize + tankRadius;
  gameArea[0][1] = -0.5f * worldSize + tankRadius;
  gameArea[1][0] =  0.5f * worldSize - tankRadius;
  gameArea[1][1] = -0.5f * worldSize + tankRadius;
  gameArea[2][0] =  0.5f * worldSize - tankRadius;
  gameArea[2][1] =  0.5f * worldSize - tankRadius;
  gameArea[3][0] = -0.5f * worldSize + tankRadius;
  gameArea[3][1] =  0.5f * worldSize - tankRadius;
  obstacleList.push_back(new BzfRegion(4, gameArea));

  const std::vector<BoxBuilding>& boxes = World::getWorld()->getBoxes();
  const int numBoxes = boxes.size();
  for (i = 0; i < numBoxes; i++)
    addObstacle(obstacleList, boxes[i]);
  const std::vector<PyramidBuilding>& pyramids = World::getWorld()->getPyramids();
  const int numPyramids = pyramids.size();
  for (i = 0; i < numPyramids; i++)
    addObstacle(obstacleList, pyramids[i]);
  const std::vector<Teleporter>& teleporters = World::getWorld()->getTeleporters();
  const int numTeleporters = teleporters.size();
  for (i = 0; i < numTeleporters; i++)
    addObstacle(obstacleList, teleporters[i]);
  if (World::getWorld()->allowTeamFlags()) {
    const std::vector<BaseBuilding>& bases = World::getWorld()->getBases();
    const int numBases = bases.size();
    for (i = 0; i < numBases; i++) {
      if ((bases[i].getHeight() != 0.0f) || (bases[i].getPosition()[2] != 0.0f))
        addObstacle(obstacleList, bases[i]);
    }
  }
}

static void		setRobotTarget(RobotPlayer* robot)
{
  Player* bestTarget = NULL;
  float bestPriority = 0.0f;
  for (int j = 0; j < curMaxPlayers; j++)
    if (player[j] && player[j]->getId() != robot->getId() &&
	player[j]->isAlive() &&
	((robot->getTeam() == RogueTeam && !World::getWorld()->allowRabbit())
	 || player[j]->getTeam() != robot->getTeam())) {

      if((player[j]->getFlag() == Flags::PhantomZone && player[j]->isFlagActive()))
        continue;

      if (World::getWorld()->allowTeamFlags() && 
      	  (robot->getTeam() == RedTeam && player[j]->getFlag() == Flags::RedTeam) ||
      	  (robot->getTeam() == GreenTeam && player[j]->getFlag() == Flags::GreenTeam) ||
      	  (robot->getTeam() == BlueTeam && player[j]->getFlag() == Flags::BlueTeam) ||
	  (robot->getTeam() == PurpleTeam && player[j]->getFlag() == Flags::PurpleTeam)) {
	bestTarget = player[j];
	break;
      }

      const float priority = robot->getTargetPriority(player[j]);
      if (priority > bestPriority) {
	bestTarget = player[j];
	bestPriority = priority;
      }
    }
  if (myTank->isAlive() &&
      ((robot->getTeam() == RogueTeam && !World::getWorld()->allowRabbit()) ||
       myTank->getTeam() != robot->getTeam())) {
    const float priority = robot->getTargetPriority(myTank);
    if (priority > bestPriority) {
      bestTarget = myTank;
      bestPriority = priority;
    }
  }
  robot->setTarget(bestTarget);
}

static void		updateRobots(float dt)
{
  static float newTargetTimeout = 1.0f;
  static float clock = 0.0f;
  bool pickTarget = false;
  int i;

  // see if we should look for new targets
  clock += dt;
  if (clock > newTargetTimeout) {
    while (clock > newTargetTimeout) clock -= newTargetTimeout;
    pickTarget = true;
  }

  // start dead robots
  for (i = 0; i < numRobots; i++)
    if (!gameOver && !robots[i]->isAlive() && !robots[i]->isExploding()
	&& pickTarget)
      robotServer[i]->sendAlive();

  // retarget robots
  for (i = 0; i < numRobots; i++)
    if (robots[i]->isAlive() && (pickTarget || !robots[i]->getTarget()
				 || !robots[i]->getTarget()->isAlive()))
      setRobotTarget(robots[i]);

  // do updates
  for (i = 0; i < numRobots; i++)
    robots[i]->update();
}


static void		checkEnvironment(RobotPlayer* tank)
{
  // skip this if i'm dead or paused
  if (!tank->isAlive() || tank->isPaused()) return;

  // see if i've been shot
  const ShotPath* hit = NULL;
  float minTime = Infinity;
  tank->checkHit(myTank, hit, minTime);
  int i;
  for (i = 0; i < curMaxPlayers; i++)
    if (player[i] && player[i]->getId() != tank->getId())
      tank->checkHit(player[i], hit, minTime);

  // Check Server Shots
  tank->checkHit( World::getWorld()->getWorldWeapons(), hit, minTime);

  float deadUnder = BZDB.eval(StateDatabase::BZDB_DEADUNDER);
  
  if (hit) {
    // i got shot!  terminate the shot that hit me and blow up.
    // force shot to terminate locally immediately (no server round trip);
    // this is to ensure that we don't get shot again by the same shot
    // after dropping our shield flag.
    if (hit->isStoppedByHit())
      lookupServer(tank)->sendEndShot(hit->getPlayer(), hit->getShotId(), 1);

    FlagType* killerFlag = hit->getFlag();
    bool stopShot;

    if (killerFlag == Flags::Thief) {
      if (tank->getFlag() != Flags::Null) {
	serverLink->sendTransferFlag(tank->getId(), hit->getPlayer());
      }
      stopShot = true;
    }
    else {
      stopShot = gotBlowedUp(tank, GotShot, hit->getPlayer(), hit);
    }

    if (stopShot || hit->isStoppedByHit()) {
      Player* hitter = lookupPlayer(hit->getPlayer());
      if (hitter) hitter->endShot(hit->getShotId());
    }
  }

  // if not dead yet, see if the robot dropped below the death level
  else if ((deadUnder > 0.0f) && (tank->getPosition()[2] <= deadUnder)) {
    gotBlowedUp(tank, SelfDestruct, ServerPlayer);
  }

  // if not dead yet, see if i got run over by the steamroller
  else {
    bool dead = false;
    const float* myPos = tank->getPosition();
    const float myRadius = tank->getRadius();
    if (((myTank->getFlag() == Flags::Steamroller) ||
	 ((tank->getFlag() == Flags::Burrow) && myTank->isAlive() &&
	  myTank->getFlag() != Flags::PhantomZone)) && !myTank->isPaused()) {
      const float* pos = myTank->getPosition();
      if (pos[2] >= 0.0f) {
	const float radius = myRadius + BZDB.eval(StateDatabase::BZDB_SRRADIUSMULT) * myTank->getRadius();
	if (hypot(hypot(myPos[0] - pos[0], myPos[1] - pos[1]), myPos[2] - pos[2]) < radius) {
	  gotBlowedUp(tank, GotRunOver, myTank->getId());
	  dead = true;
	}
      }
    }
    for (i = 0; !dead && i < curMaxPlayers; i++) {
      if (player[i] && !player[i]->isPaused() &&
	  ((player[i]->getFlag() == Flags::Steamroller) ||
	   ((tank->getFlag() == Flags::Burrow) && player[i]->isAlive()) &&
	    (player[i]->getFlag() != Flags::PhantomZone))) {
	const float* pos = player[i]->getPosition();
	if (pos[2] < 0.0f) continue;
	const float radius = myRadius + BZDB.eval(StateDatabase::BZDB_SRRADIUSMULT) * player[i]->getRadius();
	if (hypot(hypot(myPos[0] - pos[0], myPos[1] - pos[1]), myPos[2] - pos[2]) < radius) {
	  gotBlowedUp(tank, GotRunOver, player[i]->getId());
	  dead = true;
	}
      }
    }
  }
}

void		checkEnvironmentForRobots()
{
  for (int i = 0; i < numRobots; i++)
    checkEnvironment(robots[i]);
}

static void		sendRobotUpdates()
{
  for (int i = 0; i < numRobots; i++)
    if (robots[i]->isDeadReckoningWrong()) {
      robotServer[i]->sendPlayerUpdate(robots[i]);
    }
}

static void		addRobots()
{
  uint16_t code, len;
  char msg[MaxPacketLen];
  char callsign[CallSignLen];

  for (int j = 0; j < numRobots;) {

    snprintf(callsign, CallSignLen, "%s%2.2d", myTank->getCallSign(), j);

    robots[j] = new RobotPlayer(robotServer[j]->getId(), callsign, robotServer[j], myTank->getEmailAddress());
    robots[j]->setTeam(AutomaticTeam);
    robotServer[j]->sendEnter(ComputerPlayer, robots[j]->getTeam(),
			      robots[j]->getCallSign(), robots[j]->getEmailAddress());

    // wait for response
    if (robotServer[j]->read(code, len, msg, -1) < 0 || code != MsgAccept) {
      delete robots[j];
      delete robotServer[j];
      robots[j] = NULL;
      robotServer[j] = robotServer[--numRobots];
      robotServer[numRobots] = NULL;
      continue;
    }

    j++;
  }
  makeObstacleList();
  RobotPlayer::setObstacleList(&obstacleList);
}

#endif

static void cleanWorldCache()
{
  char buffer[10];
  int cacheLimit = 100L * 1024L;
  if (BZDB.isSet("worldCacheLimit")) {
    cacheLimit = atoi(BZDB.get("worldCacheLimit").c_str());
  } else {
    snprintf(buffer, 10, "%d", cacheLimit);
    BZDB.set("worldCacheLimit", buffer);
  }

  std::string worldPath = getCacheDirName();

  char *oldestFile = NULL;
  int oldestSize = 0;
  int totalSize = 0;

  do {
    oldestFile = 0;
    totalSize = 0;
#ifdef _WIN32
    std::string pattern = worldPath + "/*.bwc";

    WIN32_FIND_DATA findData;
    HANDLE h = FindFirstFile(pattern.c_str(), &findData);
    if (h != INVALID_HANDLE_VALUE) {
      FILETIME oldestTime = findData.ftLastAccessTime;
      oldestFile = strdup(findData.cFileName);
      oldestSize = findData.nFileSizeLow;
      totalSize = findData.nFileSizeLow;

      while (FindNextFile(h, &findData)) {
	if (CompareFileTime( &oldestTime, &findData.ftLastAccessTime ) > 0) {
	  oldestTime = findData.ftLastAccessTime;
	  if (oldestFile)
	    free(oldestFile);
	  oldestFile = strdup(findData.cFileName);
	  oldestSize = findData.nFileSizeLow;
	}
	totalSize += findData.nFileSizeLow;
      }
      FindClose(h);
    }
#else
    DIR *directory = opendir(worldPath.c_str());
    if (directory) {
      struct dirent* contents;
      struct stat statbuf;
      time_t oldestTime = time(NULL);
      while ((contents = readdir(directory))) {
	stat((worldPath + "/" + contents->d_name).c_str(), &statbuf);
	if (statbuf.st_atime < oldestTime) {
	  if (oldestFile)
	    free(oldestFile);
	  oldestFile = strdup(contents->d_name);
	  oldestSize = statbuf.st_size;
	}
	totalSize += statbuf.st_size;
      }
      closedir(directory);

    }
#endif

    if (totalSize < cacheLimit) {
      if (oldestFile != NULL) {
	free(oldestFile);
	oldestFile = NULL;
      }
      return;
    }

    if (oldestFile != NULL)
      remove((worldPath + "/" + oldestFile).c_str());

    if (oldestFile != NULL)
      free(oldestFile);
    totalSize -= oldestSize;
  } while (oldestFile && (totalSize > cacheLimit));
}

static void markOld(std::string &fileName)
{
#ifdef _WIN32
  FILETIME ft;
  HANDLE h = CreateFile(fileName.c_str(), FILE_WRITE_ATTRIBUTES|FILE_WRITE_EA, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {

    SYSTEMTIME st;
    memset( &st, 0, sizeof(st));
    st.wYear = 1900;
    st.wMonth = 1;
    st.wDay = 1;
    SystemTimeToFileTime( &st, &ft );
    SetFileTime(h, &ft, &ft, &ft);
    GetLastError();
    CloseHandle(h);
  }
#else
  struct utimbuf times;
  times.actime = 0;
  times.modtime = 0;
  utime(fileName.c_str(), &times);
#endif
}

static bool negotiateFlags(ServerLink* serverLink)
{
  uint16_t code, len;
  char msg[MaxPacketLen];
  char *buf = msg;
  FlagTypeMap::iterator i;

  /* Send MsgNegotiateFlags to the server with
   * the abbreviations for all the flags we support.
   */
  for (i = FlagType::getFlagMap().begin();
       i != FlagType::getFlagMap().end(); i++) {
    buf = (char*) i->second->pack(buf);
  }
  serverLink->send( MsgNegotiateFlags, buf - msg, msg );

  /* Response should always be a MsgNegotiateFlags. If not, assume the server
   * is too old or new to understand our flag system.
   */
  if (serverLink->read(code, len, msg, 5000)<=0 || code != MsgNegotiateFlags) {
    printError("Unsupported response from server during flag negotiation");
    return false;
  }

  /* The response contains a list of flags we're missing. If it's empty,
   * we're good to go. Otherwise, try to give a good error messages.
   */
  if (len > 0) {
    int i;
    int numFlags = len/2;
    std::string flags;
    buf = msg;

    for (i=0; i<numFlags; i++) {
      /* We can't use FlagType::unpack() here, since it counts on the
       * flags existing in our flag database.
       */
      if (i)
	flags += ", ";
      flags += buf[0];
      if (buf[1])
	flags += buf[1];
      buf += 2;
    }

    std::vector<std::string> args;
    args.push_back(flags);
    printError("Flags not supported by this client: {1}", &args);

    return false;
  }

  return true;
}

//
// join/leave a game
//

static World*		makeWorld(ServerLink* serverLink)
{
  std::istream *cachedWorld = NULL;
  uint16_t code, len;
  uint32_t size;
  char msg[MaxPacketLen];
  std::string worldPath;
  bool isTemp = false;

  //ask for the hash of the world (ignoring all other messages)
  serverLink->send( MsgWantWHash, 0, NULL );
  if (serverLink->read(code, len, msg, 5000) > 0) {
    if (code != MsgWantWHash) return NULL;

    char *hexDigest = new char[len];
    nboUnpackString( msg, hexDigest, len );
    isTemp = hexDigest[0] == 't';

    worldPath = getCacheDirName();
    worldPath += hexDigest;
    worldPath += ".bwc";
    cachedWorld = FILEMGR.createDataInStream(worldPath, true);
    delete[] hexDigest;
  }

  char* worldDatabase;
  if (cachedWorld == NULL) {
    // ask for world and wait for it (ignoring all other messages)
    nboPackUInt(msg, 0);
    serverLink->send(MsgGetWorld, sizeof(uint32_t), msg);
    if (serverLink->read(code, len, msg, 5000) <= 0) return NULL;
    if (code == MsgNull || code == MsgSuperKill) return NULL;
    if (code != MsgGetWorld) return NULL;

    // get size of entire world database and make space
    uint32_t bytesLeft;
    void *buf = nboUnpackUInt(msg, bytesLeft);
    size = bytesLeft + len - 4;
    worldDatabase = new char[size];

    // get world database
    uint32_t ptr = 0;
    while (bytesLeft != 0) {
      // add chunk to database so far
      ::memcpy(worldDatabase + int(ptr), buf, len - sizeof(uint32_t));

      // increment pointer
      ptr += len - sizeof(uint32_t);
      // ask and wait for next chunk
      nboPackUInt(msg, ptr);
      serverLink->send(MsgGetWorld, sizeof(uint32_t), msg);
      if (serverLink->read(code, len, msg, 5000) < 0 ||
	  code == MsgNull || code == MsgSuperKill) {
	delete[] worldDatabase;
	return NULL;
      }
      // get bytes left
      buf = nboUnpackUInt(msg, bytesLeft);
    }
    //add final chunk
    ::memcpy(worldDatabase + int(ptr), buf, len - sizeof(uint32_t));

    if (worldPath.length() > 0) {
      cleanWorldCache();
      std::ostream* cacheOut = FILEMGR.createDataOutStream(worldPath, true, true);
      if (cacheOut != NULL) {
	cacheOut->write(worldDatabase, size);
	delete cacheOut;
	if (isTemp)
	  markOld(worldPath);
      }
    }
  }
  else
    {
      cachedWorld->seekg(0, std::ios::end);
      std::streampos size = cachedWorld->tellg();
      unsigned long charSize = std::streamoff(size);
      cachedWorld->seekg(0);
      worldDatabase = new char[charSize];
      cachedWorld->read(worldDatabase, charSize);
    }

  // make world
  WorldBuilder worldBuilder;
  if (!worldBuilder.unpack(worldDatabase)){		// world didn't make for some reason
    delete[] worldDatabase;
    return NULL;
  }

  delete[] worldDatabase;
  delete cachedWorld;

  // return world
  return worldBuilder.getWorld();
}

static bool		enterServer(ServerLink* serverLink, World* world,
				    LocalPlayer* myTank)
{

  time_t timeout=time(0) + 15;  // give us 15 sec

  if (world->allowRabbit() && myTank->getTeam() != ObserverTeam)
    myTank->setTeam(RogueTeam);

  // tell server we want to join
  serverLink->sendEnter(TankPlayer, myTank->getTeam(),
			myTank->getCallSign(), myTank->getEmailAddress());

  // see if the server is feeling agreeable
  uint16_t code, rejcode;
  std::string reason;
  if (!serverLink->readEnter(reason, code, rejcode)) {
    printError(reason);
    return false;
  }

  // get updates
  uint16_t len;
  char msg[MaxPacketLen];
  
  if (serverLink->read(code, len, msg, -1) < 0) {
    goto failed;
  }
  while (code == MsgAddPlayer || code == MsgTeamUpdate ||
	 code == MsgFlagUpdate || code == MsgUDPLinkRequest ||
	 code == MsgSetVar) {
    void* buf = msg;
    switch (code) {
    case MsgAddPlayer: {
      PlayerId id;
      buf = nboUnpackUByte(buf, id);
      if (id == myTank->getId()) {
	// it's me!  end of updates

	// the server sends back the team the player was joined to
	void *tmpbuf = buf;
	uint16_t team, type;
	tmpbuf = nboUnpackUShort(tmpbuf, type);
	tmpbuf = nboUnpackUShort(tmpbuf, team);

	// if server assigns us a different team, display a message
	std::string teamMsg;
	if (myTank->getTeam() != AutomaticTeam) {
	  teamMsg = string_util::format("%s team was unavailable, you were joined ",
					Team::getName(myTank->getTeam()));
	  if ((TeamColor)team == ObserverTeam) {
	    teamMsg += "as an Observer";
	  } else {
	    teamMsg += string_util::format("to the %s", 
					   Team::getName((TeamColor)team));
	  }
	} else {
	  if ((TeamColor)team == ObserverTeam) {
	    teamMsg = "You were joined as an observer";
	  } else {
            if ( team != RogueTeam)
	      teamMsg = string_util::format("You joined the %s",Team::getName((TeamColor)team));
            else
              teamMsg = string_util::format("You joined as a %s",Team::getName((TeamColor)team));
	  }
	}
	if (myTank->getTeam() != (TeamColor)team) {
	  myTank->setTeam((TeamColor)team);
	  hud->setAlert(1, teamMsg.c_str(), 8.0f, (TeamColor)team==ObserverTeam?true:false);
	  addMessage(NULL, teamMsg.c_str(), true);
	}
	controlPanel->setControlColor(Team::getRadarColor(myTank->getTeam()));
	radar->setControlColor(Team::getRadarColor(myTank->getTeam()));
	roaming = (myTank->getTeam() == ObserverTeam);

	// scan through flags and, for flags on
	// tanks, tell the tank about its flag.
	const int maxFlags = world->getMaxFlags();
	for (int i = 0; i < maxFlags; i++) {
	  const Flag& flag = world->getFlag(i);
	  if (flag.status == FlagOnTank)
	    for (int j = 0; j < curMaxPlayers; j++)
	      if (player[j] && player[j]->getId() == flag.owner) {
		player[j]->setFlag(flag.type);
		break;
	      }
	}
	return true;
      }
      addPlayer(id, buf, false);
      break;
    }
    case MsgTeamUpdate: {
      uint8_t  numTeams;
      uint16_t team;
      buf = nboUnpackUByte(buf,numTeams);
      for (int i = 0; i < numTeams; i++) {
	buf = nboUnpackUShort(buf, team);
	buf = teams[int(team)].unpack(buf);
      }
      break;
    }
    case MsgFlagUpdate: {
      uint16_t count;
      uint16_t flag;
      buf = nboUnpackUShort(buf, count);
      for (int i = 0; i < count; i++) {
	buf = nboUnpackUShort(buf, flag);
	buf = world->getFlag(int(flag)).unpack(buf);
	world->initFlag(int(flag));
      }
      break;
    }
    case MsgUDPLinkRequest:
      printError("*** Received UDP Link Request");
      // internally <- TimRiker says huh? what's "internally" mean?
      break;
    case MsgSetVar: {
      buf = handleMsgSetVars(buf);
      break;
    }
    }

    if (time(0)>timeout) goto failed;

    if (serverLink->read(code, len, msg, -1) < 0) goto failed;
  }

 failed:
  printError("Communication error joining game");
  return false;
}

void		leaveGame()
{
  // delete scene database
  sceneRenderer->setSceneDatabase(NULL);
  delete zScene;
  delete bspScene;
  zScene = NULL;
  bspScene = NULL;


  // no more radar
  controlPanel->setRadarRenderer(NULL);
  delete radar;
  radar = NULL;

#if defined(ROBOT)
  // shut down robot connections
  int i;
  for (i = 0; i < numRobots; i++) {
    if (robots[i] && robotServer[i])
      robotServer[i]->send(MsgExit, 0, NULL);
    delete robots[i];
    delete robotServer[i];
    robots[i] = NULL;
    robotServer[i] = NULL;
  }
  numRobots = 0;

  const int count = obstacleList.size();
  for (i = 0; i < count; i++)
    delete obstacleList[i];
  obstacleList.clear();
#endif

  // my tank goes away
  const bool sayGoodbye = (myTank != NULL);
  LocalPlayer::setMyTank(NULL);
  delete myTank;
  myTank = NULL;

  // time goes back to current time if previously constrained by server
  if (BZDB.isTrue(StateDatabase::BZDB_SYNCTIME)) {
    epochOffset = userTimeEpochOffset;
    updateDaylight(epochOffset, *sceneRenderer);
    lastEpochOffset = epochOffset;
  }

  // delete world
  World::setWorld(NULL);
  delete world;
  world = NULL;
  teams = NULL;
  curMaxPlayers = 0;
  numFlags = 0;
  player = NULL;

  // update UI
  hud->setPlaying(false);
  hud->setCracks(false);
  hud->setPlayerHasHighScore(false);
  hud->setTeamHasHighScore(false);
  hud->setHeading(0.0f);
  hud->setAltitude(0.0f);
  hud->setAltitudeTape(false);

  // shut down server connection
  if (sayGoodbye) serverLink->send(MsgExit, 0, NULL);
  ServerLink::setServer(NULL);
  delete serverLink;
  serverLink = NULL;

  // reset viewpoint
  float eyePoint[3], targetPoint[3];
  float worldSize = BZDB.eval(StateDatabase::BZDB_WORLDSIZE);
  eyePoint[0] = 0.0f;
  eyePoint[1] = 0.0f;
  eyePoint[2] = 0.0f + BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
  targetPoint[0] = eyePoint[0] - 1.0f;
  targetPoint[1] = eyePoint[1] + 0.0f;
  targetPoint[2] = eyePoint[2] + 0.0f;
  sceneRenderer->getViewFrustum().setProjection(60.0f * M_PI / 180.0f,
						1.1f, 1.5f * worldSize, mainWindow->getWidth(),
						mainWindow->getHeight(), mainWindow->getViewHeight());
  sceneRenderer->getViewFrustum().setView(eyePoint, targetPoint);

  // reset some flags
  gameOver = false;
  serverError = false;
  serverDied = false;
}

static bool		joinGame(const StartupInfo* info,
				 ServerLink* _serverLink)
{
  // assume everything's okay for now
  serverDied = false;
  serverError = false;
  admin = false;

  serverLink = _serverLink;

  if (!serverLink) {
    printError("Memory error");
    leaveGame();
    return false;
  }
//
  // printError("Join Game");
  // check server
  if (serverLink->getState() != ServerLink::Okay) {
    switch (serverLink->getState()) {
    case ServerLink::BadVersion: {
      static char versionError[] = "Incompatible server version XXXXXXXX";
      strncpy(versionError + strlen(versionError) - 8,
	      serverLink->getVersion(), 8);
      printError(versionError);
      break;
    }

    case ServerLink::Rejected:
      // the server is probably full or the game is over.  if not then
      // the server is having network problems.
      printError("Game is full or over.  Try again later.");
      break;

    case ServerLink::SocketError:
      printError("Error connecting to server.");
      break;

    case ServerLink::CrippledVersion:
      // can't connect to (otherwise compatible) non-crippled server
      printError("Cannot connect to full version server.");
      break;

    default:
      printError("Internal error connecting to server.");
      break;
    }

    leaveGame();
    return false;
  }

  if (!negotiateFlags(serverLink)) {
    leaveGame();
    return false;
  }

  // create world
  world = makeWorld(serverLink);
  if (!world) {
    printError("Error downloading world database");
    leaveGame();
    return false;
  }

  ServerLink::setServer(serverLink);
  World::setWorld(world);

  // prep teams
  teams = world->getTeams();

  // prep players
  curMaxPlayers = 0;
  player = world->getPlayers();

  // prep flags
  numFlags = world->getMaxFlags();

  // make scene database
  const bool oldUseZBuffer = BZDB.isTrue("zbuffer");
  BZDB.set("zbuffer", "0");
  bspScene = sceneBuilder->make(world);
  BZDB.set("zbuffer", "1");
  // FIXME - test the zbuffer here
  if (BZDB.isTrue("zbuffer"))
    zScene = sceneBuilder->make(world);
  BZDB.set("zbuffer", oldUseZBuffer ? "1" : "0");
  setSceneDatabase();


  mainWindow->getWindow()->yieldCurrent();
  // make radar
  radar = new RadarRenderer(*sceneRenderer, *world);
  mainWindow->getWindow()->yieldCurrent();

  controlPanel->setRadarRenderer(radar);
  controlPanel->resize();

  // make local player
  myTank = new LocalPlayer(serverLink->getId(), info->callsign, info->email);
  myTank->setTeam(info->team);
  LocalPlayer::setMyTank(myTank);

  // enter server
  if (!enterServer(serverLink, world, myTank)) {
    delete myTank;
    myTank = NULL;
    leaveGame();
    return false;
  }

  // use parallel UDP if desired and using server relay
  if (startupInfo.useUDPconnection)
    serverLink->sendUDPlinkRequest();
  else
    printError("No UDP connection, see Options to enable.");

  // send my version string
  serverLink->sendVersionString();

  // add robot tanks
#if defined(ROBOT)
  addRobots();
#endif

  // resize background and adjust time (this is needed even if we
  // don't sync with the server)
  sceneRenderer->getBackground()->resize();
  if (!BZDB.isTrue(StateDatabase::BZDB_SYNCTIME)) {
    updateDaylight(epochOffset, *sceneRenderer);
  }
  else {
    epochOffset = double(world->getEpochOffset());
    updateDaylight(epochOffset, *sceneRenderer);
    lastEpochOffset = epochOffset;
  }

  // restore the sound
  if (savedVolume != -1) {
    setSoundVolume(savedVolume);
    savedVolume = -1;
  }

  // initialize some other stuff
  updateNumPlayers();
  updateFlag(Flags::Null);
  updateHighScores();
  hud->setHeading(myTank->getAngle());
  hud->setAltitude(myTank->getPosition()[2]);
  hud->setTimeLeft(-1);
  fireButton = false;
  firstLife = true;
  
  warnAboutMainFlags();
  warnAboutRadarFlags();
  
  return true;
}

static bool		joinInternetGame(const StartupInfo* info)
{
  // open server
  Address serverAddress(info->serverName);
  if (serverAddress.isAny()) return false;
  ServerLink* serverLink = new ServerLink(serverAddress, info->serverPort);

  Address multicastAddress(BroadcastAddress);

#if defined(ROBOT)
  extern int numRobotTanks;
  int i, j;
  for (i = 0, j = 0; i < numRobotTanks; i++) {
    robotServer[j] = new ServerLink(serverAddress, info->serverPort, j + 1);
    if (!robotServer[j] || robotServer[j]->getState() != ServerLink::Okay) {
      delete robotServer[j];
      continue;
    }
    j++;
  }
  numRobots = j;
#endif

  return joinGame(info, serverLink);
}

static bool		joinGame()
{
  // assume we have everything we need to join.  figure out how
  // to join by which arguments are set in StartupInfo.
  // currently only support joinInternetGame.
  if (startupInfo.serverName[0])
    return joinInternetGame(&startupInfo);

  // can't figure out how to join
  printError("Can't figure out how to join.");
  return false;
}

static void		renderDialog()
{
  if (HUDDialogStack::get()->isActive())
	{
    const int width = mainWindow->getWidth();
    const int height = mainWindow->getHeight();
    const int ox = mainWindow->getOriginX();
    const int oy = mainWindow->getOriginY();
    glScissor(ox, oy, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    OpenGLGState::resetState();
    HUDDialogStack::get()->render();
    glPopMatrix();
  }
}

static int		getZoomFactor()
{
  if (!BZDB.isSet("displayZoom")) return 1;
  const int zoom = atoi(BZDB.get("displayZoom").c_str());
  if (zoom < 1) return 1;
  if (zoom > 8) return 8;
  return zoom;
}


void renderManagedDialog ( void )
{
	if (HUDDialogStack::get()->isActive())
	{
		const int width = mainWindow->getWidth();
		const int height = mainWindow->getHeight();
		const int ox = mainWindow->getOriginX();
		const int oy = mainWindow->getOriginY();

		glPushMatrix();
	/*	glScissor(ox, oy, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity(); */
		static float shiftX = 0;
		static float shiftY = 0;
		glBegin(GL_LINES);
			glColor4f(1,1,1,1);
			glVertex3f(0,0,-1);
			glVertex3f(width+shiftX++,height+shiftY++,-1);
		glEnd();
//		OpenGLGState::resetState();
	//HUDDialogStack::get()->render();

//		OpenGLGState::resetState();

		glEnable(GL_LIGHTING);
		glPopMatrix();
	}
}

//
// drawing ( new )
//
void drawManagedScene ( float fov )
{
	View3D	&view = View3D::instance();

	view.transform();
	// compute the visible objects
	VisualElementManager::instance().calcVisObjects();

	// tell em to draw
	VisualElementManager::instance().drawVisObjects();

	// draw the things in texutre/pass order
	DrawablesManager::instance().draw();

	view.setOrtho();
	// do the radar and stuff

//	hud->render(*sceneRenderer);
	renderManagedDialog();
//	controlPanel->render(*sceneRenderer);
//	if (radar)
//		radar->render(*sceneRenderer, myTank && myTank->isPaused());

	view.unsetOrtho();
	view.untransform();
	mainWindow->getWindow()->swapBuffers();
}

//
// drawing
//
void drawScene ( float fov )
{
	float worldSize = BZDB.eval(StateDatabase::BZDB_WORLDSIZE);
	bool useManagers = BZDB.isTrue("useNewRendering");

	if (useManagers)
	{
		drawManagedScene(fov);
		return;
	}

	BzfMedia* media = PlatformFactory::getMedia();

	// get view type (constant for entire game)
	const int zoomFactor = getZoomFactor();
	const bool fakeCursor = BZDB.isTrue("fakecursor");
	mainWindow->setZoomFactor(zoomFactor);
	if (fakeCursor)
		mainWindow->getWindow()->hideMouse();

	// draw frame
	const bool blankRadar = myTank && myTank->isPaused();

	// are we zoomed
	if (zoomFactor != 1)
	{
		// draw small out-the-window view
		mainWindow->setQuadrant(MainWindow::ZoomRegion);
		const int x = mainWindow->getOriginX();
		const int y = mainWindow->getOriginY();
		const int w = mainWindow->getWidth();
		const int h = mainWindow->getHeight();
		const int vh = mainWindow->getViewHeight();
		sceneRenderer->getViewFrustum().setProjection(fov, 1.1f, 1.5f * worldSize, w, h, vh);
		sceneRenderer->render();

		// set entire window
		mainWindow->setQuadrant(MainWindow::FullWindow);
		glScissor(mainWindow->getOriginX(), mainWindow->getOriginY(),
			mainWindow->getWidth(), mainWindow->getHeight());

		// set pixel copy destination
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-0.25, (GLdouble)mainWindow->getWidth() - 0.25,
			-0.25, (GLdouble)mainWindow->getHeight() - 0.25, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glRasterPos2i(0, 0);
		glPopMatrix();

		// zoom small image to entire window
		glPixelZoom((float)zoomFactor, (float)zoomFactor);
		glCopyPixels(x, y, w, h, GL_COLOR);
		glPixelZoom(1.0f, 1.0f);
	}
	else
	{
		// normal rendering
		sceneRenderer->render();
	}

	// draw other stuff
	hud->render(*sceneRenderer);
	renderDialog();
	controlPanel->render(*sceneRenderer);
	if (radar)
		radar->render(*sceneRenderer, blankRadar);


	// get frame end time
	if (showDrawTime) {
#if defined(DEBUG_RENDERING)
		// get an accurate measure of frame time (at expense of frame rate)
		glFinish();
#endif
		hud->setDrawTime((float)media->stopwatch(false));
	}

	// draw a fake cursor if requested.  this is mostly intended for
	// pass through 3D cards that don't have cursor support.
	if (fakeCursor) {
		int mx, my;
		const int width = mainWindow->getWidth();
		const int height = mainWindow->getHeight();
		const int ox = mainWindow->getOriginX();
		const int oy = mainWindow->getOriginY();
		mainWindow->getWindow()->getMouse(mx, my);
		my = height - my - 1;

		glScissor(ox, oy, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glColor3f(0.0f, 0.0f, 0.0f);
		glRecti(mx - 8, my - 2, mx - 2, my + 2);
		glRecti(mx + 2, my - 2, mx + 8, my + 2);
		glRecti(mx - 2, my - 8, mx + 2, my - 2);
		glRecti(mx - 2, my + 2, mx + 2, my + 8);

		glColor3f(1.0f, 1.0f, 1.0f);
		glRecti(mx - 7, my - 1, mx - 3, my + 1);
		glRecti(mx + 3, my - 1, mx + 7, my + 1);
		glRecti(mx - 1, my - 7, mx + 1, my - 3);
		glRecti(mx - 1, my + 3, mx + 1, my + 7);

		glPopMatrix();
	}

	mainWindow->getWindow()->swapBuffers();
}

//
// update timers
//
void updateTimers ( float &dt )
{
	TimeKeeper prevTime = TimeKeeper::getTick();
	TimeKeeper::setTick();
	dt = TimeKeeper::getTick() - prevTime;

	mainWindow->getWindow()->yieldCurrent();
	mainWindow->getWindow()->yieldCurrent();
}

//
// do dead reckoning on remote players
//
void doDeadReckoning ( void )
{
	for (int i = 0; i < curMaxPlayers; i++)
	{
		if (player[i])
		{
			const bool wasNotResponding = player[i]->isNotResponding();

			player[i]->doDeadReckoning();

			const bool isNotResponding = player[i]->isNotResponding();

			if (!wasNotResponding && isNotResponding)
				addMessage(player[i], "not responding");
			else if (wasNotResponding && !isNotResponding)
				addMessage(player[i], "okay");
		}
	}
}

//
// try to join a game if requested
//
void doNetJoins ( void )
{
	// try to join a game if requested.  do this *before* handling
	// events so we do a redraw after the request is posted and
	// before we actually try to join.
	if (joinGameCallback)
	{
		// if already connected to a game then first sign off
		if (myTank)
			leaveGame();

		// now try connecting
		(*joinGameCallback)(joinGame(), joinGameUserData);

		// don't try again
		joinGameCallback = NULL;
	}
	mainWindow->getWindow()->yieldCurrent();
}

//
// handle events
//

void handleEvents ( void )
{
	// handle events
	clockAdjust = 0.0f;
	while (!CommandsStandard::isQuit() && display->isEventPending())
		doEvent(display);
}

//
// handle input from any joysticks
//
void handleGameDeviceEvents ( void )
{
	if (mainWindow->haveJoystick())
	{
		static const BzfKeyEvent::Button button_map[] = {	BzfKeyEvent::BZ_Button_1,BzfKeyEvent::BZ_Button_2,BzfKeyEvent::BZ_Button_3,BzfKeyEvent::BZ_Button_4,BzfKeyEvent::BZ_Button_5,BzfKeyEvent::BZ_Button_6,BzfKeyEvent::BZ_Button_7,BzfKeyEvent::BZ_Button_8,BzfKeyEvent::BZ_Button_9,BzfKeyEvent::BZ_Button_10};

		static unsigned long old_buttons = 0;
		const int button_count = countof(button_map);
		unsigned long new_buttons = mainWindow->getJoyButtonSet();

		if (old_buttons != new_buttons)
		{
			for (int j = 0; j < button_count; j++)
			{
				if ((old_buttons & (1<<j)) != (new_buttons & (1<<j)))
				{
					BzfKeyEvent ev;
					ev.button = button_map[j];
					ev.ascii = 0;
					ev.shift = 0;
					doKey(ev, (new_buttons & (1<<j)) != 0);
				}
			}
		}
		old_buttons = new_buttons;
	}
	mainWindow->getWindow()->yieldCurrent();
}

//
// check for dead server
//

void checkForDeadServer ( void )
{
	// if server died then leave the game (note that this may cause
	// further server errors but that's okay).
	if (serverError || (serverLink && serverLink->getState() == ServerLink::Hungup))
	{
		// if we haven't reported the death yet then do so now
		if (serverDied || (serverLink && serverLink->getState() == ServerLink::Hungup))
			printError("Server has unexpectedly disconnected");
		leaveGame();
	}
}

//
// update epoch times
//
void updateEpochTime ( float dt )
{
	// update time of day -- update sun and sky every few seconds
	if (!BZDB.isSet("fixedTime") || BZDB.isTrue(StateDatabase::BZDB_SYNCTIME))
		epochOffset += (double)dt;
	if (!world || !BZDB.isTrue(StateDatabase::BZDB_SYNCTIME))
		epochOffset += (double)(50.0f * dt * clockAdjust);
	if (fabs(epochOffset - lastEpochOffset) >= 4.0)
	{
		updateDaylight(epochOffset, *sceneRenderer);
		lastEpochOffset = epochOffset;
	}
}

//
// calculate Roaming Camera
//
void calculateRoamingCamera ( float dt )
{
	// move roaming camera
	if (!roaming)
		return;

	if (myTank)
	{
		bool control = ((shiftKeyStatus & BzfKeyEvent::ControlKey) != 0);
		bool alt     = ((shiftKeyStatus & BzfKeyEvent::AltKey) != 0);
		bool shift   = ((shiftKeyStatus & BzfKeyEvent::ShiftKey) != 0);

		if (display->hasGetKeyMode())
			display->getModState (shift, control, alt);

		roamDPos[0] = 0.0;
		roamDPos[1] = 0.0;
		roamDPos[2] = 0.0;
		roamDTheta  = 0.0;
		roamDTheta  = 0.0;
		roamDPhi    = 0.0;

		if (!control && !shift)
			roamDPos[0] = (float)(4 * myTank->getSpeed()) * BZDB.eval(StateDatabase::BZDB_TANKSPEED);

		if (alt)
			roamDPos[1] = (float)(4 * myTank->getRotation())* BZDB.eval(StateDatabase::BZDB_TANKSPEED);
		else
			roamDTheta =  roamZoom * (float)myTank->getRotation();

		if (control)
			roamDPhi  = -2.0f * roamZoom / 3.0f * (float)myTank->getSpeed();
		if (shift)
			roamDPos[2] = (float)(-4 * myTank->getSpeed())* BZDB.eval(StateDatabase::BZDB_TANKSPEED);
	}

	float c = cosf(roamTheta * M_PI / 180.0f);
	float s = sinf(roamTheta * M_PI / 180.0f);

	roamPos[0] += dt * (c * roamDPos[0] - s * roamDPos[1]);
	roamPos[1] += dt * (c * roamDPos[1] + s * roamDPos[0]);
	roamPos[2] += dt * roamDPos[2];

	float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);

	if (roamPos[2] < muzzleHeight)
		roamPos[2] = muzzleHeight;

	roamTheta  += dt * roamDTheta;
	roamPhi    += dt * roamDPhi;
	roamZoom   += dt * roamDZoom;

	if (roamZoom < BZDB.eval("roamZoomMin"))
		roamZoom = BZDB.eval("roamZoomMin");
	else if (roamZoom > BZDB.eval("roamZoomMax"))
		roamZoom = BZDB.eval("roamZoomMax");

	setRoamingLabel(false);

	// update test video format timer
	if (testVideoFormatTimer > 0.0f)
	{
		testVideoFormatTimer -= dt;

		if (testVideoFormatTimer <= 0.0f)
		{
			testVideoFormatTimer = 0.0f;
			setVideoFormat(testVideoPrevFormat);
		}
	}
}

//
// update pause countdown
//
void updatePauseCountdown ( float dt )
{
	if (!myTank)
		pauseCountdown = 0.0f;

	if (pauseCountdown > 0.0f && !myTank->isAlive())
	{
		pauseCountdown = 0.0f;
		hud->setAlert(1, NULL, 0.0f, true);
	}

	if (pauseCountdown > 0.0f)
	{
		const int oldPauseCountdown = (int)(pauseCountdown + 0.99f);
		pauseCountdown -= dt;
		if (pauseCountdown <= 0.0f)
		{
			/* make sure it is really safe to pause..  since the server
			* might make us drop our flag, make sure the player is on the
			* ground and not in a building.  prevents getting kicked
			* later for being in places we shouldn't without holding the
			* right flags.
			*/
			if (myTank->getLocation() == LocalPlayer::InBuilding)
			{
				// custom message when trying to pause while in a building
				// (could get stuck on un-pause if flag is taken/lost)
				hud->setAlert(1, "Can't pause while inside a building", 1.0f, false);			
			}
			else if (myTank->getLocation() == LocalPlayer::InAir)
			{
				// custom message when trying to pause when jumping/falling
				hud->setAlert(1, "Can't pause when you are in the air", 1.0f, false);
			}
			else if (myTank->getLocation() != LocalPlayer::OnGround && myTank->getLocation() != LocalPlayer::OnBuilding)
			{
				// catch-all message when trying to pause when you should not
				hud->setAlert(1, "Unable to pause right now", 1.0f, false);
			}
			else
			{
				// okay, now we pause.  first drop any team flag we may have.
				const FlagType* flagd = myTank->getFlag();
				if (flagd->flagTeam != NoTeam)
					serverLink->sendDropFlag(myTank->getPosition());

				if (World::getWorld()->allowRabbit() && (myTank->getTeam() == RabbitTeam))
					serverLink->sendNewRabbit();

				// now actually pause
				myTank->setPause(true);
				hud->setAlert(1, NULL, 0.0f, true);
				controlPanel->addMessage("Paused");

				// turn off the sound
				if (savedVolume == -1)
				{
					savedVolume = getSoundVolume();
					setSoundVolume(0);
				}

				// ungrab mouse
				mainWindow->ungrabMouse();
			}
		}
		else if ((int)(pauseCountdown + 0.99f) != oldPauseCountdown && !pausedByUnmap)
		{
			// update countdown alert
			char msgBuf[40];
			sprintf(msgBuf, "Pausing in %d", (int)(pauseCountdown + 0.99f));
			hud->setAlert(1, msgBuf, 1.0f, false);
		}
	}
}

//
// handle tank descruction
//
void handleTankDescruction ( float dt )
{
	// update destruct countdown
	if (!myTank)
		destructCountdown = 0.0f;

	if (destructCountdown > 0.0f && !myTank->isAlive())
	{
		destructCountdown = 0.0f;
		hud->setAlert(1, NULL, 0.0f, true);
	}

	if (destructCountdown > 0.0f)
	{
		const int oldDestructCountdown = (int)(destructCountdown + 0.99f);
		destructCountdown -= dt;

		if (destructCountdown <= 0.0f)
		{	// now actually destruct
			gotBlowedUp( myTank, SelfDestruct, myTank->getId() );
			hud->setAlert(1, NULL, 0.0f, true);
		}
		else if ((int)(destructCountdown + 0.99f) != oldDestructCountdown)
		{
			// update countdown alert
			char msgBuf[40];
			sprintf(msgBuf, "Self Destructing in %d", (int)(destructCountdown + 0.99f));
			hud->setAlert(1, msgBuf, 1.0f, false);
		}
	}
}

//
// handle input notification
//
void handleInputNotification ( void )
{
	// notify if input changed
	if ((myTank != NULL) && (myTank->queryInputChange() == true))
		controlPanel->addMessage(LocalPlayer::getInputMethodName(myTank->getInputMethod()) + " movement");
}

//
// update shots
//
void updateShots ( float dt )
{
	// update other tank's shots
	for (int i = 0; i < curMaxPlayers; i++)
	{
		if (player[i])
			player[i]->updateShots(dt);
	}

	World *world = World::getWorld();	// update servers shots
	if (world)
		world->getWorldWeapons()->updateShots(dt);
}

//
// compute fps
//
void computFPS ( float dt, int &frameCount, float &cumTime )
{
	frameCount++;
	cumTime += float(dt);
	if (cumTime >= 2.0)
	{
		if (showFPS)
			hud->setFPS(float(frameCount) / cumTime);
		cumTime = 0.0;
		frameCount = 0;
	}
}

//
// update world flair
//
void updateWorldFlair ( float dt )
{
	// drift clouds
	sceneRenderer->getBackground()->addCloudDrift(1.0f * dt, 0.731f * dt);
}

//
// set tank camera info
//
void setTankCameraInfo ( float* &myTankPos, float* &myTankDir, float &fov, GLfloat *eyePoint, GLfloat *targetPoint )
{
	// get tank camera info
	if (!myTank)
	{
		myTankPos = (float*)(&defaultPos[0]);
		myTankDir = (float*)(&defaultDir[0]);
		fov = 60.0f;
	}
	else
	{
		myTankPos = (float*)myTank->getPosition();
		myTankDir = (float*)myTank->getForward();

		if (myTank->getFlag() == Flags::WideAngle)
			fov = 120.0f;
		else
			fov = (BZDB.isTrue("displayBinoculars") ? 15.0f : 60.0f);
	}
	fov *= M_PI / 180.0f;

	float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);

	// set projection and view
	eyePoint[0] = myTankPos[0];
	eyePoint[1] = myTankPos[1];
	eyePoint[2] = myTankPos[2] + muzzleHeight;

	targetPoint[0] = eyePoint[0] + myTankDir[0];
	targetPoint[1] = eyePoint[1] + myTankDir[1];
	targetPoint[2] = eyePoint[2] + myTankDir[2];
}

//
// set roaming camera info
//
void setRoamingCameraInfo ( float* &myTankPos, float* &myTankDir, float &fov, GLfloat *eyePoint, GLfloat *targetPoint )
{
	if (!roaming)
		return;

	float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
	float roamViewAngle;

	hud->setAltitude(-1.0f);

#ifdef FOLLOWTANK
	eyePoint[0] = myTankPos[0] - myTankDir[0] * 20;
	eyePoint[1] = myTankPos[1] - myTankDir[1] * 20;
	eyePoint[2] = myTankPos[2] + muzzleHeight * 3;

	targetPoint[0] = eyePoint[0] + myTankDir[0];
	targetPoint[1] = eyePoint[1] + myTankDir[1];
	targetPoint[2] = eyePoint[2] + myTankDir[2];
#else
	setRoamingLabel(false);

	if (player && (roamView != roamViewFree) && player[roamTrackWinner])
	{
		RemotePlayer *target = player[roamTrackWinner];
		const float *targetTankDir = target->getForward();

		if (roamView == roamViewTrack)					// fixed camera tracking target
		{
			eyePoint[0] = roamPos[0];
			eyePoint[1] = roamPos[1];
			eyePoint[2] = roamPos[2];

			targetPoint[0] = target->getPosition()[0];
			targetPoint[1] = target->getPosition()[1];
			targetPoint[2] = target->getPosition()[2];
		}
		else if (roamView == roamViewFollow)					// camera following target
		{
			eyePoint[0] = target->getPosition()[0] - targetTankDir[0] * 40;
			eyePoint[1] = target->getPosition()[1] - targetTankDir[1] * 40;
			eyePoint[2] = target->getPosition()[2] + muzzleHeight * 6;

			targetPoint[0] = target->getPosition()[0];
			targetPoint[1] = target->getPosition()[1];
			targetPoint[2] = target->getPosition()[2];
		}
		else if (roamView == roamViewFP)	  // target's view
		{
			eyePoint[0] = target->getPosition()[0];
			eyePoint[1] = target->getPosition()[1];
			eyePoint[2] = target->getPosition()[2] + muzzleHeight;

			targetPoint[0] = eyePoint[0] + targetTankDir[0];
			targetPoint[1] = eyePoint[1] + targetTankDir[1];
			targetPoint[2] = eyePoint[2] + targetTankDir[2];

			hud->setAltitude(target->getPosition()[2]);
		}
		else if (roamView == roamViewFlag)	  // track team flag
		{
			Flag &targetFlag = world->getFlag(roamTrackFlag);
			eyePoint[0] = roamPos[0];
			eyePoint[1] = roamPos[1];
			eyePoint[2] = roamPos[2];

			targetPoint[0] = targetFlag.position[0];
			targetPoint[1] = targetFlag.position[1];
			targetPoint[2] = targetFlag.position[2];
		}
		roamViewAngle = (float) (atan2(targetPoint[1]-eyePoint[1],targetPoint[0]-eyePoint[0]) * 180.0f / M_PI);
	}
	else	// free Roaming
	{
		float dir[3];

		dir[0] = cosf(roamPhi * M_PI / 180.0f) * cosf(roamTheta * M_PI / 180.0f);
		dir[1] = cosf(roamPhi * M_PI / 180.0f) * sinf(roamTheta * M_PI / 180.0f);
		dir[2] = sinf(roamPhi * M_PI / 180.0f);

		eyePoint[0] = roamPos[0];
		eyePoint[1] = roamPos[1];
		eyePoint[2] = roamPos[2];

		targetPoint[0] = eyePoint[0] + dir[0];
		targetPoint[1] = eyePoint[1] + dir[1];
		targetPoint[2] = eyePoint[2] + dir[2];

		roamViewAngle = roamTheta;
	}
	float virtPos[]={eyePoint[0], eyePoint[1], 0};
	if (myTank)
		myTank->move(virtPos, roamViewAngle * M_PI / 180.0f);
#endif
	fov = roamZoom * M_PI / 180.0f;
		moveSoundReceiver(eyePoint[0], eyePoint[1], eyePoint[2], 0.0, false);
}

//
// set view frustum
//
void setViewFrustum ( float fov, GLfloat *eyePoint, GLfloat *targetPoint )
{
	bool useManagers = BZDB.isTrue("useNewRendering");
	float worldSize = BZDB.eval(StateDatabase::BZDB_WORLDSIZE);

	if (useManagers)
	{
		// set some kinda camera object
		View3D::instance().setViewDist(1.1f, 1.5f * worldSize);	// this will ONLY do stuff when it changes, but this is WAY LAME.
		View3D::instance().set(eyePoint,targetPoint,fov);
	}
	else
	{	

		sceneRenderer->getViewFrustum().setProjection(fov,1.1f, 1.5f * worldSize,mainWindow->getWidth(),mainWindow->getHeight(),mainWindow->getViewHeight());
		sceneRenderer->getViewFrustum().setView(eyePoint, targetPoint);
	}
}

//
// add dynamic nodes
//
void addDynamicSceneNodes ( float dt )
{
	SceneDatabase* scene = sceneRenderer->getSceneDatabase();

	if (scene && myTank)
	{
		myTank->addToScene(scene, myTank->getTeam(), false);				// add my tank
		if (myTank->getFlag() == Flags::Cloaking)
			myTank->setInvisible();	  // and make it invisible
		else if (roaming)
			myTank->setHidden(false);
		else
			myTank->setHidden();	  // or make it hidden

		myTank->addShots(scene, false);				// add my shells

		if (world)	//add server shells
			world->getWorldWeapons()->addShots(scene, false);

		myTank->addAntidote(scene);	// add antidote flag

		if (world)
			world->addFlags(scene);	// add flags

		TeamColor overrideTeam = RogueTeam;
		const bool colorblind = (myTank->getFlag() == Flags::Colorblindness);

		// add other tanks and shells
		for (int i = 0; i < curMaxPlayers; i++)
		{
			if (player[i])
			{
				player[i]->updateSparks(dt);
				player[i]->addShots(scene, colorblind);
				overrideTeam = RogueTeam;
				if (!colorblind)
				{
					if ((player[i]->getFlag() == Flags::Masquerade) && (myTank->getFlag() != Flags::Seer) && (myTank->getTeam() != ObserverTeam))
						overrideTeam = myTank->getTeam();
					else
						overrideTeam = player[i]->getTeam();
				}
				player[i]->addToScene(scene, overrideTeam, true);
				if ((player[i]->getFlag() == Flags::Cloaking) && (myTank->getFlag() != Flags::Seer))
					player[i]->setInvisible();
				else
					player[i]->setHidden(roaming && roamView == roamViewFP && roamTrackWinner == i);
			}
		}
		// add explosions
		addExplosions(scene);

		// add water-like graphics for the deadUnder line
		addDeadUnder (scene, dt);

		// if i'm inside a building then add eighth dimension scene node.
		if (myTank->getContainingBuilding())
		{
			SceneNode* node = world->getInsideSceneNode(myTank->getContainingBuilding());
			if (node)
				scene->addDynamicNode(node);
		}
	}
}

//
// set visual impearments
//
void setVisualImpearments ( void )
{
	bool useManagers = BZDB.isTrue("useNewRendering");

	if (!useManagers)
	{
		// turn blanking and inversion on/off as appropriate
		sceneRenderer->setBlank(myTank && (myTank->isPaused() || myTank->getFlag() == Flags::Blindness));
		sceneRenderer->setInvert(myTank && myTank->getFlag() == Flags::PhantomZone &&  myTank->isFlagActive());

		// turn on scene dimming when showing menu or when
		// we're dead and no longer exploding.
		sceneRenderer->setDim(HUDDialogStack::get()->isActive() || (myTank && !roaming && !myTank->isAlive() && !myTank->isExploding()));
	}
}

//
// set HUD state
//
void setHUDState ( void )
{
	hud->setDim(HUDDialogStack::get()->isActive());
	hud->setPlaying(myTank && (myTank->isAlive() && !myTank->isPaused()));
	hud->setRoaming(roaming);
	hud->setCracks(myTank && !firstLife && !myTank->isAlive());
}

//
// get frame start time
//
void setFrameStartTime ( void )
{
	if (!showDrawTime)
		return;

	// get media object
	BzfMedia* media = PlatformFactory::getMedia();

#if defined(DEBUG_RENDERING)
		// get an accurate measure of frame time (at expense of frame rate)
		glFinish();
#endif
		media->stopwatch(true);
}

//
// remove dynamic nodes from this frame
//
void removeDynamicSceneNodes ( void )
{
	SceneDatabase* scene = sceneRenderer->getSceneDatabase();

	if (scene)
		scene->removeDynamicNodes();
}

//
// do local tank motion
//
void doLocalTankMotion ( float dt )
{
	// do motion
	if (!myTank)
		return;

	if (myTank->isAlive() && !myTank->isPaused())
	{
		doMotion();
		if (hud->getHunting())
			setHuntTarget(); //spot hunt target
		if (myTank->getTeam() != ObserverTeam && ((fireButton && myTank->getFlag() == Flags::MachineGun) || (myTank->getFlag() == Flags::TriggerHappy)))
			myTank->fireShot();
	}
	else
	{
		int mx, my;
		mainWindow->getMousePosition(mx, my);
	}
	myTank->update();

#ifdef ROBOT
	updateRobots(dt);
#endif
}

//
// prep HUD
//
void prepHUD ( void )
{
	// prep the HUD
	if (!myTank)
		return;

	const float* myPos = myTank->getPosition();

	hud->setHeading(myTank->getAngle());
	hud->setAltitude(myPos[2]);

	if (world->allowTeamFlags())
	{
		const float* myTeamColor = Team::getTankColor(myTank->getTeam());

		for (int i = 0; i < numFlags; i++)				// markers for my team flag
		{
			Flag& flag = world->getFlag(i);

			if ((flag.type->flagTeam == myTank->getTeam()) && (flag.status != FlagOnTank || flag.owner != myTank->getId()))
			{
				const float* flagPos = flag.position;
				float heading = atan2f(flagPos[1] - myPos[1],flagPos[0] - myPos[0]);

				hud->addMarker(heading, myTeamColor);
			}
		}
	}

	if (myTank->getAntidoteLocation())
	{
		// marker for my antidote flag
		const GLfloat* antidotePos = myTank->getAntidoteLocation();
		float heading = atan2f(antidotePos[1] - myPos[1],antidotePos[0] - myPos[0]);
		const float antidoteColor[] = {1.0f, 1.0f, 0.0f};

		hud->addMarker(heading, antidoteColor);
	}
}

//
// send local updates
//
void sendLocalUpdates ( void )
{
	// send my data
	if (myTank && myTank->isDeadReckoningWrong() && myTank->getTeam() != ObserverTeam)
	{
		serverLink->sendPlayerUpdate(myTank);
	}

#ifdef ROBOT
	sendRobotUpdates();
#endif
}

//
// restore the sound.  if we don't do this then we'll save the
// wrong volume when we dump out the configuration file if the
// app exits when the game is paused.
//
void restoreSound ( void )
{
	if (savedVolume != -1)
	{
		setSoundVolume(savedVolume);
		savedVolume = -1;
	}
}

//
// main playing loop
//
static void		playingLoop()
{
  float* myTankPos;
  float* myTankDir;

  GLfloat eyePoint[3];
  GLfloat targetPoint[3];

  GLfloat fov;

  // get media object
  BzfMedia* media = PlatformFactory::getMedia();

  // start timing
  int frameCount = 0;
  float cumTime = 0.0f;
	float dt = 0;

  TimeKeeper::setTick();
  updateDaylight(epochOffset, *sceneRenderer);

  // main loop
  while (!CommandsStandard::isQuit())
	{
		updateTimers(dt);

    // handle incoming packets
    doMessages();
    mainWindow->getWindow()->yieldCurrent();

		doDeadReckoning();
		doNetJoins();
		handleEvents();
		handleGameDeviceEvents();
    callPlayingCallbacks(); 

    if (CommandsStandard::isQuit())    // quick out
			break;

		checkForDeadServer();

		updateEpochTime(dt);
		calculateRoamingCamera(dt);
		updatePauseCountdown(dt);

		handleTankDescruction(dt);
		handleInputNotification();

    updateFlags(dt);									// reposition flags
    updateExplosions(dt);							// update explosion animations
		updateShots(dt);

    // stuff to draw a frame
    if (!unmapped)
		{
      computFPS(dt,frameCount,cumTime);
			updateWorldFlair(dt);

			setTankCameraInfo(myTankPos,myTankDir,fov, eyePoint,targetPoint);
      setRoamingCameraInfo(myTankPos,myTankDir,fov, eyePoint,targetPoint);
			setViewFrustum(fov, eyePoint, targetPoint );

			addDynamicSceneNodes(dt);
			setVisualImpearments();
			setHUDState();

			setFrameStartTime();

			drawScene(fov);
			
			removeDynamicSceneNodes();
    }
    else
      media->sleep(0.05f);      // wait around a little to avoid spinning the CPU when iconified

    updateSound();
    doLocalTankMotion(dt);
    prepHUD();
    checkEnvironment();    // check for flags and hits
		sendLocalUpdates();
  }

	restoreSound();
  // hide window
  mainWindow->showWindow(false);
}

//
// game initialization
//

static float		timeConfiguration(bool useZBuffer)
{
  // prepare depth buffer if requested
  BZDB.set("zbuffer", useZBuffer ? "1" : "0");
  if (useZBuffer)
	{
    glEnable(GL_DEPTH_TEST);
   // glClear(GL_DEPTH_BUFFER_BIT);
  }

  // use glFinish() to get accurate timings
  glFinish();
  TimeKeeper startTime = TimeKeeper::getCurrent();
  sceneRenderer->setExposed();
  sceneRenderer->render();
  glFinish();
  TimeKeeper endTime = TimeKeeper::getCurrent();

  // turn off depth buffer
  if (useZBuffer)
		glDisable(GL_DEPTH_TEST);

  return endTime - startTime;
}

static void		timeConfigurations()
{
}

static void		findFastConfiguration()
{
  // time the rendering of the background with various rendering styles
  // until we find one fast enough.  these tests assume that we're
  // going to be fill limited.  each test comes in a pair:  with and
  // without the zbuffer.
  //
  // this, of course, is only a rough estimate since we're not drawing
  // a normal frame (no radar, no HUD, no buildings, etc.).  the user
  // can always turn stuff on later and the settings are remembered
  // across invokations.

  // setup projection
  float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
  static const GLfloat eyePoint[3] = { 0.0f, 0.0f, muzzleHeight };
  static const GLfloat targetPoint[3] = { 0.0f, 10.0f, muzzleHeight };
  float worldSize = BZDB.eval(StateDatabase::BZDB_WORLDSIZE);
  sceneRenderer->getViewFrustum().setProjection(45.0f * M_PI / 180.0f,
						1.1f, 1.5f * worldSize,
						mainWindow->getWidth(),
						mainWindow->getHeight(),
						mainWindow->getViewHeight());
  sceneRenderer->getViewFrustum().setView(eyePoint, targetPoint);

  // add a big wall in front of where we're looking.  this is important
  // because once textures are off, the background won't draw much of
  // anything.  this will ensure that we continue to test polygon fill
  // rate.  with one polygon it doesn't matter if we use a z or bsp
  // database.
  static const GLfloat base[3]  = { -10.0f, 10.0f,  0.0f };
  static const GLfloat sEdge[3] = {  20.0f,  0.0f,  0.0f };
  static const GLfloat tEdge[3] = {   0.0f,  0.0f, 10.0f };
  static const GLfloat color[4] = { 1.0f, 1.0f, 1.0f, 0.5f };
  SceneDatabase* timingScene = new ZSceneDatabase;
  WallSceneNode* node = new QuadWallSceneNode(base,
					      sEdge, tEdge, 1.0f, 1.0f, true);
  node->setColor(color);
  node->setModulateColor(color);
  node->setLightedColor(color);
  node->setLightedModulateColor(color);
  node->setTexture(HUDuiControl::getArrow());
  node->setMaterial(OpenGLMaterial(color, color));
  timingScene->addStaticNode(node);
  sceneRenderer->setSceneDatabase(timingScene);
  sceneRenderer->setDim(false);

  timeConfigurations();

  sceneRenderer->setSceneDatabase(NULL);
  delete timingScene;
}

static void		defaultErrorCallback(const char* msg)
{
  controlPanel->addMessage(msg);
}

static void		startupErrorCallback(const char* msg)
{
  controlPanel->addMessage(msg);
  //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  //glClear(GL_COLOR_BUFFER_BIT);
  //controlPanel->render(*sceneRenderer);
  //mainWindow->getWindow()->swapBuffers();
}

void registerCommands ( void )
{
	// register some commands
	for (unsigned int c = 0; c < countof(commandList); ++c)
		CMDMGR.add(commandList[c].name, commandList[c].func, commandList[c].help);
}

void initControlPanel ( void )
{
	// make control panel
	ControlPanel _controlPanel(*mainWindow, *sceneRenderer);
	controlPanel = &_controlPanel;

	// tell the control panel how many frame buffers there are.  we
	// cheat when drawing the control panel, not drawing it if it
	// hasn't changed.  that only works if we've filled all the
	// frame buffers (e.g. front and back buffers) with the correct
	// data.
	// FIXME -- assuming the contents of any frame buffer except the
	// front buffer are anything but garbage violates the OpenGL
	// spec.  we really should redraw the control panel every frame
	// but this works on every system so far.
	{
		int n = 2;	// assume triple buffering
		controlPanel->setNumberOfFrameBuffers(n);
	}
}

void setDefaultConfig ( StartupInfo* _info )
{
	// if no configuration, set some sane options. If they have old slow hardware, they can change it.
	// evolve or die
	if (!_info->hasConfiguration)
	{
		BZDB.set("lighting", "1");
		BZDB.set("texture", "1");
		sceneRenderer->setQuality(4);
		BZDB.set("shadows", "1");
		BZDB.set("enhancedradar", "1");
	}
}

void checkGrabMouse ( void )
{
	// should we grab the mouse?  yes if fullscreen.
	if (!BZDB.isSet("_window"))
		setGrabMouse(true);
	else
	{
#if defined(__linux__) && !defined(DEBUG)
		setGrabMouse(true);	// linux usually has a virtual root window so grab mouse always
#endif
	}
}

void initGraphDisplay ( void )
{
	// show window and clear it immediately
	mainWindow->showWindow(true);
	// glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// glDisable(GL_SCISSOR_TEST);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	mainWindow->getWindow()->swapBuffers();

	// resize and draw basic stuff
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	// glEnable(GL_SCISSOR_TEST);
	//controlPanel->resize();
	// sceneRenderer->render();
	// controlPanel->render(*sceneRenderer);
	// mainWindow->getWindow()->swapBuffers();
}

void initEpohTimer ( void )
{
	// initialize epoch offset (time)
	userTimeEpochOffset = (double)mktime(&userTime);
	epochOffset = userTimeEpochOffset;
	updateDaylight(epochOffset, *sceneRenderer);
	lastEpochOffset = epochOffset;
}

void initSignals ( void )
{
	// catch kill signals before changing video mode so we can
	// put it back even if we die.  ignore a few signals.
	bzSignal(SIGILL, SIG_PF(dying));
	bzSignal(SIGABRT, SIG_PF(dying));
	bzSignal(SIGSEGV, SIG_PF(dying));
	bzSignal(SIGTERM, SIG_PF(suicide));
#if !defined(_WIN32)
	if (bzSignal(SIGINT, SIG_IGN) != SIG_IGN)
		bzSignal(SIGINT, SIG_PF(suicide));
	bzSignal(SIGPIPE, SIG_PF(hangup));
	bzSignal(SIGHUP, SIG_IGN);
	if (bzSignal(SIGQUIT, SIG_IGN) != SIG_IGN)
		bzSignal(SIGQUIT, SIG_PF(dying));
#ifndef GUSI_20
	bzSignal(SIGBUS, SIG_PF(dying));
#endif
	bzSignal(SIGUSR1, SIG_IGN);
	bzSignal(SIGUSR2, SIG_IGN);
#endif /* !defined(_WIN32) */
}

void setVideoRes ( void )
{
	std::string videoFormat;
	int format = -1;
	if (BZDB.isSet("resolution"))
	{
		videoFormat = BZDB.get("resolution");
		if (videoFormat.length() != 0)
		{
			format = display->findResolution(videoFormat.c_str());
			if (format >= 0)
				mainWindow->getWindow()->callResizeCallbacks();
		}
	}

	// set the resolution (only if in full screen mode)
	if (!BZDB.isSet("_window") && BZDB.isSet("resolution"))
	{
		if (videoFormat.length() != 0)
		{
			if (display->isValidResolution(format) &&display->getResolution() != format && display->setResolution(format))
			{
				if (BZDB.isSet("geometry"))				// handle resize
				{
					int w, h, x, y, count;
					char xs, ys;

					count = sscanf(BZDB.get("geometry").c_str(),"%dx%d%c%d%c%d", &w, &h, &xs, &x, &ys, &y);

					if (w < 640)
						w = 640;

					if (h < 480)
						h = 480;

					if (count == 6)
					{
						if (xs == '-')
							x = display->getWidth() - x - w;

						if (ys == '-')
							y = display->getHeight() - y - h;

						mainWindow->setPosition(x, y);
					}
					mainWindow->setSize(w, h);
				}
				else
					mainWindow->setFullscreen();

				// more resize handling
				mainWindow->getWindow()->callResizeCallbacks();
				mainWindow->warpMouse();
			}
		}
	}
}

void grabMouse ( void )
{
	// grab mouse if we should
	if (shouldGrabMouse())
		mainWindow->grabMouse();
}

void initPanels ( void )
{
	// initialize control panel and hud
	updateNumPlayers();
	updateFlag(Flags::Null);
	updateHighScores();
	notifyBzfKeyMapChanged();
}

void loadExplosions ( void )
{
	TextureManager &tm = TextureManager::instance();

	bool done = false;
	int explostion = 1;
	while (!done)
	{
		char text[256];
		sprintf(text, "explode%d", explostion);

		int tex = tm.getTextureID(text, false);

		if (tex < 0)
			done = true;
		else
		{
			// make explosion scene node
			float	zero[3] = {0,0,0};
			BillboardSceneNode* explosion = new BillboardSceneNode(zero);
			explosion->setTexture(tex);
			explosion->setTextureAnimation(8, 8);
			explosion->setLight();
			explosion->setLightColor(1.0f, 0.8f, 0.5f);
			explosion->setLightAttenuation(0.04f, 0.0f, 0.01f);

			// add it to list of prototype explosions
			prototypeExplosions.push_back(explosion);
			explostion++;
		}
	}
}

void initWorld ( void )
{
	// let other stuff do initialization
	sceneBuilder = new SceneDatabaseBuilder(sceneRenderer);
	World::init();
}

void printStartupMessages ( void )
{
	std::string tmpString;

	// print version
	char bombMessage[80];
	sprintf(bombMessage, "OpenCombat version %s", getAppVersion());
	controlPanel->addMessage("");
	tmpString = ColorStrings[RedColor];
	tmpString += (const char *) bombMessage;
	controlPanel->addMessage(tmpString);

	// print modification
	tmpString = ColorStrings[RedColor];
	tmpString += "based on bzflag c2004 Tim Riker http://bzflag.org";
	controlPanel->addMessage(tmpString);

	// print copyright
	tmpString = ColorStrings[RogueColor];
	tmpString += copyright;
	controlPanel->addMessage(tmpString);

	// print authors
	tmpString = ColorStrings[GreenColor];
	tmpString += "Project Lead: Jeff Myers <jeff@OpenCombat.net>";
	controlPanel->addMessage(tmpString);

	// print website
	tmpString = ColorStrings[BlueColor];
	tmpString += "HTTP://www.OpenCombat.net";
	controlPanel->addMessage(tmpString);

	// print GL renderer
	tmpString = ColorStrings[PurpleColor];
	tmpString += (const char*)glGetString(GL_RENDERER);
	controlPanel->addMessage(tmpString);
}

void printSilenceWarnings ( void )
{
	//inform user of silencePlayers on startup
	for (unsigned int j = 0; j < silencePlayers.size(); j ++)
	{
		std::string aString = silencePlayers[j];
		aString += " Silenced";
		if (silencePlayers[j] == "*")
			aString = "Silenced All Msgs";
		controlPanel->addMessage(aString);
	}
}

void setStartupAction ( void )
{
	// enter game if we have all the info we need, otherwise
	// pop up main menu
	if (startupInfo.autoConnect && startupInfo.callsign[0] && startupInfo.serverName[0])
	{
		joinGameCallback = &joinGameHandler;
		controlPanel->addMessage("Trying...");
	}
	else
		HUDDialogStack::get()->push(mainMenu);
}

void cleanUpExplostions ( void )
{
	for (unsigned int ext = 0; ext < prototypeExplosions.size(); ext++)
		delete prototypeExplosions[ext];// clean up

	prototypeExplosions.clear();
}

void cleanUpGlobals ( void )
{
	setErrorCallback(NULL);

	while (HUDDialogStack::get()->isActive())
		HUDDialogStack::get()->pop();

	delete mainMenu;
	delete sceneBuilder;

	sceneRenderer->setBackground(NULL);
	sceneRenderer->setSceneDatabase(NULL);

	delete zScene;
	delete bspScene;
	World::done();

	bspScene = NULL;
	zScene = NULL;
	mainWindow = NULL;
	sceneRenderer = NULL;
	display = NULL;
}

void startPlaying( BzfDisplay* _display, SceneRenderer& renderer, StartupInfo* _info )
{
  // initalization
  display = _display;
  sceneRenderer = &renderer;
  mainWindow = &sceneRenderer->getWindow();

  registerCommands();
	initControlPanel();
	setDefaultConfig(_info);
	checkGrabMouse();
	
	initGraphDisplay();
  setErrorCallback(startupErrorCallback);  // startup error callback adds message to control panel and forces an immediate redraw.

	initEpohTimer();
	initSignals();
	setVideoRes();
	grabMouse();

	// init hud
	// TODO, Move this into a singleton
	HUDRenderer _hud(display, renderer);

	hud = &_hud;
	initPanels();

  // make background renderer
	// TODO: move this into the managers and make a bg object
  BackgroundRenderer background(renderer);
  sceneRenderer->setBackground(&background);

	loadExplosions();
	initWorld();

  mainMenu = new MainMenu;  // prepare dialogs
  startupInfo = *_info;  // initialize startup info with stuff provided from command line

  setErrorCallback(defaultErrorCallback);  // normal error callback (doesn't force a redraw)

	printStartupMessages();
	printSilenceWarnings();
  
	setStartupAction();
 
  playingLoop();  // start game loop

	// clean up your mess
	cleanUpExplostions();

  *_info = startupInfo;

  leaveGame();

	cleanUpGlobals();
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
