#include <glib.h>
#include <gio/gio.h>

static void on_notification(GDBusConnection *conn,
                          const gchar *sender,
                          const gchar *path,
                          const gchar *interface,
                          const gchar *signal,
                          GVariant *params,
                          gpointer user_data) {
    const gchar *msg;
    g_variant_get(params, "(&s)", &msg);
    g_print("Notification: %s", msg); // 시간 메시지에 이미 개행 포함
}

int main() {
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;
    
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) {
        g_print("Connection error: %s\n", error->message);
        return 1;
    }

    // 시그널 구독
    g_dbus_connection_signal_subscribe(conn,
                                     "org.example.AsyncService",
                                     "org.example.AsyncService",
                                     "Notification",
                                     "/org/example/AsyncService",
                                     NULL,
                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                     on_notification,
                                     NULL,
                                     NULL);

    // 메소드 호출 (옵션)
    GVariant *result = g_dbus_connection_call_sync(conn,
        "org.example.AsyncService",
        "/org/example/AsyncService",
        "org.example.AsyncService",
        "SendData",
        g_variant_new("(s)", "Manual request"),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (result) {
        const gchar *response;
        g_variant_get(result, "(&s)", &response);
        g_print("Manual Response: %s\n", response);
        g_variant_unref(result);
    }

    g_print("Waiting for notifications...\n");
    g_main_loop_run(loop);

    g_object_unref(conn);
    g_main_loop_unref(loop);
    return 0;
}
