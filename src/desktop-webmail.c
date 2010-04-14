/*
 *  Copyright (C) 2010  Canonical Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3 of the License,
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "config.h"

#define CACHE_DIR_PATH ".cache/desktop-webmail"
#define CONFIG_DIR_PATH ".config/desktop-webmail"

#ifndef LOCALEDIR
#define LOCALEDIR "po"
#endif

#define WEBMAILER_KEYFILE_NAME "webmailers.ini"
#define WEBMAILER_KEYFILE_PATH_DEFAULT pkgdatadir
#define WEBMAILER_SYS_DIR sysconfpkgdir

#define CONFIG_KEYFILE_NAME "desktop-webmail.ini"
#define CONFIG_KEYFILE_PATH_DEFAULT ".config/desktop-webmail/desktop-webmail.ini"

enum {
	COLUMN_NAME=0,
	COLUMN_ICONURL,
	COLUMN_ACTIONURL,
	COLUMN_PIXBUF,
	COLUMN_INBOXURL,
	COLUMN_END
};

struct _Config
{
	GKeyFile *keyfile;
	gboolean remember;
	gchar *default_provider;
	gchar *default_url;
	gchar *default_inbox;
};

struct _FormWidgets
{
	GtkWidget *combo;
	GtkWidget *checkbutton;
};

static struct _Config *global_config;
static gchar *cache_dir = NULL;
static gchar *config_dir = NULL;
static gchar *config_path = NULL;
static gchar *webmailer_user_path = NULL;
static gchar *webmailer_sys_path = NULL;
static gboolean cancelled = FALSE;


static gboolean
ensure_dirs ()
{
	gchar *home = getenv ("HOME");
	g_assert (home);

	/* ensure_dirs must only be called once */
	g_return_val_if_fail (!cache_dir, FALSE);
	g_return_val_if_fail (!config_dir, FALSE);

	cache_dir = g_strdup_printf ("%s/%s", home, CACHE_DIR_PATH);
	config_dir = g_strdup_printf ("%s/%s", home, CONFIG_DIR_PATH);
	config_path = g_strdup_printf ("%s/%s", config_dir, CONFIG_KEYFILE_NAME);
	webmailer_user_path = g_strdup_printf ("%s/%s", config_dir, WEBMAILER_KEYFILE_NAME);
	webmailer_sys_path = g_strdup_printf ("%s/%s", WEBMAILER_SYS_DIR, WEBMAILER_KEYFILE_NAME);
	mkdir (cache_dir, 0700);
	mkdir (config_dir, 0700);
	return TRUE;
}

static void
load_config_from_ini (struct _Config *config)
{
	GKeyFile *keyfile = g_key_file_new ();

	g_key_file_load_from_file (keyfile, config_path, G_KEY_FILE_KEEP_COMMENTS, NULL);

	if (config->keyfile)
		g_key_file_free(config->keyfile);
	if (config->default_provider)
		g_free(config->default_provider);

	config->keyfile = keyfile;
	config->remember = g_key_file_get_boolean (keyfile, "Config", "remember", NULL);
	config->default_provider = g_key_file_get_string (keyfile, "Config", "default-provider", NULL);
	config->default_url = g_key_file_get_string (keyfile, "Config", "default-url", NULL);
	config->default_inbox = g_key_file_get_string (keyfile, "Config", "default-inbox", NULL);
}

static void
add_keyfile_to_tree_store (GtkTreeStore *store, GKeyFile *keyfile)
{
	gchar **groups;
	gsize groups_len;
	gint i;

	groups = g_key_file_get_groups (keyfile, &groups_len);

	for (i = 0; i < groups_len; i++) {
		GtkTreeIter iter;
		gchar *group_name = g_strdup (groups[i]);
		gchar *icon_url = g_key_file_get_string (keyfile, groups[i], "ICON", NULL);
		gchar *action_url = g_key_file_get_string (keyfile, groups[i], "URL", NULL);
		gchar *inbox_url = g_key_file_get_string (keyfile, groups[i], "INBOX", NULL);
		GdkPixbuf *missing_buf =  gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
		                                                    "desktop-webmail",
		                                                    24,
		                                                    0, NULL);

		if (!icon_url || !action_url || !group_name) {
			g_free (icon_url);
			g_free (action_url);
			g_free (group_name);
			continue;
		}

		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter,
			COLUMN_NAME, group_name,
			COLUMN_ICONURL, icon_url,
			COLUMN_ACTIONURL, action_url,
			COLUMN_PIXBUF, missing_buf,
			COLUMN_INBOXURL, inbox_url,
			-1);

		g_free (icon_url);
		g_free (group_name);
		g_free (action_url);
		g_free (inbox_url);
	}
	g_strfreev (groups);
}

static void
load_model_from_ini (GtkTreeStore *store)
{
	GKeyFile *keyfile = g_key_file_new ();
	GKeyFile *keyfile_user_config = g_key_file_new ();
	GKeyFile *keyfile_system_config = g_key_file_new ();
	const gchar *paths[] = { WEBMAILER_KEYFILE_PATH_DEFAULT, "." };

	g_assert (store);

	textdomain (GETTEXT_PACKAGE);

	g_key_file_load_from_dirs (keyfile, WEBMAILER_KEYFILE_NAME, paths, NULL, G_KEY_FILE_NONE,  NULL);
	add_keyfile_to_tree_store (store, keyfile);
	
	g_key_file_load_from_file (keyfile_user_config, webmailer_user_path, G_KEY_FILE_NONE,  NULL);
	add_keyfile_to_tree_store (store, keyfile_user_config);

	g_key_file_load_from_file (keyfile_system_config, webmailer_sys_path, G_KEY_FILE_NONE,  NULL);
	add_keyfile_to_tree_store (store, keyfile_system_config);

	g_key_file_free (keyfile);
	g_key_file_free (keyfile_user_config);
	g_key_file_free (keyfile_system_config);
}

static GHashTable*
parse_fields_from_mailto (const gchar* mailto)
{
	GHashTable *fields = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
	gchar* parse = g_strdup (mailto + strlen ("mailto:"));
	gchar* start = parse;
	gchar* delim = strstr (start, "?");
	gchar** args, **argi = NULL;
	gint noargs = 0;

	if (delim != NULL)
		*delim = 0;
	else
		noargs = 1;

	g_hash_table_insert (fields, "url", g_strdup (mailto));
	g_hash_table_insert (fields, "to", g_strdup (parse));

	if (noargs)
		goto out;

	start = delim +1;
	args = g_strsplit_set (start, "&#?", 0);
	argi = args;

	while (argi && *argi) {
		gchar *key = *argi;
		gchar *value;

		value = strchr (*argi, '=');
		*value = 0;
		value++;
		g_hash_table_insert (fields, g_ascii_strdown(key, -1), g_strdup (value));

		argi++;
	}

	g_strfreev (args);

out:
	g_free(parse);
	return fields;
}

static gchar*
fill_url_template_from_mailto (const gchar* url_template, const gchar* mailto)
{
	gchar* last = g_strdup (url_template);
	gchar* iter = last;
	gchar* end = last + strlen(url_template);
	gchar* result = g_strdup("");
	GHashTable *fields = parse_fields_from_mailto (mailto);

	while (iter) {
		if (iter == end) {
			gchar* old_result = result;
			result = g_strdup_printf ("%s%s", old_result, last);
			g_free(old_result);
			break;
		} else if (*iter == '%' && iter < end) {
			gchar* old_result = result;
			gchar *replace_key, *replace;
			*iter = 0;
			iter++;
			switch (*iter) {
			case 'j':
				replace_key = "subject";
				break;
			case 'k':
				replace_key = "cc";
				break;
			case 'l':
				replace_key = "bcc";
				break;
			case 'm':
				replace_key = "body";
				break;
			case 's':
				replace_key = "url";
				break;
			case 't':
				replace_key = "to";
				break;
			default:
				replace_key = 0;
				break;
			}
			if (replace_key)
				replace = g_hash_table_lookup (fields, replace_key);
			else
				replace = 0;

			if (replace)
				result = g_strdup_printf ("%s%s%s", old_result, last, replace);
			else
				result = g_strdup_printf ("%s%s", old_result, last);

			iter++;
			last = iter;
			g_free(old_result);
		} else {
			iter++;
		}
	}
	return result;
}

static void
flush_config_data (const gchar *outbuf, gsize len)
{
	GFile *ofile = g_file_new_for_path (config_path);
	GFileOutputStream *ostream = g_file_replace (ofile, NULL, TRUE, G_FILE_CREATE_NONE, NULL, NULL);
	g_output_stream_write (G_OUTPUT_STREAM (ostream), outbuf, len, NULL, NULL);
	g_output_stream_close (G_OUTPUT_STREAM (ostream), NULL, NULL);
	g_object_unref (ostream);
	g_object_unref (ofile);
}

static void
update_mailto_preference (GtkDialog *dialog, struct _FormWidgets *form_widgets)
{
	GtkComboBox *combo = GTK_COMBO_BOX (form_widgets->combo);
	GtkCheckButton *checkbutton = GTK_CHECK_BUTTON (form_widgets->checkbutton);
	GtkTreeModel *model = GTK_TREE_MODEL (gtk_combo_box_get_model (combo));
	GtkTreeIter iter;
	gchar *name = NULL;
	gchar *url = NULL;
	gchar *inbox = NULL;
	gchar *outbuf = NULL;
	gsize len;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	gtk_tree_model_get (model, &iter, COLUMN_NAME, &name, -1);
	gtk_tree_model_get (model, &iter, COLUMN_ACTIONURL, &url, -1);
	gtk_tree_model_get (model, &iter, COLUMN_INBOXURL, &inbox, -1);

	g_key_file_set_boolean  (global_config->keyfile, "Config", "remember", !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
	g_key_file_set_string  (global_config->keyfile, "Config", "default-provider", name);
	g_key_file_set_string  (global_config->keyfile, "Config", "default-url", url);
	g_key_file_set_string  (global_config->keyfile, "Config", "default-inbox", inbox);

	outbuf = g_key_file_to_data (global_config->keyfile, &len, NULL);
	flush_config_data (outbuf, len);

	/* reload config after we flushed it */
	load_config_from_ini (global_config);

	g_free (outbuf);
	g_free (name);
}

static void
dialog_response_cb (GtkDialog *dialog,
                    gint       response_id,
                    gpointer   user_data)
{
	struct _FormWidgets *form_widgets = user_data;

	switch (response_id) {
	case GTK_RESPONSE_ACCEPT:
		update_mailto_preference (dialog, form_widgets);
		break;
	case GTK_RESPONSE_REJECT:
		cancelled = TRUE;
		break;
	default:
		g_return_if_reached ();
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	gtk_main_quit();
}

static gint
compare_str_column_name (GtkTreeModel *model,
                         GtkTreeIter *a,
                         GtkTreeIter *b,
                         gpointer user_data)
{

	gchar *name_a, *name_b;
	gint res;

	gtk_tree_model_get (model, a, COLUMN_NAME, &name_a, -1);
	gtk_tree_model_get (model, b, COLUMN_NAME, &name_b, -1);

	res = g_strcmp0 (name_a, name_b);

	g_free(name_a);
	g_free(name_b);

	return res;	
}

static gboolean
set_active_if_default (GtkTreeModel *model,
                       GtkTreePath *path,
                       GtkTreeIter *iter,
                       gpointer data)
{
	GtkComboBox *box = GTK_COMBO_BOX (data);
	gchar *name = NULL;

	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);

	if (!g_strcmp0 (name, global_config->default_provider)) {
		gtk_combo_box_set_active_iter (box, iter);
		g_free (name);
		return TRUE;
	}
	g_free (name);
	return FALSE;
}


static void
select_default (GtkTreeModel *model, GtkComboBox *box)
{

	gtk_tree_model_foreach (model,
	                        set_active_if_default,
	                        box);
}

static void
run_gtk_config (gint *argcp, gchar*** argvp)
{
	GtkDialog *dialog;
	GtkWidget *combo, *title, *content_box, *title_box, *content2_box, *remember_checkbox;
	GtkTreeStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	struct _FormWidgets *form_widgets = g_new0 (struct _FormWidgets, 1);

	gtk_init (argcp, argvp);

	gtk_window_set_default_icon_name ("desktop-webmail");

	dialog = GTK_DIALOG (gtk_dialog_new_with_buttons (_("Webmail Configuration"), NULL,
	                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	                                                  GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                                                  GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                                                  NULL));
	gtk_box_set_homogeneous (GTK_BOX (gtk_bin_get_child (GTK_BIN (dialog))), FALSE);

	title_box = gtk_hbox_new (FALSE, FALSE);
	content_box = gtk_hbox_new (FALSE, FALSE);
	content2_box = gtk_hbox_new (FALSE, FALSE);

	title = gtk_label_new (_("Select your webmail provider:"));
	gtk_label_set_justify (GTK_LABEL (title), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (title_box), title, TRUE, TRUE, 5);

	store = gtk_tree_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), COLUMN_NAME, compare_str_column_name, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COLUMN_NAME, GTK_SORT_ASCENDING);
	load_model_from_ini (store);

	combo = gtk_combo_box_new ();
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
	select_default (GTK_TREE_MODEL (store), GTK_COMBO_BOX (combo));

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_renderer_set_padding (renderer, 5, 0);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(combo), renderer, "pixbuf", COLUMN_PIXBUF, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(combo), renderer, "text", COLUMN_NAME, NULL);


	gtk_box_pack_start (GTK_BOX (content_box), combo, TRUE, TRUE, 10);

	remember_checkbox = gtk_check_button_new_with_label (_("Ask again"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (remember_checkbox),
	                              !global_config->remember);

	gtk_box_pack_start (GTK_BOX (content2_box), remember_checkbox, FALSE, FALSE, 15);

	gtk_box_pack_start (GTK_BOX (gtk_bin_get_child (GTK_BIN (dialog))), title_box, TRUE, TRUE, 10);
	gtk_box_pack_end (GTK_BOX (gtk_bin_get_child (GTK_BIN (dialog))), content2_box, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (gtk_bin_get_child (GTK_BIN (dialog))), content_box, FALSE, FALSE, 10);

	form_widgets->combo = combo;
	form_widgets->checkbutton = remember_checkbox;

	g_signal_connect (G_OBJECT (dialog), "response", (GCallback) dialog_response_cb, form_widgets);

	gtk_widget_show_all (GTK_WIDGET(dialog));

	gtk_main ();
}

int main (int argc, char **argv)
{
	gchar* url;
	gchar *command;

	/* intialize gettext */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_assert (ensure_dirs ());

	global_config = calloc (sizeof (struct _Config), 1);
	load_config_from_ini (global_config);

	/* in case we dont want to remember or miss one of the configs,
	   we will pop up the config screen; also if no mailto: url was
	   given */
	if (!global_config->remember ||
	    !global_config->default_url ||
	    !global_config->default_inbox ||
	    !global_config->default_provider ||
            (argc == 2 && !g_strcmp0 ("--config", argv[1]))) {
		run_gtk_config (&argc, &argv);
	}

	/* if we have an url (argc > 1); parse, and fire up webmail url using
	   xdg-open */
	if (argc > 1 && g_strcmp0 ("--config", argv[1]) && !cancelled) {
		url = fill_url_template_from_mailto (global_config->default_url, argv[1]);
		command = g_strdup_printf ("xdg-open %s", url);
		printf ("running: %s\n", command);
		g_spawn_command_line_async (command, NULL);
	}

	if (argc == 1) {
		command = g_strdup_printf ("xdg-open %s", global_config->default_inbox);
		printf ("running: %s\n", command);
		g_spawn_command_line_async (command, NULL);
	}
	
	return 0;
}

