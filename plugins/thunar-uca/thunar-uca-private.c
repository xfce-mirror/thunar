/* vi:set et ai sw=2 sts=2 ts=2: */
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

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-uca/thunar-uca-private.h>

#include <exo/exo.h>
#include <thunarx/thunarx.h>

/**
 * thunar_uca_i18n_init:
 *
 * Sets up i18n support for the thunar-uca plugin.
 **/
void
thunar_uca_i18n_init (void)
{
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
}



/**
 * expand first the field codes that are deprecated in Freedesktop.org specification
 * @param command The command to expand
 * @param file_infos
 * @return The command with expansions
 */
gchar*
thunar_uca_expand_deprecated_desktop_entry_field_codes (const gchar	*command,
														GList		*file_infos)
{
  const gchar        *p;
  gchar              *dirname;
  gchar              *path;
  GList              *lp;
  GFile              *location;
  GString            *command_line = g_string_new (NULL);

  /* expand first the field codes that are deprecated in Freedesktop.org specification */
  for (p = command; *p != '\0'; ++p)
  {
	if (G_UNLIKELY (p[0] == '%' && p[1] != '\0'))
	{
	  switch (*++p)
	  {
	  case 'd':
		if (G_LIKELY (file_infos != NULL))
		{
		  location = thunarx_file_info_get_location (file_infos->data);
		  path = g_file_get_path (location);
		  g_object_unref (location);

		  if (G_UNLIKELY (path == NULL))
			goto error;

		  dirname = g_path_get_dirname (path);
		  xfce_append_quoted (command_line, dirname);
		  g_free (dirname);
		  g_free (path);
		}
		break;

	  case 'D':
		for (lp = file_infos; lp != NULL; lp = lp->next)
		{
		  if (G_LIKELY (lp != file_infos))
			g_string_append_c (command_line, ' ');

		  location = thunarx_file_info_get_location (lp->data);
		  path = g_file_get_path (location);
		  g_object_unref (location);

		  if (G_UNLIKELY (path == NULL))
			goto error;

		  dirname = g_path_get_dirname (path);
		  xfce_append_quoted (command_line, dirname);
		  g_free (dirname);
		  g_free (path);
		}
		break;

	  case 'n':
		if (G_LIKELY (file_infos != NULL))
		{
		  path = thunarx_file_info_get_name (file_infos->data);
		  xfce_append_quoted (command_line, path);
		  g_free (path);
		}
		break;

	  case 'N':
		for (lp = file_infos; lp != NULL; lp = lp->next)
		{
		  if (G_LIKELY (lp != file_infos))
			g_string_append_c (command_line, ' ');

		  path = thunarx_file_info_get_name (lp->data);
		  xfce_append_quoted (command_line, path);
		  g_free (path);
		}
		break;

	  default:
		g_string_append_c (command_line, '%');
		g_string_append_c (command_line, *p);
		break;
	  }
	}
	else
	{
	  g_string_append_c (command_line, *p);
	}
  }

  path = command_line->str;

  error:
  g_string_free (command_line, path == NULL);
  return path;
}
