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

#ifndef	__JOINMENU_H__
#define	__JOINMENU_H__

#include "common.h"

/* system interface headers */
#include <vector>
#include <string>

/* common interface headers */
#include "ErrorHandler.h"
#include "Team.h"

/* local interface headers */
#include "HUDDialog.h"

class ServerStartMenu;
class ServerMenu;
class HUDuiTypeIn;
class HUDuiList;
class HUDuiLabel;

class JoinMenu : public HUDDialog {
  public:
			JoinMenu();
			~JoinMenu();

    HUDuiDefaultKey*	getDefaultKey();

    void		show();
    void		execute();
    void		dismiss();
    void		resize(int width, int height);

  private:
    static void		teamCallback(HUDuiControl*, void*);
    static void		joinGameCallback(bool, void*);
    static void		joinErrorCallback(const char* msg);
    TeamColor		getTeam() const;
    void		setTeam(TeamColor);
    void		setStatus(const char*, const std::vector<std::string> *parms = NULL);
    void		loadInfo();

  private:
    float		center;
    HUDuiTypeIn*	callsign;
    HUDuiTypeIn*	email;
    HUDuiList*		team;
    HUDuiTypeIn*	server;
    HUDuiTypeIn*	port;
    HUDuiLabel*		status;
    HUDuiLabel*		startServer;
    HUDuiLabel*		findServer;
    HUDuiLabel*		connectLabel;
    HUDuiLabel*		failedMessage;
    ErrorCallback	oldErrorCallback;
    ServerStartMenu*	serverStartMenu;
    ServerMenu*		serverMenu;
    static JoinMenu*	activeMenu;
};


#endif /* __JOINMENU_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
