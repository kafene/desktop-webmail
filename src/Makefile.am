
bin_PROGRAMS = desktop-webmail

AM_CFLAGS = \
	-Wall \
	-Werror \
	-pedantic

desktop_webmail_SOURCE = desktop-webmail.c
desktop_webmail_CFLAGS = $(AM_CFLAGS) $(CFLAGS) $(UI_CFLAGS) -Dpkgdatadir=\"$(pkgdatadir)\" -Dsysconfpkgdir=\"$(sysconfdir)/desktop-webmail\"
desktop_webmail_LDADD = $(UI_LIBS)

