/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include <thunarx/thunarx-private.h>



static GQuark thunarx_object_list_quark = 0;



/* takes a reference on target for each object in object_list */
void
thunarx_object_list_take_reference (GList   *object_list,
                                    gpointer target)
{
  GList *lp;

  /* allocate the "thunarx-object-list" quark on-demand */
  if (G_UNLIKELY (thunarx_object_list_quark == 0))
    thunarx_object_list_quark = g_quark_from_static_string ("thunarx-object-list");

  for (lp = object_list; lp != NULL; lp = lp->next)
    {
      g_object_set_qdata_full (lp->data, thunarx_object_list_quark, target, g_object_unref);
      g_object_ref (target);
    }
}


