/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-gobject-extensions.h>



static void transform_string_to_boolean (const GValue *src, GValue *dst);
static void transform_string_to_enum    (const GValue *src, GValue *dst);
static void transform_string_to_int     (const GValue *src, GValue *dst);
static void transform_string_to_uint    (const GValue *src, GValue *dst);



static void
transform_string_to_boolean (const GValue *src,
                             GValue       *dst)
{
  g_value_set_boolean (dst, strcmp (g_value_get_string (src), "FALSE") != 0);
}



static void
transform_string_to_enum (const GValue *src,
                          GValue       *dst)
{
  GEnumClass *klass;
  gint        value = 0;
  guint       n;

  /* determine the enum value matching the src... */
  klass = g_type_class_ref (G_VALUE_TYPE (dst));
  for (n = 0; n < klass->n_values; ++n)
    {
      value = klass->values[n].value;
      if (exo_str_is_equal (klass->values[n].value_name, g_value_get_string (src)))
        break;
    }
  g_type_class_unref (klass);

  /* ...and return that value */
  g_value_set_enum (dst, value);
}



static void
transform_string_to_int (const GValue *src,
                         GValue       *dst)
{
  g_value_set_int (dst, (gint) strtol (g_value_get_string (src), NULL, 10));
}



static void
transform_string_to_uint (const GValue *src,
                          GValue       *dst)
{
  g_value_set_uint (dst, (guint) strtoul (g_value_get_string (src), NULL, 10));
}



/**
 * thunar_g_initialize_transformations:
 *
 * Registers various transformation functions to the
 * GLib Type System, which are used by g_value_transform()
 * to transform between different kinds of values.
 **/
void
thunar_g_initialize_transformations (void)
{
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_BOOLEAN))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, transform_string_to_boolean);
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_INT))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT, transform_string_to_int);
  if (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_UINT))
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT, transform_string_to_uint);

  /* register a transformation function string->enum unconditionally */
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ENUM, transform_string_to_enum);
}



/**
 * thunar_g_strescape
 * @source  : The string to escape
 *
 * Similar to g_strescape, but as well escapes SPACE
 *
 * Escapes the special characters '\b', '\f', '\n', '\r', '\t', '\v', '\' ' ' and '"' in the string source by inserting a '\' before them.
 * Additionally all characters in the range 0x01-0x1F (SPACE and everything below)
 * and in the range 0x7F-0xFF (all non-ASCII chars) are replaced with a '\' followed by their octal representation.
 *
 * Return value: (transfer full): The new string. Has to be freed with g_free after usage.
 **/
gchar*
thunar_g_strescape (const gchar *source)
{
  gchar*       g_escaped;
  gchar*       result;
  unsigned int j = 0;
  unsigned int new_size = 0;

  /* First apply the default escaping .. will escape everything, expect SPACE */
  g_escaped = g_strescape (source, NULL);

  /* calc required new size */
  for (unsigned int i = 0; i < strlen (g_escaped); i++)
    {
      if (g_escaped[i] == ' ')
        new_size++;
      new_size++;
    }

  /* strlen() does not include the \0 character, add an extra slot for it */
  new_size++;
  result = malloc (new_size * sizeof (gchar));

  for (unsigned int i = 0; i < strlen (g_escaped); i++)
    {
      if (g_escaped[i] == ' ')
        {
          result[j] = '\\';
          j++;
        }
      result[j] = g_escaped[i];
      j++;
    }
  result[j] = '\0';
  g_free (g_escaped);
  return result;
}
