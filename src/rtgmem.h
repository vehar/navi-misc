/*
 * rtgmem.h - Memory address utilities
 *
 * rtgraph real-time graphing package
 * Copyright (C) 2004 Micah Dowty <micah@navi.cx>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __RTG_MEM_H__
#define __RTG_MEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Define an integer type that's the same size as a pointer */
#if SIZEOF_VOID_P == SIZEOF_UNSIGNED_SHORT
typedef unsigned short RtgAddress;
#elif SIZEOF_VOID_P == SIZEOF_UNSIGNED_INT
typedef unsigned int RtgAddress;
#elif SIZEOF_VOID_P == SIZEOF_UNSIGNED_LONG
typedef unsigned long RtgAddress;
#elif SIZEOF_VOID_P == SIZEOF_UNSIGNED_LONG_LONG
typedef unsigned long long RtgAddress;
#else
#error Can't find an integer type of equal size to this architecture's pointers
#endif

/* Sanity check- make sure G_MEM_ALIGN is a power of two */
#if ((G_MEM_ALIGN) & ((G_MEM_ALIGN)-1)) != 0
#error G_MEM_ALIGN doesnt appear to be a power of two
#endif

/* Round an address or size down to a multiple of G_MEM_ALIGN */
#define RTG_ALIGN_FLOOR(x)  ((x) & (~((RtgAddress)((G_MEM_ALIGN)-1))))

/* Round an address or size up to a multiple of G_MEM_ALIGN */
#define RTG_ALIGN_CEIL(x)   (RTG_ALIGN_FLOOR(x) + (G_MEM_ALIGN))

G_END_DECLS

#endif /* __RTG_MEM_H__ */

/* The End */
