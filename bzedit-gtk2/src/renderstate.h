/*
 * renderstate.h - Classes and structures to keep track of the rendering state
 *
 * BZEdit
 * Copyright (C) 2004 David Trowbridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __RENDER_STATE_H__
#define __RENDER_STATE_H__

#include <gtk/gtk.h>
#include <GL/gl.h>

G_BEGIN_DECLS

#define RENDER_STATE_TYPE            (render_state_get_type ())
#define RENDER_STATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RENDER_STATE_TYPE, RenderState))
#define RENDER_STATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RENDER_STATE_TYPE, RenderStateClass))
#define IS_RENDER_STATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RENDER_STATE_TYPE))
#define IS_RENDER_STATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RENDER_STATE_TYPE))

typedef struct _RenderState      RenderState;
typedef struct _RenderStateClass RenderStateClass;

struct _RenderState
{
  GObject parent;

  gboolean cube_map;
  gboolean picking;
  GHashTable *picking_state;
  guint picking_name;
};

struct _RenderStateClass
{
  GObjectClass parent_class;
};

GType        render_state_get_type     (void) G_GNUC_CONST;
RenderState* render_state_new          (void);
void         render_state_picking_name (RenderState *state, gpointer drawable);
gpointer     render_state_do_pick      (RenderState *state, guint name);

G_END_DECLS

#endif
