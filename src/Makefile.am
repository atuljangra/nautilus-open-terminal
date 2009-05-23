INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Open-Terminal\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(WARN_CFLAGS)                                  \
	-Werror                                         \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(NAUTILUS_CFLAGS)				\
	$(GCONF_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)

nautilus_extensiondir=$(NAUTILUS_EXTENSION_DIR)

nautilus_extension_LTLIBRARIES=libnautilus-open-terminal.la

libnautilus_open_terminal_la_SOURCES = \
	eel-gnome-extensions.c       \
	eel-gnome-extensions.h       \
	nautilus-open-terminal.h       \
	nautilus-open-terminal.c       \
	nautilus-open-terminal.h       \
	open-terminal.c

libnautilus_open_terminal_la_LDFLAGS = -module -avoid-version
libnautilus_open_terminal_la_LIBADD  = $(NAUTILUS_LIBS) $(GCONF_LIBS) $(GNOMEDESKTOP_LIBS)
