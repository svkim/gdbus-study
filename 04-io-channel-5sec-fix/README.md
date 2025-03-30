
4. 실행 순서

```
    Process A 실행 (별도 터미널):

    gcc process_a.c -o process_a
    ./process_a

    서버 실행:

    gcc dbus_socket_server.c -o server `pkg-config --cflags --libs gio-2.0`
    ./server

    클라이언트 실행:

    gcc dbus_socket_client.c -o client `pkg-config --cflags --libs gio-2.0`
    ./client
```
