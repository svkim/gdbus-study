#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

int main() {
    // 메인 소켓 (기존 요청 처리)
    int main_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un main_addr = {0};
    main_addr.sun_family = AF_UNIX;
    strncpy(main_addr.sun_path, "/tmp/socket_a", sizeof(main_addr.sun_path)-1);

    unlink(main_addr.sun_path);
    bind(main_fd, (struct sockaddr*)&main_addr, sizeof(main_addr));
    listen(main_fd, 5);

    // 자동 메시지 소켓 (5초 주기)
    int auto_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un auto_addr = {0};
    auto_addr.sun_family = AF_UNIX;
    strncpy(auto_addr.sun_path, "/tmp/socket_a_auto", sizeof(auto_addr.sun_path)-1);

    unlink(auto_addr.sun_path);
    bind(auto_fd, (struct sockaddr*)&auto_addr, sizeof(auto_addr));
    listen(auto_fd, 5);

    printf("Process A ready\n");

    // 자동 메시지 전송 스레드
    if (fork() == 0) {
        int client_fd = accept(auto_fd, NULL, NULL);
        while (1) {
            time_t now = time(NULL);
            char *msg = ctime(&now);
            write(client_fd, msg, strlen(msg));
            sleep(5); // 5초 대기
        }
    }

    // 메인 요청 처리
    while (1) {
        int client_fd = accept(main_fd, NULL, NULL);
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
