/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#include <gmodule.h>

#include <exo/exo.h>

#include <thunar/thunar-extension-manager.h>



#define THUNAR_EXTENSIONS_DIRECTORY (LIBDIR G_DIR_SEPARATOR_S "thunarx-" THUNAR_VERSION_API)



enum
{
  PROP_0,
  PROP_RESIDENT,
};



typedef struct _ThunarExtensionClass ThunarExtensionClass;
typedef struct _ThunarExtension      ThunarExtension;



#define THUNAR_TYPE_EXTENSION             (thunar_extension_get_type ())
#define THUNAR_EXTENSION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_EXTENSION, ThunarExtension))
#define THUNAR_EXTENSION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_EXTENSION, ThunarExtensionClass))
#define THUNAR_IS_EXTENSION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_EXTENSION))
#define THUNAR_IS_EXTENSION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_EXTENSION))
#define THUNAR_EXTENSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_EXTENSION, ThunarExtensionClass))



static GType    thunar_extension_get_type     (void) G_GNUC_CONST;
static void     thunar_extension_class_init   (ThunarExtensionClass *klass);
static void     thunar_extension_finalize     (GObject              *object);
static void     thunar_extension_get_property (GObject              *object,
                                               guint                 prop_id,
                                               GValue               *value,
                                               GParamSpec           *pspec);
static void     thunar_extension_set_property (GObject              *object,
                                               guint                 prop_id,
                                               const GValue         *value,
                                               GParamSpec           *pspec);
static gboolean thunar_extension_load         (GTypeModule          *module);
static void     thunar_extension_unload       (GTypeModule          *module);
static void     thunar_extension_list_types   (ThunarExtension      *extension,
                                               const GType         **types,
                                               gint                 *n_types);



struct _ThunarExtensionClass
{
  GTypeModuleClass __parent__;
};

struct _ThunarExtension
{
  GTypeModule __parent__;

  gchar   *path;
  GModule *module;
  gboolean resident;

  void (*initialize)  (GTypeModule  *module);
  void (*shutdown)    (void);
  void (*list_types)  (const GType **types,
                       gint         *n_types);
};



static GObjectClass *thunar_extension_parent_class;



static GType
thunar_extension_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarExtensionClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_extension_class_init,
        NULL,
        NULL,
        sizeof (ThunarExtension),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_TYPE_MODULE, "ThunarExtension", &info, 0);
    }

  return type;
}



static void
thunar_extension_class_init (ThunarExtensionClass *klass)
{
  GTypeModuleClass *gtype_module_class;
  GObjectClass     *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_extension_finalize;
  gobject_class->get_property = thunar_extension_get_property;
  gobject_class->set_property = thunar_extension_set_property;

  gtype_module_class = G_TYPE_MODULE_CLASS (klass);
  gtype_module_class->load = thunar_extension_load;
  gtype_module_class->unload = thunar_extension_unload;

  /**
   * ThunarExtension::resident:
   *
   * Tells whether the extension must reside in memory once loaded
   * for the first time.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_RESIDENT,
                                   g_param_spec_boolean ("resident",
                                                         _("Resident"),
                                                         _("Ensures that an extension will never be unloaded."),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_extension_finalize (GObject *object)
{
  ThunarExtension *extension = THUNAR_EXTENSION (object);

  /* free the path to the extension */
  g_free (extension->path);

  (*G_OBJECT_CLASS (thunar_extension_parent_class)->finalize) (object);
}



static void
thunar_extension_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ThunarExtension *extension = THUNAR_EXTENSION (object);

  switch (prop_id)
    {
    case PROP_RESIDENT:
      g_value_set_boolean (value, extension->resident);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_extension_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ThunarExtension *extension = THUNAR_EXTENSION (object);

  switch (prop_id)
    {
    case PROP_RESIDENT:
      extension->resident = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_extension_load (GTypeModule *module)
{
  ThunarExtension *extension = THUNAR_EXTENSION (module);

  /* load the extension using the runtime link editor */
  extension->module = g_module_open (extension->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  /* check if the load operation was successful */
  if (G_UNLIKELY (extension->module == NULL))
    {
      g_warning ("Failed to load file manager extension from %s: %s", extension->path, g_module_error ());
      return FALSE;
    }

  /* verify that all required public symbols are present in the extension's symbol table */
  if (!g_module_symbol (extension->module, "thunar_extension_shutdown", (gpointer) &extension->shutdown)
      || !g_module_symbol (extension->module, "thunar_extension_initialize", (gpointer) &extension->initialize)
      || !g_module_symbol (extension->module, "thunar_extension_list_types", (gpointer) &extension->list_types))
    {
      g_warning ("File manager extension loaded from %s lacks required symbols.", extension->path);
      g_module_close (extension->module);
      return FALSE;
    }

  /* initialize the extension */
  (*extension->initialize) (module);

  /* ensure that the module will never be unloaded
   * if the extension requests to be kept in memory
   */
  if (G_UNLIKELY (extension->resident))
    g_module_make_resident (extension->module);

  return TRUE;
}



static void
thunar_extension_unload (GTypeModule *module)
{
  ThunarExtension *extension = THUNAR_EXTENSION (module);

  /* shutdown the extension */
  (*extension->shutdown) ();

  /* unload the extension from memory */
  g_module_close (extension->module);

  /* reset extension state */
  extension->module = NULL;
  extension->shutdown = NULL;
  extension->initialize = NULL;
  extension->list_types = NULL;
}



static void
thunar_extension_list_types (ThunarExtension *extension,
                             const GType    **types,
                             gint            *n_types)
{
  g_return_if_fail (THUNAR_IS_EXTENSION (extension));
  g_return_if_fail (n_types != NULL);
  g_return_if_fail (types != NULL);

  (*extension->list_types) (types, n_types);
}




static void thunar_extension_manager_class_init (ThunarExtensionManagerClass *klass);
static void thunar_extension_manager_init       (ThunarExtensionManager      *manager);
static void thunar_extension_manager_finalize   (GObject                     *object);
static void thunar_extension_manager_add        (ThunarExtensionManager      *manager,
                                                 ThunarExtension             *extension);



struct _ThunarExtensionManagerClass
{
  GObjectClass __parent__;
};

struct _ThunarExtensionManager
{
  GObject __parent__;

  GList *extensions;

  GType *types;
  gint   n_types;
};



G_DEFINE_TYPE (ThunarExtensionManager, thunar_extension_manager, G_TYPE_OBJECT);



static void
thunar_extension_manager_class_init (ThunarExtensionManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_extension_manager_finalize;
}



static void
thunar_extension_manager_init (ThunarExtensionManager *manager)
{
  ThunarExtension *extension;
  const gchar     *name;
  GDir            *dp;

  // FIXME: Perform this in a timer as the extensions aren't required to
  // be available instantly, maybe even load the files in a separate thread!
  dp = g_dir_open (THUNAR_EXTENSIONS_DIRECTORY, 0, NULL);
  if (G_LIKELY (dp != NULL))
    {
      /* determine the types for all existing extensions */
      for (;;)
        {
          /* read the next entry from the directory */
          name = g_dir_read_name (dp);
          if (G_UNLIKELY (name == NULL))
            break;

          /* check if this is a valid extension file */
          if (g_str_has_suffix (name, "." G_MODULE_SUFFIX))
            {
              /* generate an extension object for the file */
              extension = g_object_new (THUNAR_TYPE_EXTENSION, NULL);
              extension->path = g_build_filename (THUNAR_EXTENSIONS_DIRECTORY, name, NULL);

              /* try to load the extension */
              if (g_type_module_use (G_TYPE_MODULE (extension)))
                {
                  /* add the extension to our list of managed extensions */
                  thunar_extension_manager_add (manager, extension);

                  /* don't unuse the type plugin if it should be resident in memory */
                  if (G_LIKELY (!extension->resident))
                    g_type_module_unuse (G_TYPE_MODULE (extension));
                }

              /* drop the reference on the extension */
              g_object_unref (G_OBJECT (extension));
            }
        }

      g_dir_close (dp);
    }
}



static void
thunar_extension_manager_finalize (GObject *object)
{
  ThunarExtensionManager *manager = THUNAR_EXTENSION_MANAGER (object);

  /* release the extensions */
  g_list_foreach (manager->extensions, (GFunc) g_object_unref, NULL);
  g_list_free (manager->extensions);

  /* free the types list */
  g_free (manager->types);

  (*G_OBJECT_CLASS (thunar_extension_manager_parent_class)->finalize) (object);
}



static void
thunar_extension_manager_add (ThunarExtensionManager *manager,
                              ThunarExtension        *extension)
{
  const GType *types;
  gint         n_types;

  /* add the extension to our internal list */
  manager->extensions = g_list_prepend (manager->extensions, g_object_ref (G_OBJECT (extension)));

  /* determines the types provided by the extension */
  thunar_extension_list_types (extension, &types, &n_types);

  /* add the types provided by the extension */
  manager->types = g_realloc (manager->types, sizeof (GType) * (manager->n_types + n_types));
  for (; n_types-- > 0; ++types)
    manager->types[manager->n_types++] = *types;
}



/**
 * thunar_extension_manager_get_default:
 *
 * Returns a reference to the default #ThunarExtensionManager
 * instance.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: a reference to the default
 *               #ThunarExtensionManager instance.
 **/
ThunarExtensionManager*
thunar_extension_manager_get_default (void)
{
  static ThunarExtensionManager *manager = NULL;

  if (G_UNLIKELY (manager == NULL))
    manager = g_object_new (THUNAR_TYPE_EXTENSION_MANAGER, NULL);

  g_object_ref (G_OBJECT (manager));

  return manager;
}



/**
 * thunar_extension_manager_list_providers:
 * @manager : a #ThunarExtensionManager instance.
 * @type    : the provider #GType.
 *
 * Returns all providers of the given @type.
 *
 * The caller is responsible to release the returned
 * list of providers using code like this:
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: the of providers for @type.
 **/
GList*
thunar_extension_manager_list_providers (ThunarExtensionManager *manager,
                                         GType                   type)
{
  GObject *provider;
  GList   *providers = NULL;
  GType   *types;
  gint     n;

  for (n = manager->n_types, types = manager->types; n > 0; --n, ++types)
    if (G_LIKELY (g_type_is_a (*types, type)))
      {
        provider = g_object_new (*types, NULL);
        if (G_LIKELY (provider != NULL))
          providers = g_list_append (providers, provider);
      }

  return providers;
}



