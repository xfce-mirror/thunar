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
 *
 * Based on code initially written by James Youngman <jay@gnu.org>
 * and Pavel Cisler <pavel@eazel.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif

#include <thunar-vfs/thunar-vfs-mime-sniffer.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/**
 * thunar_vfs_mime_sniffer_looks_like_text:
 * @data   : the input data (most probably file content).
 * @length : the number of bytes in @data.
 *
 * Checks whether @data can be considered valid text, e.g.
 * valid UTF-8 or valid multi-byte string. If so, %TRUE is
 * returned, else %FALSE is returned.
 *
 * Return value: %TRUE if @data looks like text, else %FALSE.
 **/
gboolean
thunar_vfs_mime_sniffer_looks_like_text (const gchar *data,
                                         gsize        length)
{
  const gchar *end;
#ifdef HAVE_MBRTOWC
  const gchar *send;
  const gchar *src;
  mbstate_t    ms;
  wchar_t      wc;
  gsize        wlen;
#endif
  gint         remaining;

  /* check if we have valid UTF8 here */
  if (!g_utf8_validate (data, length, &end))
    {
      /* Check whether the string was truncated in the middle of a valid
       * UTF8 character, or if we really have an invalid UTF8 string.
       */
      remaining = length - (end - data);
      if (g_utf8_get_char_validated (end, remaining) == (gunichar)-2)
        return TRUE;

#ifdef HAVE_MBRTOWC
      /* let's see, maybe we have a valid multi-byte text here */
      memset (&ms, 0, sizeof (ms));
      for (src = data, send = data + length; src < send; src += wlen)
        {
          /* Don't allow embedded zeros in textfiles */
          if (*src == '\0')
            return FALSE;

          /* check for illegal mutli-byte sequence */
          wlen = mbrtowc (&wc, src, send - src, &ms);
          if (G_UNLIKELY (wlen == (gsize) (-1)))
            return FALSE;

          /* Check for incomplete multi-byte character before
           * end. Probably cut off char which is ok.
           */
          if (wlen == (gsize) (-2))
            return TRUE;

          /* Don't allow embedded zeros in textfiles */
          if (G_UNLIKELY (wlen == 0))
            return FALSE;

          /* Check for neither printable or whitespace */
          if (!iswspace (wc) && !iswprint (wc))
            return FALSE;
        }
#endif
    }

  return TRUE;
}



#define __THUNAR_VFS_MIME_SNIFFER_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
