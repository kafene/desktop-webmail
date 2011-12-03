
EXTRA_DIST = \
	desktop-webmail.desktop.in \
	desktop-webmail-config.desktop.in \
	webmailers.ini \
	desktop-webmail.xml \
	desktop-webmail-config.svg \
	desktop-webmail.svg \
	$(NULL)

pkgdata_DATA = \
	webmailers.ini \
	$(NULL)

defaultappsdir = ${datadir}/gnome-control-center/default-apps/
defaultapps_DATA = \
	desktop-webmail.xml \
	$(NULL)

scalableiconsdir = ${datadir}/icons/gnome/scalable/apps/
scalableicons_DATA = \
	desktop-webmail-config.svg \
	desktop-webmail.svg \
	$(NULL)

appsdir = ${datadir}/applications/
apps_in_files = desktop-webmail.desktop.in desktop-webmail-config.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

