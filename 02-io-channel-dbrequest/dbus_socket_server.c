#include <glib.h>
#include <gio/gio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static GMainLoop *loop;
static const gchar *interface_xml =
"<node>"
"  <interface name='org.example.AsyncService'>"
"    <method name='SendData'>"
"      <arg type='s' name='input' direction='in'/>"
"      <arg type='s' name='response' direction='out'/>"
"    </method>"
"  </interface>"
"</node>";

// 소켓 응답을 읽고 D-Bus 응답을 완료하는 구조체
typedef struct {
    GDBusMethodInvocation *invocation;
    GIOChannel *channel;
} AsyncData;

static void async_data_free(AsyncData *data) {
    if (data->channel) g_io_channel_unref(data->channel);
    g_free(data);
}

// 소켓 응답 콜백
static gboolean on_socket_response(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    AsyncData *data = (AsyncData *)user_data;
    gchar buffer[256];
    gsize bytes_read;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_chars(source, buffer, sizeof(buffer)-1, &bytes_read, &error);
    if (status == G_IO_STATUS_ERROR) {
        g_dbus_method_invocation_return_error(data->invocation, G_IO_ERROR, G_IO_ERROR_FAILED,
                                             "Socket read error: %s", error->message);
        g_error_free(error);
        async_data_free(data);
        return FALSE;
    }

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        g_print("Received from A: %s\n", buffer);
        
        // ★ D-Bus 메소드 응답 전송
        g_dbus_method_invocation_return_value(data->invocation,
                                            g_variant_new("(s)", buffer));
    } else if (status == G_IO_STATUS_EOF) {
        g_dbus_method_invocation_return_error(data->invocation, G_IO_ERROR, G_IO_ERROR_FAILED,
                                             "Socket connection closed");
    }

    async_data_free(data);
    return FALSE; // 콜백 제거
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

        int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1) {
            g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_FAILED,
                                                "Socket creation failed: %s", strerror(errno));
            return;
        }

        // 논블로킹 설정
        int flags = fcntl(sockfd, F_GETFL);
        if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_FAILED,
                                                "Non-blocking set failed: %s", strerror(errno));
            close(sockfd);
            return;
        }

        struct sockaddr_un addr = {0};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/tmp/socket_a", sizeof(addr.sun_path)-1);

        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr))) {
            if (errno != EINPROGRESS) {
                g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_FAILED,
                                                    "Connect failed: %s", strerror(errno));
                close(sockfd);
                return;
            }
        }

        // 비동기 데이터 구조체 생성
        AsyncData *data = g_new0(AsyncData, 1);
        data->invocation = invocation;
        data->channel = g_io_channel_unix_new(sockfd);
        g_io_channel_set_encoding(data->channel, NULL, NULL);

        // 데이터 전송
        GError *error = NULL;
        g_io_channel_write_chars(data->channel, input, -1, NULL, &error);
        g_io_channel_flush(data->channel, NULL);
        if (error) {
            g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_FAILED,
                                                "Write error: %s", error->message);
            g_error_free(error);
            async_data_free(data);
            return;
        }

        // 응답 대기
        g_io_add_watch(data->channel, G_IO_IN | G_IO_HUP, on_socket_response, data);
    }
}

int main() {
    loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;

    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!conn) {
        g_print("D-Bus error: %s\n", error->message);
        return 1;
    }

    GDBusNodeInfo *introspection = g_dbus_node_info_new_for_xml(interface_xml, NULL);
    GDBusInterfaceVTable vtable = {handle_method_call, NULL, NULL};
    
    g_dbus_connection_register_object(conn,
                                    "/org/example/AsyncService",
                                    introspection->interfaces[0],
                                    &vtable,
                                    NULL, NULL, &error);

    g_bus_own_name_on_connection(conn,
                                "org.example.AsyncService",
                                G_BUS_NAME_OWNER_FLAGS_NONE,
                                NULL, NULL, NULL, NULL);

    g_print("Server ready. Waiting for requests...\n");
    g_main_loop_run(loop);

    g_object_unref(conn);
    g_main_loop_unref(loop);
    return 0;
}
