#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <csignal>

std::mutex lock_clients;

std::vector<std::pair<std::string, int>> client_list;

std::unordered_map<std::string, std::vector<int>> groups_to_client;
std::unordered_map<int, std::string> client_to_group;

int server_fd = -1;

void handle_sigint(int sig){
    std::cout<<"Detected exit";
    if (server_fd!=-1) close(server_fd);
    if (client_list.size()!=0){
        for (int i = 0; i<client_list.size(); i++){
            close(client_list[i].second);
        }
    }
    exit(0);
}

void close_client(int client_id){
    close(client_id);
    for (int j = 0; j<client_list.size(); j++){
        if (client_list[j].second==client_id){
            client_list.erase(client_list.begin()+j);
            break;
        }
    }
    if (client_to_group.find(client_id) != client_to_group.end()) {
        std::string group_name = client_to_group[client_id];
        groups_to_client[group_name].erase(std::remove(groups_to_client[group_name].begin(), groups_to_client[group_name].end(), client_id), groups_to_client[group_name].end());
        client_to_group.erase(client_id);
    }
    return;
}

int join_group(int client_id, std::string temp){
    std::string message;
    
    std::string group_name = temp.substr(6); 
    std::lock_guard<std::mutex> locker(lock_clients);
    if (client_to_group.find(client_id)!=client_to_group.end()){
        message = "You are already a part of a group.";
    }
    else{
        if (groups_to_client.find(group_name)==groups_to_client.end()){
            groups_to_client[group_name] = std::vector<int> ();
            message = "Created group " + group_name;
        }
        else{
            message = "Successfully joined group " + group_name;
        }
        groups_to_client[group_name].push_back(client_id);
        client_to_group[client_id] = group_name;
    }

    if (send(client_id, message.c_str(), message.size(), 0)<0){
        perror("send");
        std::lock_guard<std::mutex> locker(lock_clients);
        close_client(client_id);
        return -1;
    }
    return 1;
}

int get_users_list(int client_id){
    std::lock_guard<std::mutex> locker(lock_clients);
    std::string users_list = "";
    if (client_to_group.find(client_id)==client_to_group.end()){
        users_list = "Connected Users:";
        for (int j = 0; j<client_list.size(); j++){
            users_list = users_list + "\n" + std::to_string(j+1) + ". " + client_list[j].first;
        }
    }
    else{
        std::string group_name = client_to_group[client_id];
        users_list = "Users connected to" + group_name + ":";
        for (int j = 0; j<groups_to_client[group_name].size(); j++){
            std::string client;
            int id = groups_to_client[group_name][j];
            for (int k = 0; k<client_list.size(); k++){
                if (client_list[k].second==id){
                    client = client_list[k].first;
                    break;
                }
            }
            users_list = users_list + "\n" + std::to_string(j+1) + ". " + client;
        }
    }
    if (send(client_id, users_list.c_str(), users_list.size(), 0)<0){
        perror("send");
        close_client(client_id);
        return -1;
    }   
    return 1;
}

void client_thread(int client_id){
    std::string ask_username = "Please enter your username: ";
    if (send(client_id, ask_username.c_str(), ask_username.size(), 0)<0){
        perror("send");
        std::lock_guard<std::mutex> locker(lock_clients);
        close_client(client_id);
        return;
    }
    const int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    std::string client_name;
    
    ssize_t received = recv(client_id, &buffer, BUFFER_SIZE, 0);
    if (received<=0){
        std::lock_guard<std::mutex> locker(lock_clients);
        perror("receive");
        close_client(client_id);
        return;
    }
    else{
        buffer[received] = '\0';
        client_name = buffer;
        std::lock_guard<std::mutex> locker(lock_clients);
        client_list.push_back({client_name, client_id});
        std::cout<<client_name<<std::endl;
    }
    std::string welcome_message = "Welcome " + client_name + "! You can use the following commands:\n"
                                   "/users - List all connected users\n"
                                   "/join <group_name> - Join a group\n"
                                   "/groups - List all available groups\n"
                                   "/leave - Leave the current group\n";
    if (send(client_id, welcome_message.c_str(), welcome_message.size(), 0)<0){
        perror("send");
        std::lock_guard<std::mutex> locker(lock_clients);
        close_client(client_id);
        return;
    }

    while (true){
        ssize_t received = recv(client_id, &buffer, BUFFER_SIZE, 0);
        if (received <= 0){
            std::lock_guard<std::mutex> locker(lock_clients);
            if (received < 0) perror("recv failed");
            else std::cout << "Client closed the connection." << std::endl;
            close_client(client_id);
            return;
         }
        buffer[received] = '\0';

        std::string temp = buffer;
        if (temp.substr(0, 6) == "/users"){
            if (get_users_list(client_id)<0) return;
        }
        else if (temp.substr(0, 5) == "/join"){
            if (join_group(client_id, temp)<0) return;
        }
        else if (temp.substr(0, 7) == "/groups"){
            std::lock_guard<std::mutex> locker(lock_clients);
            std::string groups_list = "Available Groups:";
            for (const auto& group : groups_to_client) {
                groups_list += "\n" + group.first + " (" + std::to_string(group.second.size()) + " user/s)";
            }
            if (send(client_id, groups_list.c_str(), groups_list.size(), 0)<0){
                perror("send");
                close_client(client_id);
                return;
            }
        }
        else if (temp.substr(0, 6) == "/leave"){
            std::lock_guard<std::mutex> locker(lock_clients);
            if (client_to_group.find(client_id)!=client_to_group.end()){
                std::string group_name = client_to_group[client_id];
                groups_to_client[group_name].erase(std::remove(groups_to_client[group_name].begin(), groups_to_client[group_name].end(), client_id), groups_to_client[group_name].end());
                client_to_group.erase(client_id);
                std::string message = "You have left the group" + group_name;
                if (send(client_id, message.c_str(), message.size(), 0)<0){
                    perror("send");
                    close_client(client_id);
                    return;
                }
            }
            else{
                std::string message = "You are not part of any group.";
                if (send(client_id, message.c_str(), message.size(), 0)<0){
                    perror("send");
                    close_client(client_id);
                    return;
                }
            }
        }
        else{
            std::lock_guard<std::mutex> locker(lock_clients);
            if (client_to_group.find(client_id)!=client_to_group.end()){
                temp = "[" + client_to_group[client_id] + "] " + client_name + ": " + temp;
                std::string group_name = client_to_group[client_id];
                for (int j = 0; j<groups_to_client[group_name].size(); j++){
                    if (client_id!=groups_to_client[group_name][j]){
                        if (send(groups_to_client[group_name][j], temp.c_str(), temp.size(), 0)<0){
                            int id_to_remove = groups_to_client[group_name][j];
                            perror("send");
                            close_client(id_to_remove);
                        }
                    }
                }
            }
            else{
                temp = "[Global] " + client_name + ": " + temp;
                for (int j = 0; j<client_list.size(); j++){
                    if (client_id!=client_list[j].second){
                        if (send(client_list[j].second, temp.c_str(), temp.size(), 0)<0){
                            perror("send");
                            int id_to_remove = client_list[j].second;
                            close_client(id_to_remove);
                        }
                    }   
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

    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
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
        std::thread t(client_thread, client_id);
        t.detach();
    }
    
    close(server_fd);   
    return 0;
}