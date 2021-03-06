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

/* OpenGLGState:
 *	Encapsulates OpenGL rendering state information.
 */

#ifndef	BZF_OPENGL_GSTATE_H
#define	BZF_OPENGL_GSTATE_H

#include "common.h"
#include "bzfgl.h"

class OpenGLMaterial;
class OpenGLGStateRep;
class OpenGLGStateState;
class RenderNode;

typedef void		(*OpenGLContextInitializer)(void* userData);

class OpenGLGState {
  friend class OpenGLGStateBuilder;
  public:
			OpenGLGState();
			OpenGLGState(const OpenGLGState&);
			OpenGLGState(const OpenGLGStateState&);
			~OpenGLGState();
    OpenGLGState&	operator=(const OpenGLGState& state);
    void		setState() const;
    bool		isTextured() const;
    bool		isTextureReplace() const;
    bool		isLighted() const;
    void		addRenderNode(RenderNode* node) const;
    static void		resetState();
    static void		clearLists();
    static void		renderLists();
    static void		init();

    // these are in OpenGLGState for lack of a better place.  register...
    // is for clients to add a function to call when the OpenGL context
    // has been destroyed and must be recreated.  the function is called
    // by initContext() and initContext() will call all initializers in
    // the order they were registered, plus reset the OpenGLGState state.
    //
    // destroying and recreating the OpenGL context is only necessary on
    // platforms that cannot abstract the graphics system sufficiently.
    // for example, on win32, changing the display bit depth will cause
    // most OpenGL drivers to crash unless we destroy the context before
    // the switch and recreate it afterwards.
    static void		registerContextInitializer(
				OpenGLContextInitializer,
				void* userData = NULL);
    static void		unregisterContextInitializer(
				OpenGLContextInitializer,
				void* userData = NULL);
    static void		initContext();

  private:
    static void		initGLState();

    struct ContextInitializer {
      public:
	ContextInitializer(OpenGLContextInitializer, void*);
	~ContextInitializer();
	static void		   execute();
	static ContextInitializer* find(OpenGLContextInitializer, void*);

      public:
	OpenGLContextInitializer callback;
	void*			userData;
	ContextInitializer*	prev;
	ContextInitializer*	next;
	static ContextInitializer* head;
	static ContextInitializer* tail;
    };

  private:
    OpenGLGStateRep*	rep;
};

class OpenGLGStateBuilder {
  public:
			OpenGLGStateBuilder();
			OpenGLGStateBuilder(const OpenGLGState&);
			~OpenGLGStateBuilder();
    OpenGLGStateBuilder &operator=(const OpenGLGState&);

    void		reset();
    void		enableTexture(bool = true);
    void		enableTextureReplace(bool = true);
    void		enableMaterial(bool = true);
    void		resetAlphaFunc();
    void		setTexture(const int texture);
    void		setMaterial(const OpenGLMaterial& material);
    void		setCulling(GLenum culling);
    void		setAlphaFunc(GLenum func = GL_GEQUAL,
				     GLclampf ref = 0.1f);
    OpenGLGState	getState() const;

  private:
    void		init(const OpenGLGState&);

  private:
    OpenGLGStateState*	state;
};

#endif // BZF_OPENGL_GSTATE_H

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

