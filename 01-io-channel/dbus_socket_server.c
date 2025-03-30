#include <glib.h>
#include <gio/gio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static GMainLoop *loop;
static GDBusConnection *dbus_conn;
static const gchar *interface_xml =
"<node>"
"  <interface name='org.example.AsyncService'>"
"    <method name='SendData'>"
"      <arg type='s' name='input' direction='in'/>"
"    </method>"
"    <signal name='ResponseReady'>"
"      <arg type='s' name='response'/>"
"    </signal>"
"  </interface>"
"</node>";

// Unix 소켓 응답 콜백 (비동기)
static gboolean on_socket_response(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    gchar buffer[256];
    gsize bytes_read;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_chars(source, buffer, sizeof(buffer)-1, &bytes_read, &error);
    if (status == G_IO_STATUS_ERROR) {
        g_print("Socket read error: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        g_print("Received from A: %s\n", buffer);
        
        // D-Bus 시그널 발송
        g_dbus_connection_emit_signal(dbus_conn,
                                    NULL,
                                    "/org/example/AsyncService",
                                    "org.example.AsyncService",
                                    "ResponseReady",
                                    g_variant_new("(s)", buffer),
                                    NULL);
    }

    return (status != G_IO_STATUS_EOF);
}

// D-Bus 메소드 핸들러
static void handle_method_call(GDBusConnection *conn,
                             const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *method_name,
                             GVariant *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data) {
    if (g_strcmp0(method_name, "SendData") == 0) {
        const gchar *input;
        g_variant_get(parameters, "(&s)", &input);
        g_print("DBus Request: %s\n", input);

        // Unix 소켓 연결 (Non-blocking)
        int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        fcntl(sockfd, F_SETFL, O_NONBLOCK);

        struct sockaddr_un addr = {0};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/tmp/socket_a", sizeof(addr.sun_path)-1);

        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1 && errno != EINPROGRESS) {
            perror("connect");
            close(sockfd);
            return;
        }

        // GIOChannel로 감싸고 이벤트 등록
        GIOChannel *channel = g_io_channel_unix_new(sockfd);
        g_io_channel_set_encoding(channel, NULL, NULL);
        g_io_add_watch(channel, G_IO_IN | G_IO_HUP, on_socket_response, NULL);

        // 데이터 전송 (Non-blocking)
        GError *error = NULL;
        g_io_channel_write_chars(channel, input, -1, NULL, &error);
        g_io_channel_flush(channel, NULL);
        if (error) {
            g_print("Write error: %s\n", error->message);
            g_error_free(error);
        }

        g_dbus_method_invocation_return_value(invocation, NULL); // 빈 응답
    }
}

int main() {
    loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;

    // D-Bus 연결
    dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!dbus_conn) {
        g_print("D-Bus error: %s\n", error->message);
        return 1;
    }

    // 서비스 등록
    GDBusNodeInfo *introspection = g_dbus_node_info_new_for_xml(interface_xml, NULL);
    GDBusInterfaceVTable vtable = {handle_method_call, NULL, NULL};
    
    g_dbus_connection_register_object(dbus_conn,
                                    "/org/example/AsyncService",
                                    introspection->interfaces[0],
                                    &vtable,
                                    NULL, NULL, &error);

    g_bus_own_name_on_connection(dbus_conn,
                                "org.example.AsyncService",
                                G_BUS_NAME_OWNER_FLAGS_NONE,
                                NULL, NULL, NULL, NULL);

    g_print("Server ready. Waiting for requests...\n");
    g_main_loop_run(loop);

    // 정리
    g_object_unref(dbus_conn);
    g_main_loop_unref(loop);
    return 0;
}
