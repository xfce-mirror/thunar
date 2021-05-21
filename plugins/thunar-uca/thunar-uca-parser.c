/*
 * Copyright (c) 2021 Chigozirim Chukwu <noblechuk5[at]web[dot]de>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <thunar-uca/thunar-uca-parser.h>
#include <libxfce4util/libxfce4util.h>
#include <glib/gstdio.h>

/* *INDENT-OFF* */
static void thunar_uca_parser_init      (ThunarUcaParser        *parser);
static void thunar_uca_parser_deinit    (ThunarUcaParser        *parser);

static void start_element_handler       (GMarkupParseContext    *context,
                                         const gchar            *element_name,
                                         const gchar           **attribute_names,
                                         const gchar           **attribute_values,
                                         gpointer                user_data,
                                         GError                **error);
static void end_element_handler         (GMarkupParseContext    *context,
                                         const gchar            *element_name,
                                         gpointer                user_data,
                                         GError                **error);
static void text_handler                (GMarkupParseContext    *context,
                                         const gchar            *text,
                                         gsize                   text_len,
                                         gpointer                user_data,
                                         GError                **error);
/* *INDENT-ON* */


typedef enum
{
  PARSER_START,
  PARSER_ACTIONS,
  PARSER_ACTION,
  PARSER_ICON,
  PARSER_NAME,
  PARSER_SUB_FOLDER,
  PARSER_UNIQUE_ID,
  PARSER_COMMAND,
  PARSER_STARTUP_NOTIFY,
  PARSER_PATTERNS,
  PARSER_DESCRIPTION,
  PARSER_DIRECTORIES,
  PARSER_AUDIO_FILES,
  PARSER_IMAGE_FILES,
  PARSER_OTHER_FILES,
  PARSER_TEXT_FILES,
  PARSER_VIDEO_FILES,
  PARSER_UNKNOWN_ELEMENT,
} ParserState;

typedef XFCE_GENERIC_STACK(ParserState) ParserStack;

struct _ThunarUcaParser
{
  ParserStack    *stack;
  ThunarUcaModel *model;
  const gchar    *filename;
  gchar          *locale;
  GString        *name;
  GString        *submenu;
  gboolean        name_use;
  guint           name_match;
  GString        *unique_id;
  GString        *icon_name;
  GString        *command;
  GString        *patterns;
  GString        *description;
  gboolean        startup_notify;
  gboolean        description_use;
  guint           description_match;
  ThunarUcaTypes  types;
};



static const GMarkupParser markup_parser =
{
  .start_element = start_element_handler,
  .end_element   = end_element_handler,
  .text          = text_handler,
  NULL,
  NULL,
};



/**
 * initialize the parser
 * @param parser
 */
static void
thunar_uca_parser_init (ThunarUcaParser *parser)
{
#ifdef DEBUG
  g_debug ("Init thunar parser");
#endif
  parser->stack = xfce_stack_new (ParserStack);
  parser->locale = g_strdup (setlocale (LC_MESSAGES, NULL));
  parser->name = g_string_new (NULL);
  parser->submenu = g_string_new (NULL);
  parser->unique_id = g_string_new (NULL);
  parser->icon_name = g_string_new (NULL);
  parser->command = g_string_new (NULL);
  parser->patterns = g_string_new (NULL);
  parser->description = g_string_new (NULL);
  parser->startup_notify = FALSE;
}



static void
thunar_uca_parser_deinit (ThunarUcaParser *parser)
{
#ifdef DEBUG
  g_debug ("de-Init thunar parser");
#endif
  g_string_free (parser->description, TRUE);
  g_string_free (parser->patterns, TRUE);
  g_string_free (parser->command, TRUE);
  g_string_free (parser->icon_name, TRUE);
  g_string_free (parser->unique_id, TRUE);
  g_string_free (parser->submenu, TRUE);
  g_string_free (parser->name, TRUE);
  g_free (parser->locale);
  xfce_stack_free (parser->stack);
}



static void
start_element_handler (GMarkupParseContext *context,
                       const gchar         *element_name,
                       const gchar        **attribute_names,
                       const gchar        **attribute_values,
                       gpointer             user_data,
                       GError             **error)
{
  ThunarUcaParser *parser = user_data;
  guint            match, n;

  switch (xfce_stack_top (parser->stack))
    {
    case PARSER_START:
      if (strcmp (element_name, "actions") == 0)
        xfce_stack_push (parser->stack, PARSER_ACTIONS);
      else
        xfce_stack_push (parser->stack, PARSER_UNKNOWN_ELEMENT);
      break;

    case PARSER_ACTIONS:
      if (strcmp (element_name, "action") == 0)
        {
          parser->name_match = XFCE_LOCALE_NO_MATCH;
          parser->description_match = XFCE_LOCALE_NO_MATCH;
          parser->types = 0;
          parser->startup_notify = FALSE;
          g_string_truncate (parser->icon_name, 0);
          g_string_truncate (parser->name, 0);
          g_string_truncate (parser->submenu, 0);
          g_string_truncate (parser->unique_id, 0);
          g_string_truncate (parser->command, 0);
          g_string_truncate (parser->patterns, 0);
          g_string_truncate (parser->description, 0);
          xfce_stack_push (parser->stack, PARSER_ACTION);
        }
      else
        {
          xfce_stack_push (parser->stack, PARSER_UNKNOWN_ELEMENT);
        }
      break;

    case PARSER_ACTION:
      if (strcmp (element_name, "name") == 0)
        {
          for (n = 0; attribute_names[n] != NULL; ++n)
            if (strcmp (attribute_names[n], "xml:lang") == 0)
              break;

          if (G_LIKELY (attribute_names[n] == NULL))
            {
              parser->name_use = (parser->name_match <= XFCE_LOCALE_NO_MATCH);
            }
          else
            {
              match = xfce_locale_match (parser->locale, attribute_values[n]);
              if (parser->name_match < match)
                {
                  parser->name_match = match;
                  parser->name_use = TRUE;
                }
              else
                {
                  parser->name_use = FALSE;
                }
            }

          if (parser->name_use)
            g_string_truncate (parser->name, 0);

          xfce_stack_push (parser->stack, PARSER_NAME);
        }
      else if (strcmp (element_name, "submenu") == 0)
        {
          g_string_truncate (parser->submenu, 0);
          xfce_stack_push (parser->stack, PARSER_SUB_FOLDER);
        }
      else if (strcmp (element_name, "unique-id") == 0)
        {
          g_string_truncate (parser->unique_id, 0);
          xfce_stack_push (parser->stack, PARSER_UNIQUE_ID);
        }
      else if (strcmp (element_name, "icon") == 0)
        {
          g_string_truncate (parser->icon_name, 0);
          xfce_stack_push (parser->stack, PARSER_ICON);
        }
      else if (strcmp (element_name, "command") == 0)
        {
          g_string_truncate (parser->command, 0);
          xfce_stack_push (parser->stack, PARSER_COMMAND);
        }
      else if (strcmp (element_name, "patterns") == 0)
        {
          g_string_truncate (parser->patterns, 0);
          xfce_stack_push (parser->stack, PARSER_PATTERNS);
        }
      else if (strcmp (element_name, "description") == 0)
        {
          for (n = 0; attribute_names[n] != NULL; ++n)
            if (strcmp (attribute_names[n], "xml:lang") == 0)
              break;

          if (G_LIKELY (attribute_names[n] == NULL))
            {
              parser->description_use = (parser->description_match <= XFCE_LOCALE_NO_MATCH);
            }
          else
            {
              match = xfce_locale_match (parser->locale, attribute_values[n]);
              if (parser->description_match < match)
                {
                  parser->description_match = match;
                  parser->description_use = TRUE;
                }
              else
                {
                  parser->description_use = FALSE;
                }
            }

          if (parser->description_use)
            g_string_truncate (parser->description, 0);

          xfce_stack_push (parser->stack, PARSER_DESCRIPTION);
        }
      else if (strcmp (element_name, "startup-notify") == 0)
        {
          parser->startup_notify = TRUE;
          xfce_stack_push (parser->stack, PARSER_STARTUP_NOTIFY);
        }
      else if (strcmp (element_name, "directories") == 0)
        {
          parser->types |= THUNAR_UCA_TYPE_DIRECTORIES;
          xfce_stack_push (parser->stack, PARSER_DIRECTORIES);
        }
      else if (strcmp (element_name, "audio-files") == 0)
        {
          parser->types |= THUNAR_UCA_TYPE_AUDIO_FILES;
          xfce_stack_push (parser->stack, PARSER_AUDIO_FILES);
        }
      else if (strcmp (element_name, "image-files") == 0)
        {
          parser->types |= THUNAR_UCA_TYPE_IMAGE_FILES;
          xfce_stack_push (parser->stack, PARSER_IMAGE_FILES);
        }
      else if (strcmp (element_name, "other-files") == 0)
        {
          parser->types |= THUNAR_UCA_TYPE_OTHER_FILES;
          xfce_stack_push (parser->stack, PARSER_OTHER_FILES);
        }
      else if (strcmp (element_name, "text-files") == 0)
        {
          parser->types |= THUNAR_UCA_TYPE_TEXT_FILES;
          xfce_stack_push (parser->stack, PARSER_TEXT_FILES);
        }
      else if (strcmp (element_name, "video-files") == 0)
        {
          parser->types |= THUNAR_UCA_TYPE_VIDEO_FILES;
          xfce_stack_push (parser->stack, PARSER_VIDEO_FILES);
        }
      else
        {
          xfce_stack_push (parser->stack, PARSER_UNKNOWN_ELEMENT);
        }
      break;

    default:
      xfce_stack_push (parser->stack, PARSER_UNKNOWN_ELEMENT);
    }
}



static void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  GtkTreeIter iter =
  {
    0
  };
  ThunarUcaParser *parser = user_data;

  switch (xfce_stack_top (parser->stack))
    {
    case PARSER_START:
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   _("End element handler called while in root context"));
      return;

    case PARSER_ACTIONS:
      if (strcmp (element_name, "actions") != 0)
        goto unknown_element;
      break;

    case PARSER_ACTION:
      if (strcmp (element_name, "action") == 0)
        {
          if (thunar_uca_model_action_exists (parser->model, &iter, parser->name->str))
            thunar_uca_model_remove (parser->model, &iter);

          thunar_uca_model_append (parser->model, &iter);
          thunar_uca_model_update (parser->model,
                                   &iter,
                                   parser->filename,
                                   parser->name->str,
                                   parser->submenu->str,
                                   parser->unique_id->str,
                                   parser->description->str,
                                   parser->icon_name->str,
                                   parser->command->str,
                                   parser->startup_notify,
                                   parser->patterns->str,
                                   parser->types,
                                   0, 0);
        }
      else
        {
          goto unknown_element;
        }
      break;

    case PARSER_NAME:
      if (strcmp (element_name, "name") != 0)
        goto unknown_element;
      break;

    case PARSER_SUB_FOLDER:
      if (strcmp (element_name, "submenu") != 0)
        goto unknown_element;
      break;

    case PARSER_UNIQUE_ID:
      if (strcmp (element_name, "unique-id") != 0)
        goto unknown_element;
      break;

    case PARSER_ICON:
      if (strcmp (element_name, "icon") != 0)
        goto unknown_element;
      break;

    case PARSER_COMMAND:
      if (strcmp (element_name, "command") != 0)
        goto unknown_element;
      break;

    case PARSER_PATTERNS:
      if (strcmp (element_name, "patterns") != 0)
        goto unknown_element;
      break;

    case PARSER_DESCRIPTION:
      if (strcmp (element_name, "description") != 0)
        goto unknown_element;
      break;

    case PARSER_STARTUP_NOTIFY:
      if (strcmp (element_name, "startup-notify") != 0)
        goto unknown_element;
      break;

    case PARSER_DIRECTORIES:
      if (strcmp (element_name, "directories") != 0)
        goto unknown_element;
      break;

    case PARSER_AUDIO_FILES:
      if (strcmp (element_name, "audio-files") != 0)
        goto unknown_element;
      break;

    case PARSER_IMAGE_FILES:
      if (strcmp (element_name, "image-files") != 0)
        goto unknown_element;
      break;

    case PARSER_OTHER_FILES:
      if (strcmp (element_name, "other-files") != 0)
        goto unknown_element;
      break;

    case PARSER_TEXT_FILES:
      if (strcmp (element_name, "text-files") != 0)
        goto unknown_element;
      break;

    case PARSER_VIDEO_FILES:
      if (strcmp (element_name, "video-files") != 0)
        goto unknown_element;
      break;

    case PARSER_UNKNOWN_ELEMENT: g_warning ("Unknown element ignored: <%s>", element_name);
      break;

    default: goto unknown_element;
    }

  xfce_stack_pop (parser->stack);
  return;

unknown_element:
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
               _("Unknown closing element <%s>"), element_name);
}



static void
text_handler (GMarkupParseContext   *context,
              const gchar           *text,
              gsize                  text_len,
              gpointer               user_data,
              GError               **error)
{
  ThunarUcaParser *parser = user_data;

  switch (xfce_stack_top (parser->stack))
    {
    case PARSER_NAME:
      if (parser->name_use)
        g_string_append_len (parser->name, text, text_len);
      break;

    case PARSER_SUB_FOLDER:
      g_string_append_len (parser->submenu, text, text_len);
      break;

    case PARSER_UNIQUE_ID:
      g_string_append_len (parser->unique_id, text, text_len);
      break;

    case PARSER_ICON:
      g_string_append_len (parser->icon_name, text, text_len);
      break;

    case PARSER_COMMAND:
      g_string_append_len (parser->command, text, text_len);
      break;

    case PARSER_PATTERNS:
      g_string_append_len (parser->patterns, text, text_len);
      break;

    case PARSER_DESCRIPTION:
      if (parser->description_use)
        g_string_append_len (parser->description, text, text_len);
      break;

    default:
      break;
    }
}



gboolean
thunar_uca_parser_read_uca_from_file (ThunarUcaModel  *model,
                                      const gchar     *filename,
                                      GError         **error)
{
  ThunarUcaParser      parser;
  gchar               *content;
  gsize                content_len;
  GMarkupParseContext *context;
  gboolean             succeed;

  g_return_val_if_fail (THUNAR_UCA_IS_MODEL (model), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_path_is_absolute (filename), FALSE);

  /* read the file info memory */
  if (!g_file_get_contents (filename, &content, &content_len, error))
    return FALSE;

  thunar_uca_parser_init (&parser);
  parser.model = model;
  parser.filename = filename;
  xfce_stack_push (parser.stack, PARSER_START);

  /* parse the file */
  context = g_markup_parse_context_new (&markup_parser, 0, &parser, (GDestroyNotify) thunar_uca_parser_deinit);
  succeed = g_markup_parse_context_parse (context, content, content_len, error)
            && g_markup_parse_context_end_parse (context, error);

  /* cleanup */
  g_markup_parse_context_free (context);
  g_free (content);

  return succeed;
}
