#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <poll.h>

using namespace std;
int client_socket_fd = -1;

void handle_sigint(int sig){
    cout<<"detected exit"<<endl;
    if (client_socket_fd!=-1) close(client_socket_fd);
    exit(0);
}

int main(){
    signal(SIGINT, handle_sigint);

    client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_fd<0){
        perror("socket");
        return 1;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    server_addr.sin_port = htons(8081);
    
    if (connect(client_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        close(client_socket_fd);
        perror("connect");
    }
    cout<<"connected to server"<<endl;
    struct pollfd fd_list[2];
    fd_list[0] = {
        client_socket_fd,
        POLLIN
    };
    fd_list[1] = {
        STDIN_FILENO,
        POLLIN
    };

    for (int i = 0; i<=30000; i++){
        int ready = poll(fd_list, 2, -1);
        if (ready < 0){
            close(client_socket_fd);
            break;
        }
        int error_flag = 0;
        for (int i = 0; i<2; i++){
            if (fd_list[i].revents & POLLIN){
                if (fd_list[i].fd == STDIN_FILENO){
                    string answer;
                    cin>>answer;
                    send(client_socket_fd, answer.c_str(), answer.size(), 0);
                }
                else if (fd_list[i].fd == client_socket_fd){
                    const int BUFFER_SIZE = 1024;
                    char buffer[BUFFER_SIZE];
                    ssize_t received = recv(client_socket_fd, &buffer, BUFFER_SIZE, 0);
                    if (received<0){
                        perror("receive");
                        error_flag = 1;
                        close(client_socket_fd);
                        break;
                    }
                    else if (received == 0){
                        perror("connection disconnected");
                        error_flag = 1;
                        close(client_socket_fd);
                        break;
                    }
                    buffer[received] = '\0';
                    cout<<buffer<<endl;
                }
            }
        }
        if (error_flag == 1) break;
    }   
    close(client_socket_fd);
    
    return 0;
}