/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

/* Use g_fopen() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_fopen(filename, mode) (fopen ((filename), (mode)))
#endif

#include <thunar-vfs/thunar-vfs-monitor.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/**
 * _thunar_vfs_g_type_register_simple:
 * @type_parent      : the parent #GType.
 * @type_name_static : the name of the new type, allocated on static storage,
 *                     must not change during the lifetime of the process.
 * @class_size       : the size of the class structure in bytes.
 * @class_init       : the class initialization function or %NULL.
 * @instance_size    : the size of the instance structure in bytes.
 * @instance_init    : the constructor function or %NULL.
 * @flags            : the #GTypeFlags or %0.
 *
 * Simple wrapper for g_type_register_static(), used to reduce the
 * number of relocations required for the various #GTypeInfo<!---->s.
 *
 * Return value: the newly registered #GType.
 **/
GType
_thunar_vfs_g_type_register_simple (GType        type_parent,
                                    const gchar *type_name_static,
                                    guint        class_size,
                                    gpointer     class_init,
                                    guint        instance_size,
                                    gpointer     instance_init,
                                    GTypeFlags   flags)
{
  /* setup the type info on the stack */
  GTypeInfo info =
  {
    class_size,
    NULL,
    NULL,
    (GClassInitFunc) class_init,
    NULL,
    NULL,
    instance_size,
    0,
    (GInstanceInitFunc) instance_init,
    NULL,
  };

  /* register the new type */
  return g_type_register_static (type_parent, I_(type_name_static), &info, 0);
}



/**
 * _thunar_vfs_g_value_array_free:
 * @values   : an array of #GValue<!---->s.
 * @n_values : the number of #GValue<!---->s in @values.
 *
 * Calls g_value_unset() for all items in @values and frees
 * the @values using g_free().
 **/
void
_thunar_vfs_g_value_array_free (GValue *values,
                                guint   n_values)
{
  _thunar_vfs_return_if_fail (values != NULL);

  while (n_values-- > 0)
    g_value_unset (values + n_values);
  g_free (values);
}



/**
 * _thunar_vfs_check_only_local:
 * @path_list : a #GList of #ThunarVfsPath<!---->s.
 * @error     : return location for errors or %NULL.
 *
 * Verifies that all #ThunarVfsPath<!---->s in the @path_list are
 * local paths with scheme %THUNAR_VFS_PATH_SCHEME_FILE. If the
 * check fails, @error will be initialized to an #GError with
 * %G_FILE_ERROR_INVAL and %FALSE will be returned.
 *
 * Return value: %TRUE if the check succeeds, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_check_only_local (GList       *path_list,
                              GError     **error)
{
  GList *lp;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check all paths in the path_list */
  for (lp = path_list; lp != NULL; lp = lp->next)
    if (!_thunar_vfs_path_is_local (lp->data))
      break;

  /* check if any path failed the check */
  if (G_UNLIKELY (lp != NULL))
    {
      _thunar_vfs_set_g_error_not_supported (error);
      return FALSE;
    }

  return TRUE;
}



/**
 * _thunar_vfs_set_g_error_from_errno:
 * @error  : pointer to a #GError to set, or %NULL.
 * @serrno : the errno value to set the @error to.
 *
 * If @error is not %NULL it will be initialized to the errno
 * value in @serrno.
 **/
void
_thunar_vfs_set_g_error_from_errno (GError **error,
                                    gint     serrno)
{
  /* allocate a GError for the specified errno value */
  g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (serrno), "%s", g_strerror (serrno));
}



/**
 * _thunar_vfs_set_g_error_from_errno2:
 * @error  : pointer to a #GError to set, or %NULL.
 * @serrno : the errno value to set the @error to.
 * @format : a printf(3)-style format string.
 * @...    : arguments for the @format string.
 *
 * Similar to _thunar_vfs_set_g_error_from_errno(), but
 * allows to specify an additional string for the error
 * message text.
 **/
void
_thunar_vfs_set_g_error_from_errno2 (GError     **error,
                                     gint         serrno,
                                     const gchar *format,
                                     ...)
{
  va_list var_args;
  gchar  *message;

  /* allocate a GError for the specified errno value */
  va_start (var_args, format);
  message = g_strdup_vprintf (format, var_args);
  g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (serrno),
               "%s: %s", message, g_strerror (serrno));
  va_end (var_args);
  g_free (message);
}



/**
 * _thunar_vfs_set_g_error_from_errno3:
 * @error : pointer to a #GError to set, or %NULL.
 *
 * Similar to _thunar_vfs_set_g_error_from_errno(), but uses
 * the global errno value for this thread instead of taking
 * the errno value from a parameter.
 **/
void
_thunar_vfs_set_g_error_from_errno3 (GError **error)
{
  /* allocate a GError for the global errno value */
  _thunar_vfs_set_g_error_from_errno (error, errno);
}



/**
 * _thunar_vfs_set_g_error_not_supported:
 * @error : pointer to a #GError to set, or %NULL.
 *
 * Sets @error to point to a #GError telling that a
 * certain operation is not supported.
 **/
void
_thunar_vfs_set_g_error_not_supported (GError **error)
{
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOSYS, _("Operation not supported"));
}



static inline gint
unescape_character (const gchar *s)
{
  gint first_digit;
  gint second_digit;

  first_digit = g_ascii_xdigit_value (s[0]);
  if (first_digit < 0) 
    return -1;
  
  second_digit = g_ascii_xdigit_value (s[1]);
  if (second_digit < 0) 
    return -1;
  
  return (first_digit << 4) | second_digit;
}



/**
 * _thunar_vfs_unescape_rfc2396_string:
 * @escaped_string             : the escaped string.
 * @escaped_len                : the length of @escaped_string or %-1.
 * @illegal_escaped_characters : the characters that may not appear escaped
 *                               in @escaped_string.
 * @ascii_must_not_be_escaped  : %TRUE to disallow escaped ASCII characters.
 * @error                      : return location for errors or %NULL.
 *
 * Unescapes the @escaped_string according to RFC 2396.
 *
 * Return value: the unescaped string or %NULL in case of an error.
 **/
gchar*
_thunar_vfs_unescape_rfc2396_string (const gchar *escaped_string,
                                     gssize       escaped_len,
                                     const gchar *illegal_escaped_characters,
                                     gboolean     ascii_must_not_be_escaped,
                                     GError     **error)
{
  const gchar *s;
  gchar       *unescaped_string;
  gchar       *t;
  gint         c;

  _thunar_vfs_return_val_if_fail (illegal_escaped_characters != NULL, NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* verify that we have a valid string */
  if (G_UNLIKELY (escaped_string == NULL))
    return NULL;

  /* determine the length on-demand */
  if (G_UNLIKELY (escaped_len < 0))
    escaped_len = strlen (escaped_string);

  /* proces the escaped string */
  unescaped_string = g_malloc (escaped_len + 1);
  for (s = escaped_string, t = unescaped_string;; ++s)
    {
      /* determine the next character */
      c = *s;
      if (c == '\0')
        break;

      /* handle escaped characters */
      if (G_UNLIKELY (c == '%'))
        {
          /* catch partial escape sequence past the end of the substring */
          if (s[1] == '\0' || s[2] == '\0')
            goto error;

          /* unescape the character */
          c = unescape_character (s + 1);

          /* catch bad escape sequences and NUL characters */
          if (c <= 0)
            goto error;

          /* catch escaped ASCII */
          if (ascii_must_not_be_escaped && c > 0x1f && c <= 0x7f)
            goto error;

          /* catch other illegal escaped characters */
          if (strchr (illegal_escaped_characters, c) != NULL)
            goto error;

          s += 2;
        }

      *t++ = c;
    }
  *t = '\0';
  
  return unescaped_string;

error:
  /* TRANSLATORS: This error indicates that an URI contains an invalid escaped character (RFC 2396) */
  g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_BAD_URI, _("Invalidly escaped characters"));
  g_free (unescaped_string);
  return NULL;
}



/**
 * _thunar_vfs_desktop_file_set_value:
 * @filename : the absolute path to a .desktop file.
 * @key      : the key in the @filename to update.
 * @value    : the new value for the @key.
 * @error    : return location for errors or %NULL.
 *
 * Sets the @key in @filename to @value. If @key appears localized in @filename,
 * it will be set in the current locale if present, falling back to the unlocalized
 * setting.
 *
 * Return value: %TRUE if the @key in the @filename was successfully updated to
 *               @value, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_desktop_file_set_value (const gchar *filename,
                                    const gchar *key,
                                    const gchar *value,
                                    GError     **error)
{
  const gchar * const *locale;
  GKeyFile            *key_file;
  gsize                data_length;
  gchar               *data;
  gchar               *key_localized;
  FILE                *fp;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (g_utf8_validate (key, -1, NULL), FALSE);
  _thunar_vfs_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  /* try to open the .desktop file */
  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file, filename, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, error))
    {
err0: g_key_file_free (key_file);
      return FALSE;
    }

  /* check if the file is valid */
  if (G_UNLIKELY (!g_key_file_has_group (key_file, "Desktop Entry")))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid desktop file"));
      goto err0;
    }

  /* save the new value (localized if required) */
  for (locale = g_get_language_names (); *locale != NULL; ++locale)
    {
      key_localized = g_strdup_printf ("%s[%s]", key, *locale);
      if (g_key_file_has_key (key_file, "Desktop Entry", key_localized, NULL))
        {
          g_key_file_set_string (key_file, "Desktop Entry", key_localized, value);
          g_free (key_localized);
          break;
        }
      g_free (key_localized);
    }

  /* fallback to unlocalized value */
  if (G_UNLIKELY (*locale == NULL))
    g_key_file_set_string (key_file, "Desktop Entry", key, value);

  /* serialize the key_file to a buffer */
  data = g_key_file_to_data (key_file, &data_length, error);
  g_key_file_free (key_file);
  if (G_UNLIKELY (data == NULL))
    return FALSE;

  /* try to open the file for writing */
  fp = g_fopen (filename, "w");
  if (G_UNLIKELY (fp == NULL))
    {
      _thunar_vfs_set_g_error_from_errno3 (error);
err1: g_free (data);
      return FALSE;
    }

  /* write the data back to the file */
  if (fwrite (data, data_length, 1, fp) != 1)
    {
      _thunar_vfs_set_g_error_from_errno3 (error);
      fclose (fp);
      goto err1;
    }

  /* cleanup */
  g_free (data);
  fclose (fp);

  return TRUE;
}



#define __THUNAR_VFS_PRIVATE_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
