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

#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-info-provider.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkwidget.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <string.h> /* for strcmp */

static void nautilus_open_terminal_instance_init            (NautilusOpenTerminal               *cvs);
static void nautilus_open_terminal_class_init               (NautilusOpenTerminalClass          *class);

static GObjectClass *parent_class;

static void
open_terminal_callback (NautilusMenuItem *item,
			NautilusFileInfo *file_info)
{
	gchar **argv, *uri, *terminal_exec;
	gchar *local_uri;
	static GConfClient *client;
	gboolean desktop_is_home_dir = FALSE;

	g_print ("Open Terminal selected\n");

	client = gconf_client_get_default ();

	uri = nautilus_file_info_get_uri (file_info);
	if (uri == NULL)
		local_uri = g_strdup (g_get_home_dir ());
	else if (strcmp (uri, "x-nautilus-desktop:///") == 0) {
		desktop_is_home_dir = gconf_client_get_bool (client,
							     "/apps/nautilus/preferences/"
							     "desktop_is_home_dir",
							     NULL);
		local_uri = desktop_is_home_dir ? g_strdup (g_get_home_dir ()) :
						  g_build_filename (g_get_home_dir (),
								    "Desktop",
								    NULL);
	}
	else
		local_uri = gnome_vfs_get_local_path_from_uri (uri);

	if (local_uri == NULL)
		local_uri = g_strdup (g_get_home_dir ());

	terminal_exec = gconf_client_get_string (client,
						 "/desktop/gnome/applications/terminal/"
						 "exec",
						 NULL);

	if (terminal_exec == NULL)
		terminal_exec = g_strdup ("gnome-terminal");

	g_shell_parse_argv (terminal_exec, NULL, &argv, NULL);

	g_spawn_async (local_uri,
		       argv,
		       NULL,
		       G_SPAWN_SEARCH_PATH,
		       NULL,
		       NULL,
		       NULL,
		       NULL);

	g_free (argv);
	g_free (uri);
	g_free (terminal_exec);
	g_free (local_uri);
}

static gboolean
is_local (NautilusFileInfo *file_info)
{
	gchar    *uri_scheme;
	gboolean  ret;

	g_assert (file_info);

	uri_scheme = nautilus_file_info_get_uri_scheme (file_info);
	ret = (!strcmp (uri_scheme, "file") ||
	       !strcmp (uri_scheme, "x-nautilus-desktop"));

	g_free (uri_scheme);

	return ret;
}

static GList *
nautilus_open_terminal_get_background_items (NautilusMenuProvider *provider,
					     GtkWidget		  *window,
					     NautilusFileInfo	  *file_info)
{
	NautilusMenuItem *item;

	if (!is_local (file_info))
		return NULL;

	item = nautilus_menu_item_new ("NautilusOpenTerminal::open_terminal",
				       _("Open In _Terminal"),
				       _("Open the currently open folder in a terminal"),
				       "gnome-terminal");
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_terminal_callback),
			  file_info);

	return g_list_append (NULL, item);
}

GList *
nautilus_open_terminal_get_file_items (NautilusMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	NautilusMenuItem *item;

	if (g_list_length (files) != 1 ||
	    !nautilus_file_info_is_directory (files->data) ||
	    !is_local (files->data))
		return NULL;

	item = nautilus_menu_item_new ("NautilusOpenTerminal::open_terminal",
				       _("Open In _Terminal"),
				       _("Open the selected folder in a terminal"),
				       "gnome-terminal");
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_terminal_callback),
			  files->data);

	return g_list_append (NULL, item);
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
	parent_class = g_type_class_peek_parent (class);
}

static GType terminal_type = 0;

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
