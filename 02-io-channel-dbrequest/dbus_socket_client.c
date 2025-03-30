#include <glib.h>
#include <gio/gio.h>

int main() {
    GError *error = NULL;
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) {
        g_print("Connection error: %s\n", error->message);
        return 1;
    }

    // ★ 동기 호출로 변경 (응답을 직접 받음)
    GVariant *result = g_dbus_connection_call_sync(conn,
        "org.example.AsyncService",
        "/org/example/AsyncService",
        "org.example.AsyncService",
        "SendData",
        g_variant_new("(s)", "Hello from client!"),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error);

    if (!result) {
        g_print("Error: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    const gchar *response;
    g_variant_get(result, "(&s)", &response);
    g_print("Server Response: %s\n", response);
    g_variant_unref(result);

    g_object_unref(conn);
    return 0;
}
