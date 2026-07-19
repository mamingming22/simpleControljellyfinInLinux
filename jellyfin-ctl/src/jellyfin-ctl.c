#include <gtk/gtk.h>
#include <glib.h>
#include <glib-unix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>
#include <libayatana-appindicator/app-indicator.h>

#define LOCK_FILE  "/tmp/jellyfin-ctl.lock"
#define SERVICE    "jellyfin"
#define SYSTEMCTL  "/usr/bin/systemctl"
#define SUDO       "sudo"
#define DEF_PORT   "8096"

static gboolean is_zh = TRUE;

#define _(zh, en) (is_zh ? (zh) : (en))

typedef enum { MODE_SYSTEMD, MODE_FLATPAK, MODE_SNAP } ModeType;

typedef struct {
	GtkWidget     *window;
	GtkWidget     *status_label;
	GtkWidget     *port_entry;
	GtkWidget     *btn_start;
	GtkWidget     *btn_stop;
	GtkWidget     *btn_restart;
	AppIndicator  *indicator;
	GtkWidget     *tray_menu;
	GtkWidget     *tray_start;
	GtkWidget     *tray_stop;
	GtkWidget     *tray_restart;
	GtkWidget     *tray_show;
	GtkWidget     *tray_browser;
	GtkWidget     *tray_quit_and_stop;
	GtkWidget     *tray_quit;
	guint          timer;
	ModeType       mode_type;
	gchar         *mode_id;
	gboolean       native_found;
	gboolean       flatpak_found;
	gchar         *flatpak_id;
	gboolean       snap_found;
	gchar         *snap_id;
	GtkWidget     *mode_combo;
	GtkWidget     *vbox;
	GtkWidget     *top_section;
	GtkWidget     *lang_btn;
	GtkWidget     *lang_label;
	GtkWidget     *port_label;
	GtkWidget     *btn_quit_and_stop;
	GtkWidget     *btn_quit;
	GtkWidget     *btn_browser;
	GtkWidget     *btn_autostart;
	GtkWidget     *tray_autostart;
} App;

static App *app = NULL;

static void run_cmd(const gchar *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	gchar *cmd = g_strdup_vprintf(fmt, ap);
	va_end(ap);
	g_spawn_command_line_async(cmd, NULL);
	g_free(cmd);
}

static void disable_all_buttons(void) {
	gtk_widget_set_sensitive(app->btn_start, FALSE);
	gtk_widget_set_sensitive(app->btn_stop, FALSE);
	gtk_widget_set_sensitive(app->btn_restart, FALSE);
	gtk_widget_set_sensitive(app->tray_start, FALSE);
	gtk_widget_set_sensitive(app->tray_stop, FALSE);
	gtk_widget_set_sensitive(app->tray_restart, FALSE);
	gtk_widget_set_sensitive(app->btn_autostart, FALSE);
	gtk_widget_set_sensitive(app->tray_autostart, FALSE);
}

static void set_buttons_running(gboolean running) {
	gtk_widget_set_sensitive(app->btn_start, !running);
	gtk_widget_set_sensitive(app->btn_stop, running);
	gtk_widget_set_sensitive(app->btn_restart, running);
	gtk_widget_set_sensitive(app->tray_start, !running);
	gtk_widget_set_sensitive(app->tray_stop, running);
	gtk_widget_set_sensitive(app->tray_restart, running);
}

static gboolean is_autostart_enabled(void) {
	switch (app->mode_type) {
	case MODE_SYSTEMD: {
		gchar *out = NULL;
		if (g_spawn_command_line_sync(SYSTEMCTL " is-enabled " SERVICE,
		                              &out, NULL, NULL, NULL) && out) {
			g_strstrip(out);
			gboolean en = (g_strcmp0(out, "enabled") == 0);
			g_free(out);
			return en;
		}
		return FALSE;
	}
	case MODE_FLATPAK:
	case MODE_SNAP:
		return FALSE;
	}
	return FALSE;
}

static gboolean autostart_supported(void) {
	return (app->mode_type == MODE_SYSTEMD);
}

static void update_autostart_label(void) {
	if (!autostart_supported()) {
		gtk_widget_set_sensitive(app->btn_autostart, FALSE);
		gtk_button_set_label(GTK_BUTTON(app->btn_autostart),
		                     _("开机自启: -", "Autostart: -"));
		gtk_widget_set_sensitive(app->tray_autostart, FALSE);
		gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_autostart),
		                        _("开机自启: -", "Autostart: -"));
		return;
	}
	gboolean en = is_autostart_enabled();
	gtk_widget_set_sensitive(app->btn_autostart, TRUE);
	gtk_button_set_label(GTK_BUTTON(app->btn_autostart),
	                     en ? _("禁用开机自启", "Disable Autostart")
	                        : _("启用开机自启", "Enable Autostart"));
	gtk_widget_set_sensitive(app->tray_autostart, TRUE);
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_autostart),
	                        en ? _("禁用开机自启", "Disable Autostart")
	                           : _("启用开机自启", "Enable Autostart"));
}

static gboolean idle_update(gpointer data);

static void on_autostart_toggle(void) {
	if (!autostart_supported()) return;
	gboolean en = is_autostart_enabled();
	if (en)
		run_cmd(SUDO " " SYSTEMCTL " disable " SERVICE);
	else
		run_cmd(SUDO " " SYSTEMCTL " enable " SERVICE);
	g_timeout_add_seconds(1, idle_update, NULL);
}

static void detect(void) {
	g_free(app->flatpak_id);  app->flatpak_id = NULL;
	g_free(app->snap_id);     app->snap_id = NULL;
	app->native_found = FALSE;
	app->flatpak_found = FALSE;
	app->snap_found = FALSE;

	gint status;
	if (g_spawn_command_line_sync("which " SERVICE, NULL, NULL, &status, NULL) && status == 0)
		app->native_found = TRUE;
	if (!app->native_found) {
		gchar *cmd = g_strdup_printf(SYSTEMCTL " list-unit-files %s.service", SERVICE);
		gchar *out = NULL;
		if (g_spawn_command_line_sync(cmd, &out, NULL, &status, NULL) && status == 0 && out)
			app->native_found = (g_strstr_len(out, -1, SERVICE ".service") != NULL);
		g_free(out);
		g_free(cmd);
	}

	{
		gchar *out = NULL;
		if (g_spawn_command_line_sync("flatpak list --app --columns=application",
		                              &out, NULL, &status, NULL) && status == 0 && out) {
			gchar **lines = g_strsplit(out, "\n", -1);
			for (int i = 0; lines[i]; i++) {
				g_strstrip(lines[i]);
				if (strlen(lines[i]) > 0 &&
				    (g_strstr_len(lines[i], -1, "jellyfin") ||
				     g_strstr_len(lines[i], -1, "Jellyfin"))) {
					app->flatpak_found = TRUE;
					app->flatpak_id = g_strdup(lines[i]);
					break;
				}
			}
			g_strfreev(lines);
		}
		g_free(out);
	}

	{
		gchar *out = NULL;
		if (g_spawn_command_line_sync("snap list", &out, NULL, &status, NULL) && status == 0 && out) {
			gchar **lines = g_strsplit(out, "\n", -1);
			for (int i = 0; lines[i]; i++) {
				if (g_strstr_len(lines[i], -1, "jellyfin") ||
				    g_strstr_len(lines[i], -1, "Jellyfin")) {
					app->snap_found = TRUE;
					char name[64] = {0};
					sscanf(lines[i], "%63s", name);
					app->snap_id = g_strdup(name);
					break;
				}
			}
			g_strfreev(lines);
		}
		g_free(out);
	}

	g_free(app->mode_id);
	if (app->native_found) {
		app->mode_type = MODE_SYSTEMD;
		app->mode_id = g_strdup("systemd");
	} else if (app->flatpak_found) {
		app->mode_type = MODE_FLATPAK;
		app->mode_id = g_strdup(app->flatpak_id);
	} else if (app->snap_found) {
		app->mode_type = MODE_SNAP;
		app->mode_id = g_strdup(app->snap_id);
	} else {
		app->mode_type = MODE_SYSTEMD;
		app->mode_id = NULL;
	}
}

static gboolean update_label(void) {
	if (app->mode_type == MODE_SYSTEMD && !app->native_found) {
		gtk_label_set_text(GTK_LABEL(app->status_label), _("未安装 Jellyfin", "Jellyfin not installed"));
		disable_all_buttons();
		return FALSE;
	}

	gboolean running = FALSE;

	switch (app->mode_type) {
	case MODE_SYSTEMD: {
		gchar *out = NULL;
		if (g_spawn_command_line_sync(SYSTEMCTL " is-active " SERVICE,
		                              &out, NULL, NULL, NULL) && out) {
			g_strstrip(out);
			running = (g_strcmp0(out, "active") == 0);
			g_free(out);
		}
		break;
	}
	case MODE_FLATPAK: {
		gchar *out = NULL;
		if (g_spawn_command_line_sync("flatpak ps --columns=application",
		                              &out, NULL, NULL, NULL) && out) {
			running = (g_strstr_len(out, -1, app->mode_id) != NULL);
			g_free(out);
		}
		break;
	}
	case MODE_SNAP: {
		gchar *out = NULL;
		gchar *scmd = g_strdup_printf("snap services %s", app->mode_id);
		if (g_spawn_command_line_sync(scmd, &out, NULL, NULL, NULL) && out) {
			gchar **lines = g_strsplit(out, "\n", -1);
			for (int i = 0; lines[i]; i++) {
				if (g_strstr_len(lines[i], -1, app->mode_id)) {
					running = (g_strstr_len(lines[i], -1, " active") != NULL);
					break;
				}
			}
			g_strfreev(lines);
			g_free(out);
		}
		g_free(scmd);
		break;
	}
	}

	gtk_label_set_text(GTK_LABEL(app->status_label),
	                   running ? _("● 运行中", "● Running") : _("○ 已停止", "○ Stopped"));
	set_buttons_running(running);
	update_autostart_label();
	return FALSE;
}

static gboolean timer_cb(gpointer data) {
	(void)data;
	update_label();
	return G_SOURCE_CONTINUE;
}

static gboolean idle_update(gpointer data) {
	(void)data;
	update_label();
	return G_SOURCE_REMOVE;
}

static void on_mode_changed(GtkComboBox *combo, gpointer data) {
	(void)combo; (void)data;
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
		return;
	gchar *text = NULL;
	gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)),
	                   &iter, 0, &text, -1);
	if (text == NULL) return;

	if (g_strstr_len(text, -1, "Flatpak") && app->flatpak_id) {
		app->mode_type = MODE_FLATPAK;
		g_free(app->mode_id);
		app->mode_id = g_strdup(app->flatpak_id);
	} else if (g_strstr_len(text, -1, "Snap") && app->snap_id) {
		app->mode_type = MODE_SNAP;
		g_free(app->mode_id);
		app->mode_id = g_strdup(app->snap_id);
	} else {
		app->mode_type = MODE_SYSTEMD;
		g_free(app->mode_id);
		app->mode_id = g_strdup("systemd");
	}
	g_free(text);
	update_label();
}

static void on_start(void) {
	switch (app->mode_type) {
	case MODE_SYSTEMD:
		run_cmd(SUDO " " SYSTEMCTL " start " SERVICE);
		break;
	case MODE_FLATPAK:
		run_cmd("flatpak run %s", app->mode_id);
		break;
	case MODE_SNAP:
		run_cmd(SUDO " snap start %s", app->mode_id);
		break;
	}
	g_timeout_add_seconds(1, timer_cb, NULL);
}

static void on_stop(void) {
	switch (app->mode_type) {
	case MODE_SYSTEMD:
		run_cmd(SUDO " " SYSTEMCTL " stop " SERVICE);
		break;
	case MODE_FLATPAK:
		run_cmd("flatpak kill %s", app->mode_id);
		break;
	case MODE_SNAP:
		run_cmd(SUDO " snap stop %s", app->mode_id);
		break;
	}
	g_timeout_add_seconds(1, timer_cb, NULL);
}

static void on_restart(void) {
	switch (app->mode_type) {
	case MODE_SYSTEMD:
		run_cmd(SUDO " " SYSTEMCTL " restart " SERVICE);
		break;
	case MODE_FLATPAK:
		run_cmd("flatpak kill %s", app->mode_id);
		g_usleep(500000);
		run_cmd("flatpak run %s", app->mode_id);
		break;
	case MODE_SNAP:
		run_cmd(SUDO " snap restart %s", app->mode_id);
		break;
	}
	g_timeout_add_seconds(1, timer_cb, NULL);
}

static void on_quit(void) {
	gtk_main_quit();
}

static void on_quit_and_stop(void) {
	switch (app->mode_type) {
	case MODE_SYSTEMD:
		run_cmd(SUDO " " SYSTEMCTL " stop " SERVICE);
		break;
	case MODE_FLATPAK:
		run_cmd("flatpak kill %s", app->mode_id);
		break;
	case MODE_SNAP:
		run_cmd(SUDO " snap stop %s", app->mode_id);
		break;
	}
	gtk_main_quit();
}

static void on_browser(void) {
	const gchar *p = gtk_entry_get_text(GTK_ENTRY(app->port_entry));
	if (p == NULL || *p == '\0') p = DEF_PORT;
	gchar *url = g_strdup_printf("http://localhost:%s", p);
	run_cmd("xdg-open %s", url);
	g_free(url);
}

static void show_window(void) {
	update_label();
	gtk_window_present(GTK_WINDOW(app->window));
}

static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer data) {
	(void)w; (void)e; (void)data;
	gtk_widget_hide(w);
	return TRUE;
}

static GtkWidget *make_button(const gchar *label, GCallback cb) {
	GtkWidget *b = gtk_button_new_with_label(label);
	g_signal_connect(b, "clicked", cb, NULL);
	return b;
}

static int count_installed(void) {
	int n = 0;
	if (app->native_found) n++;
	if (app->flatpak_found) n++;
	if (app->snap_found) n++;
	return n;
}

static void do_refresh(gboolean redetect);

static void on_refresh(void) {
	do_refresh(TRUE);
}

static void on_lang_changed(void) {
	is_zh = !is_zh;
	gtk_button_set_label(GTK_BUTTON(app->lang_btn), is_zh ? "English" : "中文");
	gtk_window_set_title(GTK_WINDOW(app->window), _("Jellyfin 控制", "Jellyfin Control"));
	app_indicator_set_title(app->indicator, _("Jellyfin 控制", "Jellyfin Control"));

	gtk_button_set_label(GTK_BUTTON(app->btn_start), _("启动", "Start"));
	gtk_button_set_label(GTK_BUTTON(app->btn_stop), _("停止", "Stop"));
	gtk_button_set_label(GTK_BUTTON(app->btn_restart), _("重启", "Restart"));
	gtk_button_set_label(GTK_BUTTON(app->btn_quit_and_stop), _("退出并停止服务", "Quit & Stop Service"));
	gtk_button_set_label(GTK_BUTTON(app->btn_quit), _("仅退出", "Quit Only"));
	gtk_button_set_label(GTK_BUTTON(app->btn_browser), _("打开浏览器", "Open Browser"));

	gtk_label_set_label(GTK_LABEL(app->port_label), _("端口:", "Port:"));
	gtk_label_set_label(GTK_LABEL(app->lang_label), _("语言：", "Language:"));

	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_show), _("显示", "Show"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_start), _("启动", "Start"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_stop), _("停止", "Stop"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_restart), _("重启", "Restart"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_browser), _("打开浏览器", "Open Browser"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_quit_and_stop), _("退出并停止服务", "Quit & Stop Service"));
	gtk_menu_item_set_label(GTK_MENU_ITEM(app->tray_quit), _("仅退出", "Quit Only"));

	do_refresh(FALSE);
}

static void do_refresh(gboolean redetect) {
	if (redetect) detect();
	gboolean installed = (count_installed() > 0);

	if (app->top_section) {
		gtk_widget_destroy(app->top_section);
		app->top_section = NULL;
		app->mode_combo = NULL;
		app->status_label = NULL;
	}

	g_source_remove(app->timer);
	app->top_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

	if (installed) {
		if (count_installed() > 1) {
			GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
			gtk_box_pack_start(GTK_BOX(app->top_section), hbox, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox),
			                   gtk_label_new(_("版本:", "Version:")), FALSE, FALSE, 0);

			app->mode_combo = gtk_combo_box_text_new();
			if (app->native_found)
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo),
				                          NULL, _("系统安装 (systemd)", "System (systemd)"));
			if (app->flatpak_found) {
				gchar *lbl = g_strdup_printf("Flatpak (%s)", app->flatpak_id);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo),
				                          NULL, lbl);
				g_free(lbl);
			}
			if (app->snap_found) {
				gchar *lbl = g_strdup_printf("Snap (%s)", app->snap_id);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo),
				                          NULL, lbl);
				g_free(lbl);
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(app->mode_combo), 0);
			g_signal_connect(app->mode_combo, "changed",
			                 G_CALLBACK(on_mode_changed), NULL);
			gtk_box_pack_start(GTK_BOX(hbox), app->mode_combo, FALSE, FALSE, 0);
		} else {
			gchar *mode_label;
			if (app->native_found)
				mode_label = g_strdup(_("版本: 系统安装 (systemd)", "Version: System (systemd)"));
			else if (app->flatpak_found)
				mode_label = g_strdup_printf(_("版本: Flatpak (%s)", "Version: Flatpak (%s)"), app->flatpak_id);
			else
				mode_label = g_strdup_printf(_("版本: Snap (%s)", "Version: Snap (%s)"), app->snap_id);
			GtkWidget *mlabel = gtk_label_new(mode_label);
			gtk_box_pack_start(GTK_BOX(app->top_section), mlabel, FALSE, FALSE, 0);
			g_free(mode_label);
		}

		app->status_label = gtk_label_new(_("检查中...", "Checking..."));
		gtk_label_set_markup(GTK_LABEL(app->status_label), _("<big>检查中...</big>", "<big>Checking...</big>"));
		gtk_box_pack_start(GTK_BOX(app->top_section), app->status_label, FALSE, FALSE, 0);

		disable_all_buttons();
		app->timer = g_timeout_add_seconds(5, timer_cb, NULL);
		g_idle_add(idle_update, NULL);
	} else {
		GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_pack_start(GTK_BOX(app->top_section), hbox, FALSE, FALSE, 0);

		app->status_label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(app->status_label), _("<big>未检测到 Jellyfin</big>", "<big>Jellyfin not detected</big>"));
		gtk_box_pack_start(GTK_BOX(hbox), app->status_label, FALSE, FALSE, 0);

		GtkWidget *refresh_btn = gtk_button_new_with_label(_("刷新", "Refresh"));
		g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh), NULL);
		gtk_box_pack_start(GTK_BOX(hbox), refresh_btn, FALSE, FALSE, 0);
	}

	gtk_box_pack_start(GTK_BOX(app->vbox), app->top_section, FALSE, FALSE, 0);
	gtk_widget_show_all(app->top_section);

	if (!installed)
		disable_all_buttons();
}

static void build_ui(void) {
	gboolean installed = (count_installed() > 0);

	app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(app->window), _("Jellyfin 控制", "Jellyfin Control"));
	gtk_window_set_default_size(GTK_WINDOW(app->window), 340, -1);
	gtk_container_set_border_width(GTK_CONTAINER(app->window), 16);
	g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete), NULL);
	g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	app->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(app->window), app->vbox);

	app->top_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

	if (installed) {
		if (count_installed() > 1) {
			GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
			gtk_box_pack_start(GTK_BOX(app->top_section), hbox, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox),
			                   gtk_label_new(_("版本:", "Version:")), FALSE, FALSE, 0);

			app->mode_combo = gtk_combo_box_text_new();
			if (app->native_found)
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo),
				                          NULL, _("系统安装 (systemd)", "System (systemd)"));
			if (app->flatpak_found) {
				gchar *lbl = g_strdup_printf("Flatpak (%s)", app->flatpak_id);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo),
				                          NULL, lbl);
				g_free(lbl);
			}
			if (app->snap_found) {
				gchar *lbl = g_strdup_printf("Snap (%s)", app->snap_id);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo),
				                          NULL, lbl);
				g_free(lbl);
			}
			gtk_combo_box_set_active(GTK_COMBO_BOX(app->mode_combo), 0);
			g_signal_connect(app->mode_combo, "changed",
			                 G_CALLBACK(on_mode_changed), NULL);
			gtk_box_pack_start(GTK_BOX(hbox), app->mode_combo, FALSE, FALSE, 0);
		} else {
			gchar *mode_label;
			if (app->native_found)
				mode_label = g_strdup(_("版本: 系统安装 (systemd)", "Version: System (systemd)"));
			else if (app->flatpak_found)
				mode_label = g_strdup_printf(_("版本: Flatpak (%s)", "Version: Flatpak (%s)"), app->flatpak_id);
			else
				mode_label = g_strdup_printf(_("版本: Snap (%s)", "Version: Snap (%s)"), app->snap_id);
			GtkWidget *mlabel = gtk_label_new(mode_label);
			gtk_box_pack_start(GTK_BOX(app->top_section), mlabel, FALSE, FALSE, 0);
			g_free(mode_label);
		}

		app->status_label = gtk_label_new(_("检查中...", "Checking..."));
		gtk_label_set_markup(GTK_LABEL(app->status_label), _("<big>检查中...</big>", "<big>Checking...</big>"));
		gtk_box_pack_start(GTK_BOX(app->top_section), app->status_label, FALSE, FALSE, 0);
	} else {
		GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_pack_start(GTK_BOX(app->top_section), hbox, FALSE, FALSE, 0);

		app->status_label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(app->status_label), _("<big>未检测到 Jellyfin</big>", "<big>Jellyfin not detected</big>"));
		gtk_box_pack_start(GTK_BOX(hbox), app->status_label, FALSE, FALSE, 0);

		GtkWidget *refresh_btn = gtk_button_new_with_label(_("刷新", "Refresh"));
		g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh), NULL);
		gtk_box_pack_start(GTK_BOX(hbox), refresh_btn, FALSE, FALSE, 0);
	}

	gtk_box_pack_start(GTK_BOX(app->vbox), app->top_section, FALSE, FALSE, 0);

	GtkWidget *hbtn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(app->vbox), hbtn, FALSE, FALSE, 0);

	app->btn_start = make_button(_("启动", "Start"), G_CALLBACK(on_start));
	gtk_box_pack_start(GTK_BOX(hbtn), app->btn_start, FALSE, FALSE, 0);
	app->btn_stop = make_button(_("停止", "Stop"), G_CALLBACK(on_stop));
	gtk_box_pack_start(GTK_BOX(hbtn), app->btn_stop, FALSE, FALSE, 0);
	app->btn_restart = make_button(_("重启", "Restart"), G_CALLBACK(on_restart));
	gtk_box_pack_start(GTK_BOX(hbtn), app->btn_restart, FALSE, FALSE, 0);

	GtkWidget *hquit = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(app->vbox), hquit, FALSE, FALSE, 0);
	app->btn_quit_and_stop = make_button(_("退出并停止服务", "Quit & Stop Service"), G_CALLBACK(on_quit_and_stop));
	gtk_box_pack_start(GTK_BOX(hquit), app->btn_quit_and_stop, FALSE, FALSE, 0);
	app->btn_quit = make_button(_("仅退出", "Quit Only"), G_CALLBACK(on_quit));
	gtk_box_pack_start(GTK_BOX(hquit), app->btn_quit, FALSE, FALSE, 0);

	GtkWidget *hport = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(app->vbox), hport, FALSE, FALSE, 0);

	app->port_label = gtk_label_new(_("端口:", "Port:"));
	gtk_box_pack_start(GTK_BOX(hport), app->port_label, FALSE, FALSE, 0);

	app->port_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(app->port_entry), DEF_PORT);
	gtk_entry_set_max_length(GTK_ENTRY(app->port_entry), 6);
	gtk_entry_set_width_chars(GTK_ENTRY(app->port_entry), 6);
	gtk_box_pack_start(GTK_BOX(hport), app->port_entry, FALSE, FALSE, 0);

	app->btn_browser = make_button(_("打开浏览器", "Open Browser"), G_CALLBACK(on_browser));
	gtk_box_pack_start(GTK_BOX(hport), app->btn_browser, FALSE, FALSE, 0);

	GtkWidget *hlang = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(app->vbox), hlang, FALSE, FALSE, 0);
	app->lang_label = gtk_label_new(_("语言：", "Language:"));
	gtk_box_pack_start(GTK_BOX(hlang), app->lang_label, FALSE, FALSE, 0);
	app->lang_btn = gtk_button_new_with_label("English");
	g_signal_connect(app->lang_btn, "clicked", G_CALLBACK(on_lang_changed), NULL);
	gtk_box_pack_start(GTK_BOX(hlang), app->lang_btn, FALSE, FALSE, 0);

	GtkWidget *haustart = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(app->vbox), haustart, FALSE, FALSE, 0);
	app->btn_autostart = gtk_button_new_with_label(_("启用开机自启", "Enable Autostart"));
	g_signal_connect(app->btn_autostart, "clicked", G_CALLBACK(on_autostart_toggle), NULL);
	gtk_box_pack_start(GTK_BOX(haustart), app->btn_autostart, FALSE, FALSE, 0);

	/* tray menu */
	app->tray_menu = gtk_menu_new();

	app->tray_show = gtk_menu_item_new_with_label(_("显示", "Show"));
	g_signal_connect(app->tray_show, "activate", G_CALLBACK(show_window), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_show);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), gtk_separator_menu_item_new());

	app->tray_start = gtk_menu_item_new_with_label(_("启动", "Start"));
	g_signal_connect(app->tray_start, "activate", G_CALLBACK(on_start), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_start);
	app->tray_stop = gtk_menu_item_new_with_label(_("停止", "Stop"));
	g_signal_connect(app->tray_stop, "activate", G_CALLBACK(on_stop), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_stop);
	app->tray_restart = gtk_menu_item_new_with_label(_("重启", "Restart"));
	g_signal_connect(app->tray_restart, "activate", G_CALLBACK(on_restart), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_restart);

	app->tray_autostart = gtk_menu_item_new_with_label(_("启用开机自启", "Enable Autostart"));
	g_signal_connect(app->tray_autostart, "activate", G_CALLBACK(on_autostart_toggle), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_autostart);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), gtk_separator_menu_item_new());

	app->tray_browser = gtk_menu_item_new_with_label(_("打开浏览器", "Open Browser"));
	g_signal_connect(app->tray_browser, "activate", G_CALLBACK(on_browser), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_browser);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), gtk_separator_menu_item_new());

	app->tray_quit_and_stop = gtk_menu_item_new_with_label(_("退出并停止服务", "Quit & Stop Service"));
	g_signal_connect(app->tray_quit_and_stop, "activate", G_CALLBACK(on_quit_and_stop), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_quit_and_stop);

	app->tray_quit = gtk_menu_item_new_with_label(_("仅退出", "Quit Only"));
	g_signal_connect(app->tray_quit, "activate", G_CALLBACK(on_quit), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_quit);

	gtk_widget_show_all(app->tray_menu);

	/* indicator (system tray on Wayland via StatusNotifierItem) */
	app->indicator = app_indicator_new_with_path(
		"jellyfin-ctl", "jellyfin-ctl",
		APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
		"/usr/share/pixmaps");
	app_indicator_set_status(app->indicator, APP_INDICATOR_STATUS_ACTIVE);
	app_indicator_set_menu(app->indicator, GTK_MENU(app->tray_menu));
	app_indicator_set_title(app->indicator, _("Jellyfin 控制", "Jellyfin Control"));

	app->timer = g_timeout_add_seconds(5, timer_cb, NULL);
	g_idle_add(idle_update, NULL);
}

static gboolean on_sigusr1(gpointer data) {
	(void)data;
	if (app && app->window) {
		gtk_window_present(GTK_WINDOW(app->window));
		update_label();
	}
	return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[]) {
	/* single instance */
	int fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0644);
	if (fd < 0) return 1;
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		char buf[32] = {0};
		int rfd = open(LOCK_FILE, O_RDONLY);
		if (rfd >= 0) {
			if (read(rfd, buf, sizeof(buf) - 1) < 0) {} /* ignore */
			close(rfd);
			pid_t pid = atoi(buf);
			if (pid > 0) kill(pid, SIGUSR1);
		}
		close(fd);
		return 0;
	}
	char pid_buf[32];
	int len = snprintf(pid_buf, sizeof(pid_buf), "%d\n", getpid());
	if (ftruncate(fd, 0) < 0) {} /* ignore */
	if (write(fd, pid_buf, len) < 0) {} /* ignore */

	gtk_init(&argc, &argv);
	g_unix_signal_add(SIGUSR1, on_sigusr1, NULL);

	App a;
	memset(&a, 0, sizeof(a));
	app = &a;

	detect();
	build_ui();
	gtk_widget_show_all(app->window);
	gtk_main();

	g_source_remove(app->timer);
	g_free(a.mode_id);
	g_free(a.flatpak_id);
	g_free(a.snap_id);
	close(fd);
	unlink(LOCK_FILE);
	return 0;
}
