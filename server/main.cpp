#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    cout<<server_fd;
    cout<<"\n";
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);


    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    struct sockaddr* client_socket;
    socklen_t* client_socklen;

    int client_socket_connect = accept(server_fd, client_socket, client_socklen);
    send(client_socket_connect, "hello_client", 13, 0);
    close(client_socket_connect);
    return 0;
}
