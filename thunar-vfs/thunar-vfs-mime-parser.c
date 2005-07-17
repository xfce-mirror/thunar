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

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <thunar-vfs/thunar-vfs-mime-parser.h>



typedef enum
{
  PARSER_STATE_START,
  PARSER_STATE_MIMETYPE,
  PARSER_STATE_COMMENT,
  PARSER_STATE_UNKNOWN,
} ParserState;
 
typedef XFCE_GENERIC_STACK(ParserState) ParserStateStack;
 
typedef struct
{
  ParserStateStack *stack;
  guint             comment_match;
  gboolean          comment_use;
  GString          *comment;
  const gchar      *locale;
} Parser;
 
 
 
static void         start_element_handler     (GMarkupParseContext  *context,
                                               const gchar          *element_name,
                                               const gchar         **attribute_names,
                                               const gchar         **attribute_values,
                                               gpointer              user_data,
                                               GError              **error);
static void         end_element_handler       (GMarkupParseContext  *context,
                                               const gchar          *element_name,
                                               gpointer              user_data,
                                               GError              **error);
static void         text_handler              (GMarkupParseContext  *context,
                                               const gchar          *text,
                                               gsize                 text_len,
                                               gpointer              user_data,
                                               GError             **error);
 
 
 
static GMarkupParser markup_parser =
{
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL,
};
 
 
 
static void
start_element_handler (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
  Parser *parser = (Parser *) user_data;
  guint   match;
  guint   n;
 
  switch (xfce_stack_top (parser->stack))
    {
    case PARSER_STATE_START:
      if (exo_str_is_equal (element_name, "mime-type"))
        {
          xfce_stack_push (parser->stack, PARSER_STATE_MIMETYPE);
        }
      else
        goto unknown_element;
      break;
 
    case PARSER_STATE_MIMETYPE:
      if (exo_str_is_equal (element_name, "comment"))
        {
          for (n = 0; attribute_names[n] != NULL; ++n)
            if (exo_str_is_equal (attribute_names[n], "xml:lang"))
              break;
 
          if (G_UNLIKELY (attribute_names[n] == NULL))
            {
              parser->comment_use = (parser->comment_match <= XFCE_LOCALE_NO_MATCH);
            }
          else
            {
              match = xfce_locale_match (parser->locale, attribute_values[n]);
              if (parser->comment_match < match)
                {
                  parser->comment_match = match;
                  parser->comment_use = TRUE;
                }
              else
                {
                  parser->comment_use = FALSE;
                }
            }
 
          if (parser->comment_use)
            g_string_truncate (parser->comment, 0);
 
          xfce_stack_push (parser->stack, PARSER_STATE_COMMENT);
        }
      else
        xfce_stack_push (parser->stack, PARSER_STATE_UNKNOWN);
      break;
 
    default:
      xfce_stack_push (parser->stack, PARSER_STATE_UNKNOWN);
      break;
    }
 
  return;
 
unknown_element:
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
               "Unknown element <%s>", element_name);
  return;
}
 
 
 
static void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  Parser *parser = (Parser *) user_data;
 
  switch (xfce_stack_top (parser->stack))
    {
    case PARSER_STATE_START:
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "End element handler called while in root context");
      return;
 
    case PARSER_STATE_MIMETYPE:
      if (!exo_str_is_equal (element_name, "mime-type"))
        goto unknown_element;
      break;
 
    case PARSER_STATE_COMMENT:
      if (!exo_str_is_equal (element_name, "comment"))
        goto unknown_element;
      break;
 
    default:
      break;
    }
 
  xfce_stack_pop (parser->stack);
  return;
 
unknown_element:
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
               "Unknown closing element <%s>", element_name);
  return;
}
 
 
 
static void
text_handler (GMarkupParseContext  *context,
              const gchar          *text,
              gsize                 text_len,
              gpointer              user_data,
              GError             **error)
{
  Parser *parser = (Parser *) user_data;
 
  switch (xfce_stack_top (parser->stack))
    {
    case PARSER_STATE_COMMENT:
      if (parser->comment_use)
        g_string_append_len (parser->comment, text, text_len);
      break;
 
    default:
      break;
    }
}
 
 
 
gchar*
_thunar_vfs_mime_parser_load_comment_from_file (const gchar *filename,
                                                GError     **error)
{
  GMarkupParseContext *context;
  Parser               parser;
  gchar               *content;
  gsize                content_len;
  gboolean             comment_free = TRUE;
  gchar               *comment = NULL;
 
  if (!g_file_get_contents (filename, &content, &content_len, error))
    return NULL;
 
  parser.comment_match = XFCE_LOCALE_NO_MATCH;
  parser.comment = g_string_new ("");
  parser.locale = setlocale (LC_MESSAGES, NULL);
 
  parser.stack = xfce_stack_new (ParserStateStack);
  xfce_stack_push (parser.stack, PARSER_STATE_START);
 
  context = g_markup_parse_context_new (&markup_parser, 0, &parser, NULL);
 
  if (!g_markup_parse_context_parse (context, content, content_len, error))
    goto done;
 
  if (!g_markup_parse_context_end_parse (context, error))
    goto done;
 
  comment = parser.comment->str;
  comment_free = FALSE;
 
done:
  g_markup_parse_context_free (context);
  g_string_free (parser.comment, comment_free);
  xfce_stack_free (parser.stack);
  g_free (content);
 
  return comment;
}
