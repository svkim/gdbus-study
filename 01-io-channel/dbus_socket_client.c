#include <glib.h>
#include <gio/gio.h>

static void on_signal(GDBusConnection *conn,
                     const gchar *sender,
                     const gchar *path,
                     const gchar *interface,
                     const gchar *signal,
                     GVariant *params,
                     gpointer user_data) {
    const gchar *response;
    g_variant_get(params, "(&s)", &response);
    g_print("Received Signal: %s\n", response);
    g_main_loop_quit((GMainLoop*)user_data);
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
                                     "ResponseReady",
                                     "/org/example/AsyncService",
                                     NULL,
                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                     on_signal,
                                     loop,
                                     NULL);

    // 메소드 호출
    g_dbus_connection_call(conn,
                         "org.example.AsyncService",
                         "/org/example/AsyncService",
                         "org.example.AsyncService",
                         "SendData",
                         g_variant_new("(s)", "Hello from client!"),
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         NULL,
                         NULL);

    g_print("Waiting for response...\n");
    g_main_loop_run(loop);

    g_object_unref(conn);
    g_main_loop_unref(loop);
    return 0;
}
