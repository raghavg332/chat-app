#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket, client_socket[MAX_CLIENTS], max_sd, sd, activity;
    int opt = 1;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    fd_set readfds;

    // Initialize all client_socket[] to 0
    for (int i = 0; i < MAX_CLIENTS; i++)
        client_socket[i] = 0;

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set options
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    // Define address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    std::cout << "Listening on port " << PORT << std::endl;

    // Start listening
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (true) {
        FD_ZERO(&readfds);

        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add client sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Wait for activity
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        // Incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            std::cout << "New connection, socket fd is " << new_socket
                      << ", IP: " << inet_ntoa(address.sin_addr)
                      << ", Port: " << ntohs(address.sin_port) << std::endl;

            // Add to client sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    std::cout << "Added to client list at index " << i << std::endl;
                    break;
                }
            }
        }

        // IO operations on clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];

            if (FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Connection closed
                    getpeername(sd, (struct sockaddr*)&address, &addrlen);
                    std::cout << "Client disconnected, IP: "
                              << inet_ntoa(address.sin_addr)
                              << ", Port: " << ntohs(address.sin_port) << std::endl;
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    // Broadcast message
                    buffer[valread] = '\0';
                    std::cout << "Received: " << buffer;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (client_socket[j] != 0 && client_socket[j] != sd) {
                            send(client_socket[j], buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
