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

# FIXME: get this from the .pc file
nautilus_extensiondir=$(libdir)/nautilus/extensions-1.0

nautilus_extension_LTLIBRARIES=libnautilus-open-terminal.la

libnautilus_open_terminal_la_SOURCES = \
	nautilus-open-terminal.c       \
	nautilus-open-terminal.h       \
	open-terminal.c

libnautilus_open_terminal_la_LDFLAGS = -module -avoid-version
libnautilus_open_terminal_la_LIBADD  = $(NAUTILUS_LIBS) $(GNOMEVFS_LIBS) $(GCONF_LIBS) $(GNOMEDESKTOP_LIBS)
