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
	guint          timer;
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

static gboolean update_label(void) {
	gchar *out = NULL;
	if (g_spawn_command_line_sync(SYSTEMCTL " is-active " SERVICE,
	                              &out, NULL, NULL, NULL) && out) {
		g_strstrip(out);
		const gchar *text;
		if (g_strcmp0(out, "active") == 0)
			text = "● 运行中";
		else if (g_strcmp0(out, "inactive") == 0 ||
		         g_strcmp0(out, "failed") == 0)
			text = "○ 已停止";
		else
			text = "? 未知";
		gtk_label_set_text(GTK_LABEL(app->status_label), text);
		gboolean running = (g_strcmp0(out, "active") == 0);
		if (GTK_IS_WIDGET(app->btn_start))
			gtk_widget_set_sensitive(app->btn_start, !running);
		if (GTK_IS_WIDGET(app->btn_stop))
			gtk_widget_set_sensitive(app->btn_stop, running);
		if (GTK_IS_WIDGET(app->btn_restart))
			gtk_widget_set_sensitive(app->btn_restart, running);
		if (GTK_IS_WIDGET(app->tray_start))
			gtk_widget_set_sensitive(app->tray_start, !running);
		if (GTK_IS_WIDGET(app->tray_stop))
			gtk_widget_set_sensitive(app->tray_stop, running);
		if (GTK_IS_WIDGET(app->tray_restart))
			gtk_widget_set_sensitive(app->tray_restart, running);
		g_free(out);
	}
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

static void on_start(void) {
	run_cmd(SUDO " " SYSTEMCTL " start " SERVICE);
	g_timeout_add_seconds(1, timer_cb, NULL);
}

static void on_stop(void) {
	run_cmd(SUDO " " SYSTEMCTL " stop " SERVICE);
	g_timeout_add_seconds(1, timer_cb, NULL);
}

static void on_restart(void) {
	run_cmd(SUDO " " SYSTEMCTL " restart " SERVICE);
	g_timeout_add_seconds(1, timer_cb, NULL);
}

static void on_quit(void) {
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

static void build_ui(void) {
	app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(app->window), "Jellyfin 控制");
	gtk_window_set_resizable(GTK_WINDOW(app->window), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(app->window), 16);
	g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete), NULL);
	g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(app->window), vbox);

	app->status_label = gtk_label_new("检查中...");
	gtk_label_set_markup(GTK_LABEL(app->status_label), "<big>检查中...</big>");
	gtk_box_pack_start(GTK_BOX(vbox), app->status_label, FALSE, FALSE, 0);

	GtkWidget *hbtn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbtn, FALSE, FALSE, 0);

	app->btn_start = make_button("启动", G_CALLBACK(on_start));
	gtk_box_pack_start(GTK_BOX(hbtn), app->btn_start, FALSE, FALSE, 0);
	app->btn_stop = make_button("停止", G_CALLBACK(on_stop));
	gtk_box_pack_start(GTK_BOX(hbtn), app->btn_stop, FALSE, FALSE, 0);
	app->btn_restart = make_button("重启", G_CALLBACK(on_restart));
	gtk_box_pack_start(GTK_BOX(hbtn), app->btn_restart, FALSE, FALSE, 0);

	GtkWidget *hquit = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hquit, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hquit),
	                   make_button("退出", G_CALLBACK(on_quit)),
	                   FALSE, FALSE, 0);

	GtkWidget *hport = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hport, FALSE, FALSE, 0);

	GtkWidget *port_label = gtk_label_new("端口:");
	gtk_box_pack_start(GTK_BOX(hport), port_label, FALSE, FALSE, 0);

	app->port_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(app->port_entry), DEF_PORT);
	gtk_entry_set_max_length(GTK_ENTRY(app->port_entry), 6);
	gtk_entry_set_width_chars(GTK_ENTRY(app->port_entry), 6);
	gtk_box_pack_start(GTK_BOX(hport), app->port_entry, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hport),
	                   make_button("打开浏览器", G_CALLBACK(on_browser)),
	                   FALSE, FALSE, 0);

	/* tray menu */
	app->tray_menu = gtk_menu_new();
	GtkWidget *item;

	item = gtk_menu_item_new_with_label("显示");
	g_signal_connect(item, "activate", G_CALLBACK(show_window), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), item);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), gtk_separator_menu_item_new());

	app->tray_start = gtk_menu_item_new_with_label("启动");
	g_signal_connect(app->tray_start, "activate", G_CALLBACK(on_start), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_start);
	app->tray_stop = gtk_menu_item_new_with_label("停止");
	g_signal_connect(app->tray_stop, "activate", G_CALLBACK(on_stop), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_stop);
	app->tray_restart = gtk_menu_item_new_with_label("重启");
	g_signal_connect(app->tray_restart, "activate", G_CALLBACK(on_restart), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), app->tray_restart);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), gtk_separator_menu_item_new());

	item = gtk_menu_item_new_with_label("打开浏览器");
	g_signal_connect(item, "activate", G_CALLBACK(on_browser), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), item);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), gtk_separator_menu_item_new());

	item = gtk_menu_item_new_with_label("退出");
	g_signal_connect(item, "activate", G_CALLBACK(on_quit), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_menu), item);

	gtk_widget_show_all(app->tray_menu);

	/* indicator (system tray on Wayland via StatusNotifierItem) */
	app->indicator = app_indicator_new_with_path(
		"jellyfin-ctl", "jellyfin-ctl",
		APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
		"/usr/share/pixmaps");
	app_indicator_set_status(app->indicator, APP_INDICATOR_STATUS_ACTIVE);
	app_indicator_set_menu(app->indicator, GTK_MENU(app->tray_menu));
	app_indicator_set_title(app->indicator, "Jellyfin 控制");

	app->timer = g_timeout_add_seconds(3, timer_cb, NULL);
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
			read(rfd, buf, sizeof(buf) - 1);
			close(rfd);
			pid_t pid = atoi(buf);
			if (pid > 0) kill(pid, SIGUSR1);
		}
		close(fd);
		return 0;
	}
	char pid_buf[32];
	int len = snprintf(pid_buf, sizeof(pid_buf), "%d\n", getpid());
	ftruncate(fd, 0);
	write(fd, pid_buf, len);

	gtk_init(&argc, &argv);
	g_unix_signal_add(SIGUSR1, on_sigusr1, NULL);

	App a;
	memset(&a, 0, sizeof(a));
	app = &a;

	build_ui();
	gtk_widget_show_all(app->window);
	gtk_main();

	g_source_remove(app->timer);
	close(fd);
	unlink(LOCK_FILE);
	return 0;
}
