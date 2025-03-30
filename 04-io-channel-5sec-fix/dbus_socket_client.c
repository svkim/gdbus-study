#include <glib.h>
#include <gio/gio.h>

static void on_notification(GDBusConnection *conn,
                          const gchar *sender,
                          const gchar *object_path,
                          const gchar *interface,
                          const gchar *signal_name,
                          GVariant *parameters,
                          gpointer user_data) {
    const gchar *message;
    g_variant_get(parameters, "(&s)", &message);
    g_print("★ Received signal: %s\n", message);
}

int main() {
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;

    // D-Bus 연결 (서버와 동일한 세션 버스)
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) {
        g_print("Connection failed: %s\n", error->message);
        return 1;
    }

    // ★ 정확한 시그널 구독 설정
    guint sub_id = g_dbus_connection_signal_subscribe(conn,
                                                    "org.example.AsyncService",  // 발신자
                                                    "org.example.AsyncService",  // 인터페이스
                                                    "Notification",              // 시그널명
                                                    "/org/example/AsyncService", // 경로
                                                    NULL,                        // 세부 조건
                                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                                    on_notification,
                                                    NULL,
                                                    NULL);

    g_print("Client waiting for signals...\n");
    g_main_loop_run(loop);

    // 정리
    g_dbus_connection_signal_unsubscribe(conn, sub_id);
    g_object_unref(conn);
    g_main_loop_unref(loop);
    return 0;
}
