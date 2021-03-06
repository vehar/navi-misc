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

#ifndef	GLCOLLECT_H
#define	GLCOLLECT_H

#include "bzfgl.h"

class GLCollect
{
public:
  GLCollect( GLenum en );
  ~GLCollect();
private:
  GLCollect( const GLCollect &c );
  GLCollect& operator=( const GLCollect &c );

  static int count;
};

#endif
