

#include "thunar/thunar-gio-extensions.h"

#include <glib/gstdio.h>
#include <unistd.h> // for symlink()

/* Use-case taken from here: https://gitlab.xfce.org/xfce/thunar/-/issues/1260 */
static void
test_relative_link (void)
{
  /* generated file structure for testing:
  - tmp/d1/file
  - tmp/d1/d2/
  - tmp/d2 --> d1/d2
  - tmp/d1/d2/file --> ../file
  */

  FILE *file_target;

  /* XXXXXX need to be present for some weird reason (see doc of 'g_dir_make_tmp')*/
  g_autofree gchar *tmpdir = g_dir_make_tmp ("thunar-test-relative-link-XXXXXX", NULL);
  g_autofree gchar *tmpdir_d1 = g_build_filename (tmpdir, "d1", NULL);
  g_autofree gchar *tmpdir_d1_d2 = g_build_filename (tmpdir, "d1", "d2", NULL);
  g_assert_nonnull (tmpdir);
  g_assert_nonnull (tmpdir_d1_d2);

  g_assert_cmpint (g_mkdir_with_parents (tmpdir_d1_d2, 0700), ==, 0);
  g_autofree gchar *target = g_build_filename (tmpdir, "d1", "file", NULL);
  g_autofree gchar *link_d2 = g_build_filename (tmpdir, "d2", NULL);
  g_autofree gchar *link_d2_file = g_build_filename (tmpdir, "d2", "file", NULL);
  g_autofree gchar *link_d1_d2_file = g_build_filename (tmpdir_d1_d2, "file", NULL);

  file_target = fopen (target, "w");
  g_assert_nonnull (file_target);
  if (file_target)
    fclose (file_target);

  g_assert_cmpint (symlink ("d1/d2", link_d2), ==, 0);
  g_assert_cmpint (symlink ("../file", link_d1_d2_file), ==, 0);

  g_autoptr (GFile) g_file_link = g_file_new_for_path (link_d2_file);
  g_assert_nonnull (g_file_link);

  g_autoptr (GFile) g_file_target = g_file_new_for_path (target);
  g_autoptr (GFile) g_file_link_resolved = thunar_g_file_resolve_symlink (g_file_link);
  g_assert_nonnull (g_file_link_resolved);

  g_assert_cmpstr (g_file_get_path (g_file_target), ==, g_file_get_path (g_file_link_resolved));

  /* Delete testfiles */
  g_remove (link_d2);
  g_remove (link_d1_d2_file);
  g_remove (target);
  g_rmdir (tmpdir_d1);
  g_rmdir (tmpdir_d1_d2);
  g_rmdir (tmpdir);
}



/* E.g. checking for 'G_FILE_TYPE_SYMBOLIC_LINK' seems not to work for desktop files .. so let's have a test for them */
static void
test_desktop_link (void)
{
  FILE *file;

  /* XXXXXX need to be present for some weird reason (see doc of 'g_dir_make_tmp')*/
  g_autofree gchar *tmpdir = g_dir_make_tmp ("thunar-test-desktop-link-XXXXXX", NULL);
  g_assert_nonnull (tmpdir);

  g_autofree gchar *target = g_build_filename (tmpdir, "target.desktop", NULL);
  g_autofree gchar *link = g_build_filename (tmpdir, "link.desktop", NULL);

  /* Create some basic .desktop file */
  file = fopen (target, "w");
  g_assert_nonnull (file);
  if (file)
    {
      fprintf (file,
               "[Desktop Entry]\n"
               "Version=1.0\n"
               "Type=Application\n"
               "Name=My App\n"
               "Exec=/full/path/to/my-app-binary\n"
               "Terminal=false\n");
      fclose (file);
    }

  g_assert_cmpint (symlink (target, link), ==, 0);

  g_autoptr (GFile) g_file_link = g_file_new_for_path (link);
  g_autoptr (GFile) g_file_target = g_file_new_for_path (target);
  g_autoptr (GFile) g_file_link_resolved = thunar_g_file_resolve_symlink (g_file_link);

  g_assert_nonnull (g_file_link_resolved);

  g_assert_cmpstr (g_file_get_path (g_file_target), ==, g_file_get_path (g_file_link_resolved));

  /* Delete testfiles */
  g_remove (link);
  g_remove (target);
  g_rmdir (tmpdir);
}



static void
test_multi_level_link (void)
{
  FILE *file;

  /* XXXXXX need to be present for some weird reason (see doc of 'g_dir_make_tmp')*/
  g_autofree gchar *tmpdir = g_dir_make_tmp ("thunar-test-multi-level-link-XXXXXX", NULL);
  g_assert_nonnull (tmpdir);

  g_autofree gchar *target = g_build_filename (tmpdir, "target", NULL);
  g_autofree gchar *link1 = g_build_filename (tmpdir, "link1", NULL);
  g_autofree gchar *link2 = g_build_filename (tmpdir, "link2", NULL);
  g_autofree gchar *link3 = g_build_filename (tmpdir, "link3", NULL);

  file = fopen (target, "w");
  g_assert_nonnull (file);
  if (file)
    fclose (file);

  g_assert_cmpint (symlink (target, link1), ==, 0);
  g_assert_cmpint (symlink (link1, link2), ==, 0);
  g_assert_cmpint (symlink (link2, link3), ==, 0);

  g_autoptr (GFile) g_file_link = g_file_new_for_path (link3);
  g_autoptr (GFile) g_file_target = g_file_new_for_path (target);
  g_autoptr (GFile) g_file_link_resolved = thunar_g_file_resolve_symlink (g_file_link);

  g_assert_nonnull (g_file_link_resolved);

  g_assert_cmpstr (g_file_get_path (g_file_target), ==, g_file_get_path (g_file_link_resolved));

  /* Delete testfiles */
  g_remove (link1);
  g_remove (link2);
  g_remove (link3);
  g_remove (target);
  g_rmdir (tmpdir);
}



static void
test_link_loop (void)
{
  /* XXXXXX need to be present for some weird reason (see doc of 'g_dir_make_tmp')*/
  g_autofree gchar *tmpdir = g_dir_make_tmp ("thunar-test-link-loop-XXXXXX", NULL);
  g_assert_nonnull (tmpdir);

  g_autofree gchar *link1 = g_build_filename (tmpdir, "link1", NULL);
  g_autofree gchar *link2 = g_build_filename (tmpdir, "link2", NULL);
  g_autofree gchar *link3 = g_build_filename (tmpdir, "link3", NULL);

  g_assert_cmpint (symlink (link1, link2), ==, 0);
  g_assert_cmpint (symlink (link2, link3), ==, 0);
  g_assert_cmpint (symlink (link3, link1), ==, 0);

  g_autoptr (GFile) g_file_link = g_file_new_for_path (link3);
  GFile *g_file_link_resolved = thunar_g_file_resolve_symlink (g_file_link);
  g_assert_null (g_file_link_resolved);

  /* Delete testfiles */
  g_remove (link1);
  g_remove (link2);
  g_remove (link3);
  g_rmdir (tmpdir);
}



int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/resolve-symlinks/test_desktop_link", test_desktop_link);
  g_test_add_func ("/resolve-symlinks/test_multi_level_link", test_multi_level_link);
  g_test_add_func ("/resolve-symlinks/test_relative_link", test_relative_link);
  g_test_add_func ("/resolve-symlinks/test_link_loop", test_link_loop);

  return g_test_run ();
}
