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

/* Obstacle:
 *	Interface for all obstacles in the game environment,
 *	including boxes, pyramids, and teleporters.
 *
 * isInside(const float*, float) is a rough test that considers
 *	the tank as a circle
 * isInside(const float*, float, float, float) is a careful test
 *	that considers the tank as a rectangle
 */

#ifndef	BZF_OBSTACLE_H
#define	BZF_OBSTACLE_H

#include "common.h"
#include "Ray.h"
#include <string>

class ObstacleSceneNodeGenerator;
class WallSceneNode;

/** This ABC represents a (normally) solid object in a world. It has pure 
    virtual functions for getting information about it's size, checking ray
    intersections, checking point intersections, computing normals etc.
    All these functions have to be implemented in concrete subclasses.
*/
class Obstacle {
  public:
  /** The default constructor. It sets all values to 0
      and is not very useful. */
  Obstacle();
  
  /** This function initializes the Obstacle with the given parameters.
      @param pos         The position of the obstacle in world coordinates
      @param rotation    The rotation around the obstacle's Z axis
      @param hwidth      Half the X size of the obstacle
      @param hbreadth    Half the Y size of the obstacle
      @param height      The Z size of the obstacle
      @param drive       @c true if the obstacle is drivethtrough, i.e. tanks
                         can pass through it
      @param shoot       @c true if the obstacle is shootthrough, i.e. bullets
                         can pass through it
  */
  Obstacle(const float* pos, float rotation, float hwidth, float hbreadth, 
	   float height, bool drive = false, bool shoot = false);
  
  /** A virtual destructor is needed to let subclasses do their cleanup. */
  virtual ~Obstacle();
  
  /** This function returns a string describing what kind of obstacle this is.
   */
  virtual std::string getType() const = 0;
  
  /** This function returns the position of this obstacle. */
  const float* getPosition() const;
  
  /** This function returns the obstacle's rotation around its own Y axis. */
  float getRotation() const;
  
  /** This function returns half the obstacle's X size. */
  float getWidth() const;
  
  /** This function returns half the obstacle's Y size. */
  float getBreadth() const;
  
  /** This function returns the obstacle's full height. */
  float getHeight() const;
  
  /** This function returns the time of intersection between the obstacle
      and a Ray object. If the ray does not intersect this obstacle -1 is 
      returned. */
  virtual float	intersect(const Ray&) const = 0;
  
  /** This function computes the two-dimensional surface normal of this 
      obstacle at the point @c p. The normal is stored in @c n. */
  virtual void getNormal(const float* p, float* n) const = 0;
  
  /** This function computes the three-dimensional surface normal of this 
      obstacle at the point @c p. The normal is stored in @c n. */
  virtual void get3DNormal(const float* p, float* n) const;
  
  /** This function checks if a tank, approximated as a cylinder with base
      centre in point @c p and radius @c radius, intersects this obstacle. */
  virtual bool isInside(const float* p, float radius) const = 0;
  
  /** This function checks if a tank, approximated as a box rotated around its
      Z axis, intersects this obstacle. */
  virtual bool isInside(const float* p, float angle,
			 float halfWidth, float halfBreadth) const = 0;
  
  /** This function checks if a horizontal rectangle crosses the surface of
      this obstacle.
      @param p           The position of the centre of the rectangle
      @param angle       The rotation of the rectangle
      @param halfWidth   Half the width of the rectangle
      @param halfBreadth Half the breadth of the rectangle
      @param plane       The tangent plane of the obstacle where it's 
                         intersected by the rectangle will be stored here
  */
  virtual bool isCrossing(const float* p, float angle,
			   float halfWidth, float halfBreadth,
			   float* plane) const;
  
  /** This function checks if a box moving from @c pos1 to @c pos2 will hit
      this obstacle, and if it does what the surface normal at the hitpoint is.
      @param pos1         The original position of the box
      @param azimuth1     The original rotation of the box
      @param pos2         The position of the box at the hit
      @param azimuth2     The rotation of the box at the hit
      @param halfWidth    Half the width of the box
      @param halfBreadth  Half the breadth of the box
      @param height       The height of the box
      @param normal       The surface normal of this obstacle at the hit point
                          will be stored here
      @returns            @c true if the box hits this obstacle, @c false 
                          otherwise
  */
  virtual bool getHitNormal(const float* pos1, float azimuth1,
			    const float* pos2, float azimuth2,
			    float halfWidth, float halfBreadth,
			    float height, float* normal) const = 0;
  
  /** This function returns a pointer to an ObstacleSceneNodeGenerator for this
      obstacle. */
  virtual ObstacleSceneNodeGenerator* newSceneNodeGenerator() const = 0;
  
  /** This function returns @c true if tanks can pass through this object,
      @c false if they can't. */
  bool isDriveThrough() const;
  
  /** This function returns @c true if bullets can pass through this object,
      @c false if they can't. */
  bool isShootThrough() const;
  
  /** This function sets the "zFlip" flag of this obstacle, i.e. if it's 
      upside down. */
  void setZFlip(void);
  
  /** This function returns the "zFlip" flag of this obstacle.
      @see setZFlip()
  */
  bool getZFlip(void) const;
  
 protected:
  /** This function checks if a moving horizontal rectangle will hit a 
      box-shaped obstacle, and if it does, computes the obstacle's normal 
      at the hitpoint.
      @param pos1        The original position of the rectangle
      @param azimuth1    The original rotation of the rectangle
      @param pos2        The final position of the rectangle
      @param azimuth2    The final rotation of the rectangle
      @param halfWidth   Half the width of the rectangle
      @param halfBreadth Half the breadth of the rectangle
      @param oPos        The position of the obstacle
      @param oAzimuth    The rotation of the obstacle
      @param oWidth      Half the width of the obstacle
      @param oBreadth    Half the breadth of the obstacle
      @param oHeight     The height of the obstacle
      @param normal      The surface normal of the obstacle at the hitpoint 
                         will be stored here
      @returns           The time of the hit, where 0 is the time when the 
                         rectangle is at @c pos1 and 1 is the time when it's
			 at @c pos2, and -1 means "no hit"
  */
  float getHitNormal(const float* pos1, float azimuth1,
		     const float* pos2, float azimuth2,
		     float halfWidth, float halfBreadth,
		     const float* oPos, float oAzimuth,
		     float oWidth, float oBreadth, float oHeight,
		     float* normal) const;
  
 protected:
  float		pos[3];
  float		angle;
  float		width;
  float		breadth;
  float		height;
  bool		driveThrough;
  bool		shootThrough;
  bool		ZFlip;
};

//
// Obstacle
//

inline const float*	Obstacle::getPosition() const
{
  return pos;
}

inline float		Obstacle::getRotation() const
{
  return angle;
}

inline float		Obstacle::getWidth() const
{
  return width;
}

inline float		Obstacle::getBreadth() const
{
  return breadth;
}

inline float		Obstacle::getHeight() const
{
  return height;
}

inline void		Obstacle::get3DNormal(const float *p, float *n) const
{
  getNormal(p, n);
}

#endif // BZF_OBSTACLE_H

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

