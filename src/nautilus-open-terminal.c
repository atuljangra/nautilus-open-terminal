/*
 *  nautilus-open-terminal.c
 * 
 *  Copyright (C) 2004, 2005 Free Software Foundation, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Christian Neumair <chris@gnome-de.org>
 * 
 */

#ifdef HAVE_CONFIG_H
 #include <config.h> /* for GETTEXT_PACKAGE */
#endif

#include "nautilus-open-terminal.h"

#include <libnautilus-extension/nautilus-menu-provider.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h> /* for strcmp */
#include <unistd.h> /* for chdir */
#include <sys/stat.h>

#define SSH_DEFAULT_PORT 22

static void nautilus_open_terminal_instance_init (NautilusOpenTerminal      *cvs);
static void nautilus_open_terminal_class_init    (NautilusOpenTerminalClass *class);

static GType terminal_type = 0;

typedef enum {
	FILE_INFO_LOCAL,
	FILE_INFO_DESKTOP,
	FILE_INFO_SFTP,
	FILE_INFO_OTHER
} TerminalFileInfo;

static TerminalFileInfo
get_terminal_file_info (NautilusFileInfo *file_info)
{
	TerminalFileInfo  ret;
	char             *uri_scheme, *p;

	g_assert (file_info);

	uri_scheme = nautilus_file_info_get_activation_uri (file_info);
	if ((p = strchr (uri_scheme, ':'))) {
		*p = 0;
	}

	if (strcmp (uri_scheme, "file") == 0) {
		ret = FILE_INFO_LOCAL;
	} else if (strcmp (uri_scheme, "x-nautilus-desktop") == 0) {
		ret = FILE_INFO_DESKTOP;
	} else if (strcmp (uri_scheme, "sftp") == 0 ||
		   strcmp (uri_scheme, "ssh") == 0) {
		ret = FILE_INFO_SFTP;
	} else {
		ret = FILE_INFO_OTHER;
	}

	g_free (uri_scheme);

	return ret;
}

char *
lookup_in_data_dir (const char *basename,
                    const char *data_dir)
{
	char *path;

	path = g_build_filename (data_dir, basename, NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);
		return NULL;
	}

	return path;
}

static char *
lookup_in_data_dirs (const char *basename)
{
	const char * const *system_data_dirs;
	const char          *user_data_dir;
	char                *retval;
	int                  i;

	user_data_dir    = g_get_user_data_dir ();
	system_data_dirs = g_get_system_data_dirs ();

	if ((retval = lookup_in_data_dir (basename, user_data_dir))) {
		return retval;
	}

	for (i = 0; system_data_dirs[i]; i++) {
		if ((retval = lookup_in_data_dir (basename, system_data_dirs[i])))
			return retval;
	}

	return NULL;
}

static inline gboolean
desktop_opens_home_dir (GConfClient *client)
{
	return gconf_client_get_bool (client, "/apps/nautilus-open-terminal/desktop_opens_home_dir", NULL);
}

static inline gboolean
desktop_is_home_dir (GConfClient *client)
{
	return gconf_client_get_bool (client, "/apps/nautilus/preferences/desktop_is_home_dir", NULL);
}

#ifdef HAVE_GLIB_DESKTOP_DIR_API
  #define get_desktop_dir() g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP))
#else
  #define get_desktop_dir() g_build_filename (g_get_home_dir (), "Desktop", NULL)
#endif

static void
append_sftp_info (char **terminal_exec,
		  NautilusFileInfo *file_info)
{
	GnomeVFSURI *vfs_uri;
	const char *host_name, *path, *user_name;
	char *uri, *user_host, *cmd, *quoted_cmd, *unescaped_path;
	guint host_port;

	g_assert (terminal_exec != NULL);
	g_assert (file_info != NULL);

	uri = nautilus_file_info_get_activation_uri (file_info);
	g_assert (uri != NULL);
	g_assert (strncmp (uri, "sftp", strlen ("sftp")) == 0 ||
		  strncmp (uri, "ssh", strlen ("ssh")) == 0);

	vfs_uri = gnome_vfs_uri_new (uri);
	g_assert (vfs_uri != NULL);

	host_name = gnome_vfs_uri_get_host_name (vfs_uri);
	host_port = gnome_vfs_uri_get_host_port (vfs_uri);
	user_name = gnome_vfs_uri_get_user_name (vfs_uri);
	path = gnome_vfs_uri_get_path (vfs_uri);
	/* FIXME to we have to consider the remote file encoding? */
	unescaped_path = gnome_vfs_unescape_string (path, NULL);

	if (host_port == 0) {
		host_port = SSH_DEFAULT_PORT;
	}

	if (user_name != NULL) {
		user_host = g_strdup_printf ("%s@%s", user_name, host_name);
	} else {
		user_host = g_strdup (host_name);
	}

	cmd = g_strdup_printf ("ssh %s -p %d -t \"cd \'%s\' && $SHELL -l\"", user_host, host_port, unescaped_path);
	quoted_cmd = g_shell_quote (cmd);
	g_free (cmd);

	*terminal_exec = g_realloc (*terminal_exec, strlen (*terminal_exec) + strlen (quoted_cmd) + 4 + 1);
	strcpy (*terminal_exec + strlen (*terminal_exec), " -e ");
	strcpy (*terminal_exec + strlen (*terminal_exec), quoted_cmd);

	g_free (quoted_cmd);
	g_free (user_host);
	g_free (unescaped_path);
	g_free (uri);
	gnome_vfs_uri_unref (vfs_uri);
}

static void
open_terminal_callback (NautilusMenuItem *item,
			NautilusFileInfo *file_info)
{
	gchar *display_str;
	const gchar *old_display_str;
	gchar *uri;
	gchar **argv, *terminal_exec;
	gchar *working_directory;
	gchar *dfile;
	GnomeDesktopItem *ditem;
	GdkScreen *screen;
	static GConfClient *client;

	client = gconf_client_get_default ();

	terminal_exec = gconf_client_get_string (client,
						 "/desktop/gnome/applications/terminal/"
						 "exec",
						 NULL);

	if (terminal_exec == NULL || strlen (terminal_exec) == 0) {
		g_free (terminal_exec);
		terminal_exec = g_strdup ("gnome-terminal");
	}

	switch (get_terminal_file_info (file_info)) {
		case FILE_INFO_LOCAL:
			uri = nautilus_file_info_get_activation_uri (file_info);
			if (uri != NULL) {
				working_directory = g_filename_from_uri (uri, NULL, NULL);
			} else {
				working_directory = g_strdup (g_get_home_dir ());
			}
			g_free (uri);
			break;

		case FILE_INFO_DESKTOP:
			if (desktop_is_home_dir (client) || desktop_opens_home_dir (client)) {
				working_directory = g_strdup (g_get_home_dir ());
			} else {
				working_directory = get_desktop_dir ();
			}
			break;

		case FILE_INFO_SFTP:
			working_directory = NULL;
			append_sftp_info (&terminal_exec, file_info);
			break;

		case FILE_INFO_OTHER:
		default:
			g_assert_not_reached ();
	}

	if (g_str_has_prefix (terminal_exec, "gnome-terminal")) {
		dfile = lookup_in_data_dirs ("applications/gnome-terminal.desktop");
	} else {
		dfile = NULL;
	}

	g_shell_parse_argv (terminal_exec, NULL, &argv, NULL);

	display_str = NULL;
	old_display_str = g_getenv ("DISPLAY");

	screen = g_object_get_data (G_OBJECT (item), "NautilusOpenTerminal::screen");
	if (screen != NULL) {
		display_str = gdk_screen_make_display_name (screen);
		g_setenv ("DISPLAY", display_str, TRUE);
	}

	if (dfile != NULL) {
		int orig_cwd = -1;

		do {
			orig_cwd = open (".", O_RDONLY);
		} while (orig_cwd == -1 && errno == EINTR);

		if (orig_cwd == -1) {
			g_message ("NautilusOpenTerminal: Failed to open current Nautilus working directory.");
		} else if (working_directory != NULL) {

			if (chdir (working_directory) == -1) {
				int ret;

				g_message ("NautilusOpenTerminal: Failed to change Nautilus working directory to \"%s\".",
					   working_directory);

				do {
					ret = close (orig_cwd);
				} while (ret == -1 && errno == EINTR);

				if (ret == -1) {
					g_message ("NautilusOpenTerminal: Failed to close() current Nautilus working directory.");
				}

				orig_cwd = -1;
			}
		}

		ditem = gnome_desktop_item_new_from_file (dfile, 0, NULL);

		gnome_desktop_item_set_string (ditem, "Exec", terminal_exec);
		if (gtk_get_current_event_time () > 0) {
			gnome_desktop_item_set_launch_time (ditem, gtk_get_current_event_time ());
		}
		gnome_desktop_item_launch (ditem, NULL, GNOME_DESKTOP_ITEM_LAUNCH_USE_CURRENT_DIR, NULL);
		gnome_desktop_item_unref (ditem);
		g_free (dfile);

		if (orig_cwd != -1) {
			int ret;

			ret = fchdir (orig_cwd);
			if (ret == -1) {
				g_message ("NautilusOpenTerminal: Failed to change back Nautilus working directory to original location after changing it to \"%s\".",
					   working_directory);
			}

			do {
				ret = close (orig_cwd);
			} while (ret == -1 && errno == EINTR);

			if (ret == -1) {
				g_message ("NautilusOpenTerminal: Failed to close Nautilus working directory.");
			}
		}
	} else {	
		g_spawn_async (working_directory,
			       argv,
			       NULL,
			       G_SPAWN_SEARCH_PATH,
			       NULL,
			       NULL,
			       NULL,
			       NULL);
	}

	g_setenv ("DISPLAY", old_display_str, TRUE);

	g_strfreev (argv);
	g_free (terminal_exec);
	g_free (working_directory);
	g_free (display_str);
}

static NautilusMenuItem *
open_terminal_menu_item_new (TerminalFileInfo  terminal_file_info,
			     GdkScreen        *screen,
			     gboolean          is_file_item)
{
	NautilusMenuItem *ret;
	const char *name;
	const char *tooltip;

	switch (terminal_file_info) {
		case FILE_INFO_LOCAL:
		case FILE_INFO_SFTP:
			name = _("Open In _Terminal");
			if (is_file_item) {
				tooltip = _("Open the currently selected folder in a terminal");
			} else {
				tooltip = _("Open the currently open folder in a terminal");
			}
			break;

		case FILE_INFO_DESKTOP:
			if (desktop_opens_home_dir (gconf_client_get_default ())) {
				name = _("Open _Terminal");
				tooltip = _("Open a terminal");
			} else {
				name = _("Open In _Terminal");
				tooltip = _("Open the currently open folder in a terminal");
			}
			break;

		case FILE_INFO_OTHER:
		default:
			g_assert_not_reached ();
	}

	ret = nautilus_menu_item_new ("NautilusOpenTerminal::open_terminal",
				      name, tooltip, "gnome-terminal");
	g_object_set_data (G_OBJECT (ret),
			   "NautilusOpenTerminal::screen",
			   screen);

	return ret;
}

static gboolean
terminal_locked_down (void)
{
	return gconf_client_get_bool (gconf_client_get_default (),
                                      "/desktop/gnome/lockdown/disable_command_line",
                                      NULL);
}


static GList *
nautilus_open_terminal_get_background_items (NautilusMenuProvider *provider,
					     GtkWidget		  *window,
					     NautilusFileInfo	  *file_info)
{
	NautilusMenuItem *item;
	TerminalFileInfo  terminal_file_info;

        if (terminal_locked_down ()) {
            return NULL;
        }

	terminal_file_info = get_terminal_file_info (file_info);
	switch (terminal_file_info) {
		case FILE_INFO_LOCAL:
		case FILE_INFO_DESKTOP:
		case FILE_INFO_SFTP:
			item = open_terminal_menu_item_new (terminal_file_info, gtk_widget_get_screen (window), FALSE);
			g_object_set_data_full (G_OBJECT (item), "file-info",
						g_object_ref (file_info),
						(GDestroyNotify) g_object_unref);
			g_signal_connect (item, "activate",
					  G_CALLBACK (open_terminal_callback),
					  file_info);

			return g_list_append (NULL, item);

		case FILE_INFO_OTHER:
			return NULL;

		default:
			g_assert_not_reached ();
	}
}

GList *
nautilus_open_terminal_get_file_items (NautilusMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	NautilusMenuItem *item;
	TerminalFileInfo  terminal_file_info;

        if (terminal_locked_down ()) {
            return NULL;
        }

	if (g_list_length (files) != 1 ||
	    (!nautilus_file_info_is_directory (files->data) &&
	     nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_SHORTCUT &&
	     nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_MOUNTABLE)) {
		return NULL;
	}

	terminal_file_info = get_terminal_file_info (files->data);
	switch (terminal_file_info) {
		case FILE_INFO_LOCAL:
		case FILE_INFO_SFTP:
			item = open_terminal_menu_item_new (terminal_file_info, gtk_widget_get_screen (window), TRUE);
			g_object_set_data_full (G_OBJECT (item), "file-info",
						g_object_ref (files->data),
						(GDestroyNotify) g_object_unref);
			g_signal_connect (item, "activate",
					  G_CALLBACK (open_terminal_callback),
					  files->data);

			return g_list_append (NULL, item);

		case FILE_INFO_DESKTOP:
		case FILE_INFO_OTHER:
			return NULL;

		default:
			g_assert_not_reached ();
	}
}

static void
nautilus_open_terminal_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
	iface->get_background_items = nautilus_open_terminal_get_background_items;
	iface->get_file_items = nautilus_open_terminal_get_file_items;
}

static void 
nautilus_open_terminal_instance_init (NautilusOpenTerminal *cvs)
{
}

static void
nautilus_open_terminal_class_init (NautilusOpenTerminalClass *class)
{
}

GType
nautilus_open_terminal_get_type (void) 
{
	return terminal_type;
}

void
nautilus_open_terminal_register_type (GTypeModule *module)
{
	static const GTypeInfo info = {
		sizeof (NautilusOpenTerminalClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) nautilus_open_terminal_class_init,
		NULL, 
		NULL,
		sizeof (NautilusOpenTerminal),
		0,
		(GInstanceInitFunc) nautilus_open_terminal_instance_init,
	};

	static const GInterfaceInfo menu_provider_iface_info = {
		(GInterfaceInitFunc) nautilus_open_terminal_menu_provider_iface_init,
		NULL,
		NULL
	};

	terminal_type = g_type_module_register_type (module,
						     G_TYPE_OBJECT,
						     "NautilusOpenTerminal",
						     &info, 0);

	g_type_module_add_interface (module,
				     terminal_type,
				     NAUTILUS_TYPE_MENU_PROVIDER,
				     &menu_provider_iface_info);
}
