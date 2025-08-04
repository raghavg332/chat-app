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

void handle_sigint(int sig){
    cout<<"Detected exit";
    if (server_fd!=-1) close(server_fd);
    if (client_list.size()!=0){
        for (int i = 0; i<client_list.size(); i++){
            close(client_list[i].second);
        }
    }
    exit(0);
}


void client_thread(int client_id){
    string ask_username = "Please enter your username: ";
    if (send(client_id, ask_username.c_str(), ask_username.size(), 0)<0){
        perror("send");
        close(client_id);
        return;
    }
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    string client_name;
    
    ssize_t received = recv(client_id, &buffer, BUFFER_SIZE, 0);
    if (received<=0){
        close(client_id);
        perror("receive");
        return;
    }
    else{
        buffer[received] = '\0';
        client_name = buffer;
        lock_guard<mutex> locker(lock_clients);
        client_list.push_back({client_name, client_id});
        cout<<client_name<<endl;
    }
   


    while (true){
        ssize_t received = recv(client_id, &buffer, BUFFER_SIZE, 0);
        if (received < 0){
            perror("receive");
            break;
        }
        else if (received == 0){
            lock_guard<mutex> locker(lock_clients);
            cout<<"client disconnected"<<endl;

            for (int j = 0; j<client_list.size(); j++){
                if (client_list[j].second==client_id){
                    client_list.erase(client_list.begin()+j);
                    break;
                }
            }
            break;
         }
        buffer[received] = '\0';
        cout<<buffer<<endl;
        
        cout<<"sending message"<<endl;
        string temp = buffer;
        temp = "[" + client_name + "] " + temp;
        lock_guard<mutex> locker(lock_clients);
        for (int j = 0; j<client_list.size(); j++){
            if (client_id!=client_list[j].second){
                if (send(client_list[j].second, temp.c_str(), temp.size(), 0)<0){
                    perror("send");
                    break;
                }
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
    server_addr.sin_port = htons(8081);

    
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

    while (true){
        struct sockaddr_in client_socket;
        socklen_t client_addr_len= sizeof(client_socket);

        int client_id = accept(server_fd, (struct sockaddr*)&client_socket, &client_addr_len);
        
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
