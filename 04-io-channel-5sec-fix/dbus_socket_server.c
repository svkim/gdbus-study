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

// 시그널 발송 함수 (명시적으로 export)
static void send_notification(const gchar *message) {
    g_dbus_connection_emit_signal(dbus_conn,
                                NULL,
                                "/org/example/AsyncService",
                                "org.example.AsyncService",
                                "Notification",
                                g_variant_new("(s)", message),
                                NULL);
    g_print("Signal sent: %s\n", message); // 로그 추가
}

// Process A 자동 메시지 처리
static gboolean on_auto_message(GIOChannel *source, GIOCondition cond, gpointer data) {
    gchar buffer[256];
    gsize bytes_read;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_chars(source, buffer, sizeof(buffer)-1, &bytes_read, &error);
    if (status != G_IO_STATUS_NORMAL) {
        g_print("Read error: %s\n", error ? error->message : "EOF");
        if (error) g_error_free(error);
        return FALSE;
    }

    buffer[bytes_read] = '\0';
    send_notification(buffer); // ★ 시그널 발송
    return TRUE;
}


static void connect_to_process_a() {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        g_print("Socket creation failed: %s\n", strerror(errno));
        return;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/socket_a_auto", sizeof(addr.sun_path)-1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        g_print("Connection failed: %s\n", strerror(errno));
        close(sockfd);
        return;
    }

    // 논블로킹 모드 설정
    int flags = fcntl(sockfd, F_GETFL);
    if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        g_print("Non-blocking set failed: %s\n", strerror(errno));
        close(sockfd);
        return;
    }

    GIOChannel *channel = g_io_channel_unix_new(sockfd);
    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_add_watch(channel, G_IO_IN, on_auto_message, NULL);
    g_print("Connected to Process A\n");
}

int main() {
    loop = g_main_loop_new(NULL, FALSE);
    
    // D-Bus 초기화 (기존 코드와 동일)
    dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    g_bus_own_name_on_connection(dbus_conn, "org.example.AsyncService", 
                               G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL);

    // Process A 연결
    connect_to_process_a();

    g_print("Server ready\n");
    g_main_loop_run(loop);

    // ... [기존 정리 코드] ...
}
