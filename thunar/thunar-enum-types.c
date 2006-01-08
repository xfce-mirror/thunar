/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-enum-types.h>



static void thunar_enum_from_string (const GValue *src_value,
                                     GValue       *dst_value);



GType
thunar_recursive_permissions_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] =
      {
        { THUNAR_RECURSIVE_PERMISSIONS_ASK,    "THUNAR_RECURSIVE_PERMISSIONS_ASK",    "ask",    },
        { THUNAR_RECURSIVE_PERMISSIONS_ALWAYS, "THUNAR_RECURSIVE_PERMISSIONS_ALWAYS", "always", },
        { THUNAR_RECURSIVE_PERMISSIONS_NEVER,  "THUNAR_RECURSIVE_PERMISSIONS_NEVER",  "never",  },
        { 0,                                   NULL,                                  NULL,     },
      };

      type = g_enum_register_static (I_("ThunarRecursivePermissions"), values);

      /* register transformation function for string->ThunarRecursivePermissions */
      g_value_register_transform_func (G_TYPE_STRING, type, thunar_enum_from_string);
    }

  return type;
}



static void
thunar_enum_from_string (const GValue *src_value,
                         GValue       *dst_value)
{
  GEnumClass *klass;
  gint        value = 0;
  gint        n;

  /* determine the enum value matching the src_value... */
  klass = g_type_class_ref (G_VALUE_TYPE (dst_value));
  for (n = 0; n < klass->n_values; ++n)
    {
      value = klass->values[n].value;
      if (exo_str_is_equal (klass->values[n].value_name, g_value_get_string (src_value)))
        break;
    }
  g_type_class_unref (klass);

  /* ...and return that value */
  g_value_set_enum (dst_value, value);
}



