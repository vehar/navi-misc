/*
 * PipelineDrawing.cs - Gtk.DrawingArea subclass that handles drawing of
 *	a Fyre pipeline
 *
 * Fyre - a generic framework for computational art
 * Copyright (C) 2004-2005 Fyre Team (see AUTHORS)
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

namespace Fyre
{
	class PipelineDrawing : Gtk.DrawingArea
	{
		Gtk.Scrollbar		hscroll;
		Gtk.Scrollbar		vscroll;
		Gtk.Adjustment		hadj;
		Gtk.Adjustment		vadj;

		// Back buffer
		Gdk.Pixmap		backing;

		// We use a 50ms timeout when we to redraw
		uint			redraw_timeout;
		uint			scrollbars_timeout;

		// Whether or not we should update the canvas sizes
		bool			update_sizes;

		// This is the main layout. Yep.
		Layout			layout;

		/* The drawing extents are the size of our current drawing area. The
		 * position depends on the scrollbars, and the size is always the pixel
		 * size of the drawing area.
		 *
		 * Scrollbars are determined by the union of the drawing and the layout
		 * boxes, plus some buffer around the layout (maybe 50px?) size to make
		 * it easier to drag the document for a bigger size.
		 */

		Gdk.Rectangle		drawing_extents;
		public Gdk.Rectangle	DrawingExtents
		{
			get { return drawing_extents; }
		}
		public Gdk.Rectangle	CanvasExtents
		{
			get { return ExpandRect (layout.Extents, layout_buffer).Union (drawing_extents); }
		}

		// Size of the buffer of pixels around the items on the canvas, giving
		// the user something to grab on to if they want to move the diagram.
		static int		layout_buffer = 50;

		// We can't get events on a drawing area, so we have an event box on top
		// of it.
		Gtk.EventBox		event_box;

		// Internal data to keep track of dragging
		bool			dragging;
		int			drag_x, drag_y;

		/*
		 * We use a bunch of different cursors here:
		 *
		 * Open hand: normal cursor when the mouse is just over the background
		 * Closed hand: used when dragging the document canvas around
		 * Pointer: cursor when the mouse is hovering over an element
		 * Fleur: cursor when moving an element
		 * FIXME: cursor when hovering over a pad
		 * FIXME: cursor when dragging out a new edge
		 */
		Gdk.Cursor hand_open_cursor;
		Gdk.Cursor HandOpenCursor
		{
			get {
				if (hand_open_cursor == null)
					hand_open_cursor = CreateCursor ("hand-open-data.png", "hand-open-mask.png", 20, 20, 10, 10);
				return hand_open_cursor;
			}
		}
		Gdk.Cursor hand_closed_cursor;
		Gdk.Cursor HandClosedCursor
		{
			get {
				if (hand_closed_cursor == null)
					hand_closed_cursor = CreateCursor ("hand-closed-data.png", "hand-closed-mask.png", 20, 20, 10, 10);
				return hand_closed_cursor;
			}
		}
		Gdk.Cursor pointer_cursor;
		Gdk.Cursor PointerCursor
		{
			get {
				if (pointer_cursor == null)
					pointer_cursor = new Gdk.Cursor (Gdk.CursorType.LeftPtr);
				return pointer_cursor;
			}
		}
		Gdk.Cursor fleur_cursor;
		Gdk.Cursor FleurCursor
		{
			get {
				if (fleur_cursor == null)
					fleur_cursor = new Gdk.Cursor (Gdk.CursorType.Fleur);
				return fleur_cursor;
			}
		}

		public
		PipelineDrawing (Glade.XML xml) : base ()
		{
			drawing_extents = new Gdk.Rectangle ();

			layout = new Layout ();

			drawing_extents.X = 0;
			drawing_extents.Y = 0;

			// Get a handle to our event box and set event handlers
			event_box = (Gtk.EventBox) xml.GetWidget ("pipeline_window");
			event_box.ButtonPressEvent   += new Gtk.ButtonPressEventHandler   (ButtonPressHandler);
			event_box.ButtonReleaseEvent += new Gtk.ButtonReleaseEventHandler (ButtonReleaseHandler);
			event_box.MotionNotifyEvent  += new Gtk.MotionNotifyEventHandler  (MotionNotifyHandler);
			event_box.LeaveNotifyEvent   += new Gtk.LeaveNotifyEventHandler   (LeaveNotifyHandler);

			update_sizes = true;

			redraw_timeout = 0;
			scrollbars_timeout = 0;

			Show ();
		}

		Gdk.Rectangle
		ExpandRect (Gdk.Rectangle source, int pixels)
		{
			Gdk.Rectangle ret = new Gdk.Rectangle ();
			ret.X = source.X - pixels;
			ret.Y = source.Y - pixels;
			ret.Width  = source.Width  + pixels*2;
			ret.Height = source.Height + pixels*2;
			return ret;
		}

		Gdk.Cursor
		CreateCursor (string data, string mask, int width, int height, int x_hotspot, int y_hotspot)
		{
			Gdk.Pixbuf data_pixbuf = new Gdk.Pixbuf (null, data);
			Gdk.Pixbuf mask_pixbuf = new Gdk.Pixbuf (null, mask);
			Gdk.Pixmap data_pixmap = new Gdk.Pixmap (null, width, height, 1);
			Gdk.Pixmap mask_pixmap = new Gdk.Pixmap (null, width, height, 1);
			data_pixbuf.RenderThresholdAlpha (data_pixmap, 0, 0, 0, 0, width, height, 1);
			mask_pixbuf.RenderThresholdAlpha (mask_pixmap, 0, 0, 0, 0, width, height, 1);

			Gdk.Color fore = new Gdk.Color (0xff, 0xff, 0xff);
			Gdk.Color back = new Gdk.Color (0x00, 0x00, 0x00);

			return new Gdk.Cursor (data_pixmap, mask_pixmap, fore, back, x_hotspot, y_hotspot);
		}

		protected override bool
		OnExposeEvent (Gdk.EventExpose ev)
		{
			Gdk.Color white = new Gdk.Color (0xff, 0xff, 0xff);
			Gdk.Colormap.System.AllocColor (ref white, true, true);
			Gdk.GC gc = new Gdk.GC (backing);
			gc.Foreground = white;

			backing.DrawRectangle (gc, true, ev.Area);

			System.Drawing.Graphics g = Gtk.DotNet.Graphics.FromDrawable (backing);
			g.ResetTransform ();
			g.TranslateTransform ((float) -drawing_extents.X, (float) -drawing_extents.Y);

			System.Drawing.Rectangle re = new System.Drawing.Rectangle();
			re.X      = drawing_extents.X;
			re.Y      = drawing_extents.Y;
			re.Width  = drawing_extents.Width;
			re.Height = drawing_extents.Height;
			layout.Draw (g, re);

			Gdk.Rectangle r = ev.Area;
			GdkWindow.DrawDrawable (gc, backing, r.X, r.Y, r.X, r.Y, r.Width, r.Height);

			return true;
		}

		protected override bool
		OnConfigureEvent (Gdk.EventConfigure ev)
		{
			drawing_extents.Width = ev.Width;
			drawing_extents.Height = ev.Height;

			if (hadj != null)
				SetScrollbars ();

			// Make sure our cursor is set
			event_box.GdkWindow.Cursor = HandOpenCursor;

			// Create the backing store
			backing = new Gdk.Pixmap (GdkWindow, ev.Width, ev.Height, -1);

			return base.OnConfigureEvent (ev);
		}

		public void
		SetScrollbars ()
		{
			if (scrollbars_timeout == 0)
				scrollbars_timeout = GLib.Timeout.Add (50, new GLib.TimeoutHandler (SetScrollbarsTimeout));
		}

		bool
		SetScrollbarsTimeout ()
		{
			Gdk.Rectangle r = layout.Extents;
			Gdk.Rectangle size;

			if (r.Width == 0 && r.Height == 0) {
				size = drawing_extents;
			} else {
				Gdk.Rectangle exp_layout = ExpandRect (r, layout_buffer);
				size = exp_layout.Union (drawing_extents);
			}

			hadj.Lower = size.X;
			vadj.Lower = size.Y;

			hadj.Upper = size.X + size.Width;
			vadj.Upper = size.Y + size.Height;

			hadj.PageIncrement = drawing_extents.Width  / 2;
			vadj.PageIncrement = drawing_extents.Height / 2;
			hadj.PageSize = drawing_extents.Width;
			vadj.PageSize = drawing_extents.Height;

			scrollbars_timeout = 0;

			return false;
		}

		public void
		AddScrollbars (Gtk.Scrollbar hscroll, Gtk.Scrollbar vscroll)
		{
			this.hscroll = hscroll;
			this.vscroll = vscroll;
			hadj = hscroll.Adjustment;
			vadj = vscroll.Adjustment;

			hadj.StepIncrement = 10.0;
			vadj.StepIncrement = 10.0;

			hadj.ValueChanged += new System.EventHandler (HValueChanged);
			vadj.ValueChanged += new System.EventHandler (VValueChanged);

			SetScrollbars ();

			hadj.Value = layout.Extents.X - layout_buffer;
			vadj.Value = layout.Extents.Y - layout_buffer;
		}

		void
		HValueChanged (object o, System.EventArgs e)
		{
			Gtk.Adjustment a = (Gtk.Adjustment) o;
			drawing_extents.X = (int) a.Value;
			if (update_sizes)
				SetScrollbars ();
		}

		void
		VValueChanged (object o, System.EventArgs e)
		{
			Gtk.Adjustment a = (Gtk.Adjustment) o;
			drawing_extents.Y = (int) a.Value;
			if (update_sizes)
				SetScrollbars ();
		}

		void
		ButtonPressHandler (object o, Gtk.ButtonPressEventArgs args)
		{
			Gdk.EventButton ev = args.Event;

			// FIXME - check whether we're clicking on an element. If so, select
			// it/begin drag/etc.  We also need behavior for right- and double-clicks.
			// Right click should pop up a context menu (appropriate for whatever the
			// user's mouse is hovering over).  Double clicking on an element should
			// pop up an edit dialog for it.

			drag_x = (int) ev.X;
			drag_y = (int) ev.Y;

			dragging = true;

			event_box.GdkWindow.Cursor = HandClosedCursor;
		}

		void
		ButtonReleaseHandler (object o, Gtk.ButtonReleaseEventArgs args)
		{
			dragging = false;
			event_box.GdkWindow.Cursor = HandOpenCursor;
		}

		void
		MotionNotifyHandler (object o, Gtk.MotionNotifyEventArgs args)
		{
			Gdk.EventMotion ev = args.Event;

			// FIXME - we'll want to check whether we're moused over an element here
			// and change cursors/draw prelights appropriately

			if (dragging && ev.State == Gdk.ModifierType.Button1Mask) {
				// Compute the offset from the last event we got, and move
				// our view appropriately.
				int offset_x = ((int) ev.X) - drag_x;
				int offset_y = ((int) ev.Y) - drag_y;

				if (offset_x > 0 && hadj.Value == hadj.Lower)
					hadj.Lower -= offset_x;
				if (offset_y > 0 && vadj.Value == vadj.Lower)
					vadj.Lower -= offset_y;

				SetViewPosition ((float) (drawing_extents.X - offset_x), (float) (drawing_extents.Y - offset_y));
				SetScrollbars ();

				// Set these so we'll get proper offsets next time there's an event.
				drag_x = (int) ev.X;
				drag_y = (int) ev.Y;
			}
		}

		void
		LeaveNotifyHandler (object o, Gtk.LeaveNotifyEventArgs args)
		{
			// FIXME - not sure this is needed
		}

		public void
		SetViewPosition (float x, float y)
		{
			update_sizes = false;
			hadj.Value = x;
			vadj.Value = y;
			update_sizes = true;

			if (redraw_timeout == 0)
				redraw_timeout = GLib.Timeout.Add (50, new GLib.TimeoutHandler (RedrawTimeout));
		}

		bool
		RedrawTimeout ()
		{
			Gdk.Rectangle r = drawing_extents;
			r.X = 0; r.Y = 0;
			GdkWindow.InvalidateRect (r, false);

			redraw_timeout = 0;

			return false;
		}

		public void
		RedrawScrollbars ()
		{
			hscroll.GdkWindow.InvalidateRect (hscroll.Allocation, true);
			vscroll.GdkWindow.InvalidateRect (hscroll.Allocation, true);
			hscroll.GdkWindow.ProcessUpdates (true);
			vscroll.GdkWindow.ProcessUpdates (true);
		}

		public void
		AddElement (Element e, int x, int y)
		{
			CanvasElement ce = new CanvasElement (e, GdkWindow);
			ce.position.X = drawing_extents.X + x;
			ce.position.Y = drawing_extents.Y + y;

			layout.Add (e, ce);
		}
	}

}
