/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2026 The Xfce Development Team
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

#include <thunar/thunar-uca-chooser.h>
#include <thunar/thunar-uca-editor.h>
#include <thunar/thunar-uca-model.h>
#include <thunar/thunar-uca-plugin.h>
#include <thunar/thunar-uca-provider.h>

/* This name exactly matches the name of the old version of the uca extension
 * loaded from a shared library, so it will prevent the uca extension from
 * being reloaded from the dynamic library if, for some reason, that library
 * is installed. */
#define THUNAR_UCA_EXTENSION_NAME "thunar-uca." G_MODULE_SUFFIX

static void
thunar_extension_initialize (ThunarxProviderModule *module);
static void
thunar_extension_shutdown (void);
static void
thunar_extension_list_types (const GType **types,
                             gint         *n_types);


static GType                            type_list[1];
static ThunarxProviderModuleMethodTable method_table = {
  .initialize = thunar_extension_initialize,
  .shutdown = thunar_extension_shutdown,
  .list_types = thunar_extension_list_types,
};



static void
thunar_extension_initialize (ThunarxProviderModule *module)
{
  ThunarxProviderPlugin *plugin = THUNARX_PROVIDER_PLUGIN (module);
  const gchar *mismatch;

  /* verify that the thunarx versions are compatible */
  mismatch = thunarx_check_version (THUNARX_MAJOR_VERSION, THUNARX_MINOR_VERSION, THUNARX_MICRO_VERSION);
  if (G_UNLIKELY (mismatch != NULL))
    {
      g_warning ("Version mismatch: %s", mismatch);
      return;
    }

#ifdef G_ENABLE_DEBUG
  g_message ("Initializing internal ThunarUca extension");
#endif

  /* register the types provided by this plugin */
  thunar_uca_chooser_register_type (plugin);
  thunar_uca_editor_register_type (plugin);
  thunar_uca_model_register_type (plugin);
  thunar_uca_provider_register_type (plugin);

  /* setup the plugin provider type list */
  type_list[0] = THUNAR_UCA_TYPE_PROVIDER;
}



static void
thunar_extension_shutdown (void)
{
#ifdef G_ENABLE_DEBUG
  g_message ("Shutting down internal ThunarUca extension");
#endif
}



static void
thunar_extension_list_types (const GType **types,
                             gint         *n_types)
{
  *types = type_list;
  *n_types = G_N_ELEMENTS (type_list);
}


void
thunar_uca_plugin_init (void)
{
  ThunarxProviderModule *module;

  module = thunarx_provider_module_new_internal (THUNAR_UCA_EXTENSION_NAME, &method_table);
  thunarx_provider_factory_add_module (module);
}
