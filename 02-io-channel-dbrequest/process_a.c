#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main() {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/socket_a", sizeof(addr.sun_path)-1);

    unlink(addr.sun_path);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Process A ready\n");
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        char buffer[256];
        ssize_t count = read(client_fd, buffer, sizeof(buffer)-1);
        if (count > 0) {
            buffer[count] = '\0';
            printf("Received: %s\n", buffer);
            write(client_fd, "Processed: ", 11);
            write(client_fd, buffer, count);
        }
        close(client_fd);
    }
}
