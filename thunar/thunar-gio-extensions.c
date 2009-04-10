/* $Id$ */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>

#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-private.h>



GFile *
g_file_new_for_home (void)
{
  return g_file_new_for_path (xfce_get_homedir ());
}



GFile *
g_file_new_for_root (void)
{
  return g_file_new_for_uri ("file:///");
}



GFile *
g_file_new_for_trash (void)
{
  return g_file_new_for_uri ("trash:///");
}



gboolean
g_file_is_root (GFile *file)
{
  GFile   *parent;
  gboolean is_root = FALSE;

  parent = g_file_get_parent (file);
  if (G_UNLIKELY (parent == NULL))
    is_root = TRUE;
  g_object_unref (parent);

  return is_root;
}



gboolean 
g_file_is_trashed (GFile *file)
{
  _thunar_return_val_if_fail (G_IS_FILE (file), FALSE);
  return g_file_has_uri_scheme (file, "trash");
}



/**
 * g_file_list_new_from_string:
 * @string : a string representation of an URI list.
 *
 * Splits an URI list conforming to the text/uri-list
 * mime type defined in RFC 2483 into individual URIs,
 * discarding any comments and whitespace. The resulting
 * list will hold one #GFile for each URI.
 *
 * If @string contains no URIs, this function
 * will return %NULL.
 *
 * Return value: the list of #GFile<!---->s or %NULL.
 **/
GList *
g_file_list_new_from_string (const gchar *string)
{
  GList  *list = NULL;
  gchar **uris;
  gsize   n;

  uris = g_uri_list_extract_uris (string);

  for (n = 0; uris != NULL && uris[n] != NULL; ++n)
    list = g_list_append (list, g_file_new_for_uri (uris[n]));

  g_strfreev (uris);

  return list;
}



/**
 * g_file_list_to_string:
 * @list : a list of #GFile<!---->s.
 *
 * Free the returned value using g_free() when you
 * are done with it.
 *
 * Return value: the string representation of @list conforming to the
 *               text/uri-list mime type defined in RFC 2483.
 **/
gchar *
g_file_list_to_string (GList *list)
{
  GString *string;
  gchar   *uri;
  GList   *lp;

  /* allocate initial string */
  string = g_string_new (NULL);

  for (lp = list; lp != NULL; lp = lp->next)
    {
      uri = g_file_get_uri (lp->data);
      /* TODO Would UTF-8 in URIs be ok? */
      string = g_string_append_uri_escaped (string, 
                                            uri, 
                                            G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, 
                                            FALSE);
      g_free (uri);

      string = g_string_append (string, "\r\n");
    }

  return g_string_free (string, FALSE);
}



/**
 * g_file_list_copy:
 * @list : a list of #GFile<!---->s.
 *
 * Takes a deep copy of @list and returns the
 * result. The caller is responsible to free the
 * returned list using g_file_list_free().
 *
 * Return value: a deep copy of @list.
 **/
GList*
g_file_list_copy (GList *list)
{
  GList *copy = NULL;
  GList *lp;

  for (list = NULL, lp = g_list_last (list); lp != NULL; lp = lp->prev)
    copy = g_list_prepend (copy, g_object_ref (lp->data));

  return list;
}



/**
 * g_file_list_free:
 * @list : a list of #GFile<!---->s.
 *
 * Frees the #GFile<!---->s in @list and
 * the @list itself.
 **/
void
g_file_list_free (GList *list)
{
  GList *lp;
  for (lp = list; lp != NULL; lp = lp->next)
    g_object_unref (lp->data);
  g_list_free (list);
}
