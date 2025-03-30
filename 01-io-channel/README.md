### 빌드 및 실행 방법 ###

```
# 컴파일
gcc dbus_socket_server.c -o server `pkg-config --cflags --libs gio-2.0`
gcc dbus_socket_client.c -o client `pkg-config --cflags --libs gio-2.0`
gcc process_a.c -o process_a

# 실행 (3개의 터미널에서)
# 터미널 1:
./process_a

# 터미널 2:
./server

# 터미널 3:
./client
```

### 예상 출력 ###

```
# process_a 출력
Received: Hello from client!

# server 출력
DBus Request: Hello from client!
Received from A: Processed: Hello from client!

# client 출력
Waiting for response...
Received Signal: Processed: Hello from client!
```
