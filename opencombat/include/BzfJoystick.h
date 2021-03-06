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

/* BzfJoystick:
 *	Abstract, platform independent base for Joysticks.
 */

#ifndef BZF_JOYSTICK_H
#define	BZF_JOYSTICK_H

#if defined(_MSC_VER)
  #pragma warning(disable: 4786)
#endif

#include "common.h"
#include <vector>

class BzfJoystick {
  public:
			BzfJoystick();
    virtual		~BzfJoystick();

    virtual void	initJoystick(const char* joystickName);
    virtual bool	joystick() const;
    virtual void	getJoy(int& x, int& y) const;
    virtual unsigned long getJoyButtons() const;
    virtual void        getJoyDevices(std::vector<std::string> &list) const;
};

#endif // BZF_JOYSTICK_H

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

