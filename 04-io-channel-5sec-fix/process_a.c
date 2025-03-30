#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#define SOCKET_PATH "/tmp/socket_a_auto"

int main() {
    int sockfd;
    struct sockaddr_un addr;

    // 소켓 생성
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // 주소 설정
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);

    // 기존 소켓 파일 삭제
    unlink(SOCKET_PATH);

    // 바인드 및 리슨
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        exit(1);
    }

    printf("Process A: Waiting for server connection...\n");

    // 서버 연결 대기
    int client_fd;
    if ((client_fd = accept(sockfd, NULL, NULL)) == -1) {
        perror("accept");
        close(sockfd);
        exit(1);
    }

    printf("Process A: Server connected. Sending messages every 5 seconds...\n");

    // 5초마다 메시지 전송
    while (1) {
        time_t now = time(NULL);
        char *msg = ctime(&now); // 현재 시간 문자열
        msg[strlen(msg)-1] = '\0'; // 개행 문자 제거

        if (write(client_fd, msg, strlen(msg)) == -1) {
            perror("write");
            break;
        }

        printf("Process A: Sent '%s'\n", msg);
        sleep(5);
    }

    close(client_fd);
    close(sockfd);
    unlink(SOCKET_PATH);
    return 0;
}
