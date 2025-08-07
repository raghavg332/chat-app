// New version of the client application using ncurses

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <ncurses.h>

int client_socket_fd = -1;
std::atomic<bool> running{true};

void handle_sigint(int sig){
    std::cout << "detected exit" << std::endl;
    if (client_socket_fd != -1) close(client_socket_fd);
    running = false;
    endwin();
    exit(0);
}

void input_thread(int client_socket_fd, WINDOW* input_win, WINDOW* chat_win){
    while (running){
        char str[1024];
        mvwprintw(input_win, 1, 2, "> ");  
        wclrtoeol(input_win);
        wrefresh(input_win);
        wgetnstr(input_win, str, sizeof(str));
        std::string answer = std::string(str);

        wattron(chat_win, A_BOLD);
        wattron(chat_win, COLOR_PAIR(1));
        wprintw(chat_win, "You:");
        wattroff(chat_win, COLOR_PAIR(1));     

        wprintw(chat_win, " %s\n\n", answer.c_str());
        wattroff(chat_win, A_BOLD);
        wrefresh(chat_win);

        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 2, "> ");
        wrefresh(input_win);

        if (send(client_socket_fd, answer.c_str(), answer.size(), 0) < 0){
            running = false;
            close(client_socket_fd);
        }
    }
}

void chat_thread(int client_socket_fd, WINDOW* input_win, WINDOW* chat_win){
    while (running){
        const int BUFFER_SIZE = 1024;
        char buffer[BUFFER_SIZE];
        ssize_t received = recv(client_socket_fd, &buffer, BUFFER_SIZE - 1, 0);
        if (received < 0){
            perror("receive");
            running = false;
            close(client_socket_fd);
            break;
        } else if (received == 0){
            perror("connection disconnected");
            running = false;
            close(client_socket_fd);
            break;
        }
        buffer[received] = '\0';

        std::string msg(buffer);
        size_t colon_pos = msg.find(':');
        size_t bracket_pos = msg.find('[');

        if (bracket_pos != std::string::npos) {
            std::string prefix = msg.substr(0, colon_pos + 1);   
            std::string rest = msg.substr(colon_pos + 1);

            wattron(chat_win, COLOR_PAIR(2));
            wprintw(chat_win, "%s", prefix.c_str());
            wattroff(chat_win, COLOR_PAIR(2));

            wprintw(chat_win, "%s\n\n", rest.c_str());
        } else {
            wprintw(chat_win, "%s\n\n", buffer);
        }

        wrefresh(chat_win);
        wrefresh(input_win);  
    }
}

int main(){
    signal(SIGINT, handle_sigint);

    initscr();
    start_color();
    use_default_colors();  
    init_pair(1, COLOR_CYAN, -1);    
    init_pair(2, COLOR_RED, -1);   
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);
    refresh();

    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);

    WINDOW* chat_win = newwin(term_rows - 5, term_cols, 0, 0);
    scrollok(chat_win, TRUE);
    wrefresh(chat_win);

    WINDOW* input_win = newwin(3, term_cols, term_rows - 3, 0);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "> ");
    wrefresh(input_win);

    // Socket connection
    client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_fd < 0){
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "13.200.235.191", &server_addr.sin_addr);
    server_addr.sin_port = htons(8081);

    if (connect(client_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        close(client_socket_fd);
        perror("connect");
        return 0;
    }

    std::thread t1(input_thread, client_socket_fd, input_win, chat_win);
    std::thread t2(chat_thread, client_socket_fd, input_win, chat_win);

    t1.join();
    t2.join();
    endwin();
    return 0;
}