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
"      <arg type='s' name='response' direction='out'/>"
"    </method>"
"    <signal name='Notification'>"
"      <arg type='s' name='message'/>"
"    </signal>"
"  </interface>"
"</node>";

// Process A의 자동 메시지를 처리할 소켓
static int auto_sockfd = -1;
static GIOChannel *auto_channel = NULL;

// 주기적 메시지 수신 콜백
static gboolean on_auto_message(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    gchar buffer[256];
    gsize bytes_read;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_chars(source, buffer, sizeof(buffer)-1, &bytes_read, &error);
    if (status == G_IO_STATUS_ERROR) {
        g_print("Auto-read error: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        g_print("Auto-received from A: %s\n", buffer);
        
        // ★ D-Bus 시그널 발송
        g_dbus_connection_emit_signal(dbus_conn,
                                    NULL,
                                    "/org/example/AsyncService",
                                    "org.example.AsyncService",
                                    "Notification",
                                    g_variant_new("(s)", buffer),
                                    NULL);
    }

    return TRUE;
}

// Process A와의 자동 통신 연결 초기화
static void init_auto_connection() {
    auto_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (auto_sockfd == -1) {
        g_print("Auto-socket creation failed: %s\n", strerror(errno));
        return;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/socket_a_auto", sizeof(addr.sun_path)-1);

    if (connect(auto_sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        g_print("Auto-connect failed: %s\n", strerror(errno));
        close(auto_sockfd);
        return;
    }

    auto_channel = g_io_channel_unix_new(auto_sockfd);
    g_io_channel_set_encoding(auto_channel, NULL, NULL);
    g_io_add_watch(auto_channel, G_IO_IN, on_auto_message, NULL);
}

// ... [이전의 handle_method_call, main 함수는 동일] ...

int main() {
    loop = g_main_loop_new(NULL, FALSE);
    GError *error = NULL;

    dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!dbus_conn) {
        g_print("D-Bus error: %s\n", error->message);
        return 1;
    }

    // Process A 자동 통신 초기화
    init_auto_connection();

    // ... [이전의 D-Bus 서비스 등록 코드] ...

    g_print("Server ready. Waiting for requests...\n");
    g_main_loop_run(loop);

    // 정리
    if (auto_channel) g_io_channel_unref(auto_channel);
    if (auto_sockfd != -1) close(auto_sockfd);
    g_object_unref(dbus_conn);
    g_main_loop_unref(loop);
    return 0;
}
