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

#include <math.h>
#include "common.h"
#include "TracerSceneNode.h"
#include "ShellSceneNode.h"
#include "ViewFrustum.h"
#include "SceneRenderer.h"
#include "StateDatabase.h"
#include "BZDBCache.h"

#define	ShellRadius1_2	(M_SQRT1_2 * ShellRadius)

const GLfloat		TracerSceneNode::TailLength = 10.0f;
const GLfloat		TracerSceneNode::tailVertex[9][3] = {
				{-0.5f * TailLength, 0.0f, 0.0f },
				{ 0.5f * TailLength, -ShellRadius, 0.0f },
				{ 0.5f * TailLength, -ShellRadius1_2, -ShellRadius1_2 },
				{ 0.5f * TailLength, 0.0f, -ShellRadius },
				{ 0.5f * TailLength, ShellRadius1_2, -ShellRadius1_2 },
				{ 0.5f * TailLength, ShellRadius, 0.0f },
				{ 0.5f * TailLength, ShellRadius1_2, ShellRadius1_2 },
				{ 0.5f * TailLength, 0.0f, ShellRadius },
				{ 0.5f * TailLength, -ShellRadius1_2, ShellRadius1_2 }
			};

TracerSceneNode::TracerSceneNode(const GLfloat pos[3],
				const GLfloat forward[3]) :
				renderNode(this)
{
  // prepare light
  light.setAttenuation(0, 0.0667f);
  light.setAttenuation(1, 0.0f);
  light.setAttenuation(2, 0.0667f);
  light.setColor(2.0f, 2.0f, 0.0f);

  // prepare geometry
  move(pos, forward);
  setRadius(0.25f * TailLength * TailLength);
}

TracerSceneNode::~TracerSceneNode()
{
  // do nothing
}

void			TracerSceneNode::move(const GLfloat pos[3],
						const GLfloat forward[3])
{
  const GLfloat d = 1.0f / sqrtf(forward[0] * forward[0] +
				forward[1] * forward[1] +
				forward[2] * forward[2]);
  azimuth = 180.0f / M_PI * atan2f(forward[1], forward[0]);
  elevation = -180.0f / M_PI * atan2f(forward[2], hypotf(forward[0],forward[1]));
  setCenter(pos[0] - 0.5f * TailLength * d * forward[0],
	    pos[1] - 0.5f * TailLength * d * forward[1],
	    pos[2] - 0.5f * TailLength * d * forward[2]);
  light.setPosition(getSphere());
}

void			TracerSceneNode::addLight(SceneRenderer& renderer)
{
  renderer.addLight(light);
}

void			TracerSceneNode::notifyStyleChange(
				const SceneRenderer&)
{
  OpenGLGStateBuilder builder(gstate);
  gstate = builder.getState();
}

void			TracerSceneNode::addRenderNodes(
				SceneRenderer& renderer)
{
  renderer.addRenderNode(&renderNode, &gstate);
}

//
// TracerSceneNode::TracerRenderNode
//

TracerSceneNode::TracerRenderNode::TracerRenderNode(
				const TracerSceneNode* _sceneNode) :
				sceneNode(_sceneNode)
{
  // do nothing
}

TracerSceneNode::TracerRenderNode::~TracerRenderNode()
{
  // do nothing
}

void			TracerSceneNode::TracerRenderNode::render()
{
  const GLfloat* sphere = sceneNode->getSphere();
  glPushMatrix();
    glTranslatef(sphere[0], sphere[1], sphere[2]);
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);

    glBegin(GL_TRIANGLE_FAN);
      myColor4f(1.0f, 1.0f, 0.0f, 0.0f);
      glVertex3fv(tailVertex[0]);
      myColor4f(1.0f, 1.0f, 0.0f, 0.7f);
      glVertex3fv(tailVertex[8]);
      glVertex3fv(tailVertex[7]);
      glVertex3fv(tailVertex[6]);
      glVertex3fv(tailVertex[5]);
      glVertex3fv(tailVertex[4]);
      glVertex3fv(tailVertex[3]);
      glVertex3fv(tailVertex[2]);
      glVertex3fv(tailVertex[1]);
      glVertex3fv(tailVertex[8]);
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
      glVertex3fv(tailVertex[8]);
      glVertex3fv(tailVertex[7]);
      glVertex3fv(tailVertex[6]);
      glVertex3fv(tailVertex[5]);
      glVertex3fv(tailVertex[4]);
      glVertex3fv(tailVertex[3]);
      glVertex3fv(tailVertex[2]);
      glVertex3fv(tailVertex[1]);
    glEnd();

  glPopMatrix();
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

