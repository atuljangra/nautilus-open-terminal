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
#include <gconf/gconf-client.h>

#include <string.h> /* for strcmp */

static void nautilus_open_terminal_instance_init (NautilusOpenTerminal      *cvs);
static void nautilus_open_terminal_class_init    (NautilusOpenTerminalClass *class);

static GType terminal_type = 0;

typedef int TerminalFileInfo;

enum {
	FILE_INFO_IS_LOCAL   = 1 << 1,
	FILE_INFO_IS_DESKTOP = 1 << 2
};

static TerminalFileInfo
get_terminal_file_info (NautilusFileInfo *file_info)
{
	TerminalFileInfo  ret;
	gchar            *uri_scheme;

	g_assert (file_info);

	uri_scheme = nautilus_file_info_get_uri_scheme (file_info);

	if (!strcmp (uri_scheme, "file"))
		ret |= FILE_INFO_IS_LOCAL;
	else if (!strcmp (uri_scheme, "x-nautilus-desktop"))
		ret |= FILE_INFO_IS_LOCAL | FILE_INFO_IS_DESKTOP;
	else
		ret = 0;

	g_free (uri_scheme);

	return ret;
}

static void
open_terminal_callback (NautilusMenuItem *item,
			NautilusFileInfo *file_info)
{
	gchar **argv, *terminal_exec;
	gchar *working_directory;
	static GConfClient *client;

	TerminalFileInfo terminal_file_info;

	g_print ("Open Terminal selected\n");

	terminal_file_info = get_terminal_file_info (file_info);

	g_assert (terminal_file_info & FILE_INFO_IS_LOCAL);

	if (terminal_file_info & FILE_INFO_IS_DESKTOP)
		working_directory = g_strdup (g_get_home_dir ());
	else {
		gchar *uri;

		uri = nautilus_file_info_get_uri (file_info);

		working_directory = uri ?
			g_filename_from_uri (uri, NULL, NULL) :
			g_strdup (g_get_home_dir ());

		g_free (uri);
	}


	client = gconf_client_get_default ();

	terminal_exec = gconf_client_get_string (client,
						 "/desktop/gnome/applications/terminal/"
						 "exec",
						 NULL);

	if (!terminal_exec)
		terminal_exec = g_strdup ("gnome-terminal");

	g_shell_parse_argv (terminal_exec, NULL, &argv, NULL);

	g_spawn_async (working_directory,
		       argv,
		       NULL,
		       G_SPAWN_SEARCH_PATH,
		       NULL,
		       NULL,
		       NULL,
		       NULL);

	g_free (argv);
	g_free (terminal_exec);
	g_free (working_directory);
}

static GList *
nautilus_open_terminal_get_background_items (NautilusMenuProvider *provider,
					     GtkWidget		  *window,
					     NautilusFileInfo	  *file_info)
{
	NautilusMenuItem *item;
	gboolean          is_desktop;
	TerminalFileInfo  terminal_file_info;

	terminal_file_info = get_terminal_file_info (file_info);

	if (!(terminal_file_info & FILE_INFO_IS_LOCAL))
		return NULL;

	item = nautilus_menu_item_new ("NautilusOpenTerminal::open_terminal",
				       (terminal_file_info & FILE_INFO_IS_DESKTOP) ?
				        _("Open _Terminal") :
					_("Open In _Terminal"),
				       (terminal_file_info & FILE_INFO_IS_DESKTOP) ?
				        _("Open a terminal") :
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
	TerminalFileInfo  terminal_file_info;

	if (g_list_length (files) != 1 ||
	    !nautilus_file_info_is_directory (files->data))
		return NULL;

	if (!(get_terminal_file_info (files->data) & FILE_INFO_IS_LOCAL))
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
