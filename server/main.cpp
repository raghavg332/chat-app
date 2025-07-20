#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <mutex>

using namespace std;

mutex lock_clients;

vector<pair<string, int>> client_list;

int server_fd = -1;
int client_id = -1;

void handle_sigint(int sig){
    cout<<"Detected exit";
    if (server_fd!=-1) close(server_fd);
    if (client_id!=-1) close(client_id);
    exit(0);
}


void client_thread(int client_id){
    signal(SIGINT ,handle_sigint);
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    
    for (int i = 0; i<=30000; i++){
        ssize_t received = recv(client_id, &buffer, BUFFER_SIZE, 0);
        if (received < 0){
            perror("receive");
            break;
        }
        else if (received == 0){
            cout<<"client disconnected"<<endl;
            break;
         }
        buffer[received] = '\0';
        cout<<buffer<<endl;

        cout<<"sending message"<<endl;
        for (int j = 0; j<client_list.size(); j++){
                if (send(client_list[j].second, &buffer, received, 0)<0){
                perror("send");
                break;
            }
        }
    }
    close(client_id);
    return;
}


int main() {
    signal(SIGINT, handle_sigint);


    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd<0){
        perror("socket");
        return 1;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    
    if (::bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        close(server_fd);
        perror("bind failed");
        return 1;
    }

    if (listen(server_fd, 5) <0){
        close(server_fd);
        perror("listen failed");
        return 1;
    }

    for (int i = 0; i<2000; i++){
        struct sockaddr_in client_socket;
        socklen_t client_addr_len= sizeof(client_socket);

        client_id = accept(server_fd, (struct sockaddr*)&client_socket, &client_addr_len);
        client_list.push_back({"client"+to_string(i), client_id});

        if (client_id<0){
            close(server_fd);
            perror("accept");
            return 1;
        }
        thread t(client_thread, client_id);
        t.detach();
    }
    
    close(server_fd);   
    return 0;
}
