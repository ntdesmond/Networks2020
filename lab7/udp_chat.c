
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // for inet_ntop()
#define BUF_SIZE 1000
#define MAX_CLIENTS 100
#define ADDR_LEN 21 // len("xxx.xxx.xxx.xxx:nnnnn")

const char hello_msg[] =  "/join";
char prompt[] =  "chat> ";          // not const since might be disabled (set to "")

// Passing args to pthread using this struct. See message_receiver(void*)
struct msg_receiver_args {
    int socket;              // socket to use
    struct sockaddr* sa;     // client info to use with recvfrom
    socklen_t* sa_size;
    char* typed_message;     // the part of message typed by client, but not sent yet
    int* ret_value;          // the return value of the thread
};


int find_param(char* param_name, int argc, char* args[]) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(param_name, args[i]) == 0)
            return i;                        // index in the array
    }
    return -1;                               // param not found
}

int find_client(struct sockaddr_in* client, int length, struct sockaddr_in* clients_sa) {
    for (int i = 0; i < length; i++) {
        if (
            client->sin_addr.s_addr == clients_sa[i].sin_addr.s_addr &&
            client->sin_port == clients_sa[i].sin_port
        ) {
            return i;                        // index in the array
        }
    }
    return -1;                               // client not found
}

void* message_receiver(void* args) {
    for (;;) {
        char response[BUF_SIZE] = "";
        if (recvfrom(
            ((struct msg_receiver_args*)args)->socket, response, BUF_SIZE, 0,
            ((struct msg_receiver_args*)args)->sa, ((struct msg_receiver_args*)args)->sa_size
        ) == -1) {
            printf("Failed to get the reply from the server\n");
            *((struct msg_receiver_args*)args)->ret_value = 1;
            return NULL;
        }
        else {
             printf("\r%s", response);                                                   // \r would overwrite the message on edit
             int diff = strlen(prompt) + strlen(((struct msg_receiver_args*)args)->typed_message) - strlen(response);
             for (int i = 0; i < diff; i++)
                 printf(" ");                                                            // so we erase it to it's full length   
             printf("\n%s%s", prompt, ((struct msg_receiver_args*)args)->typed_message); // and restore it afterwards
             fflush(stdout);
        }
    }
}

int main(int argc, char* argv[]) {
    int index_client_param = find_param("-c", argc, argv);
    int quiet_mode_param = find_param("-q", argc, argv);
    int show_client_port_param = find_param("-vp", argc, argv);
    int keep_newlines_param = find_param("-kn", argc, argv);

    char* server_addr = NULL;
    int server_port = atoi(argv[argc - 1]);
    
    if (
        index_client_param > 0 && (show_client_port_param > 0 || (argc - index_client_param) != 3) || 
        server_port < 1 || server_port > 65535 || argc < 2 || argc > 5
    ) {
        printf("Usage: %s [-vp] [-q] [-c ADDRESS] PORT\n\n", argv[0]);
        printf("\tServer mode options:\n");
        printf("\t-vp\tShow client's port together with IP address\n");
        printf("\t-kn\tKeep newlines in the messages (by default, incoming messages are split by \\n)\n");
        printf("\t-q\tDo not react to 'hello' messages (just skip them)\n\n");
        printf("\tClient mode options:\n");
        printf("\t-c\tEnable client mode, connecting to server on ADDRESS\n");
        printf("\t\tIf not specified, current instance will be the server.\n");
        printf("\t-q\tDo not send a 'hello' message at the start\n");
        printf("\t\tNote: you'll not receive messages unless you send your first message.\n\n");
        printf("\tADDRESS\tthe server's address to connect to)\n");
        printf("\t\tAddress must be in the IPv4 format (xxx.xxx.xxx.xxx).\n\n");
        printf("\tPORT\tthe port to connect to or to listen on (in server mode)\n");
        printf("\t\tValue must be in range 1-65535\n");
        return 1;
    }
    if (index_client_param > 0) {      // client-mode => extract address
        server_addr = argv[argc - 2];
    }

    if (server_addr == NULL) {
        printf("Server mode; listening on port %d\n\n", server_port);

        int server_socket = socket(PF_INET, SOCK_DGRAM, 0);
        if (server_socket == -1) {
            printf("Failed to create the server socket\n");
            return 1;
        }

        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(server_port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);

        int bind_code = bind(server_socket, (struct sockaddr*) &sa, sizeof(sa));
        if (bind_code == -1) {
            printf("Failed to bind the socket\n");
            return 1;
        }
        
        // Array to store clients
        struct sockaddr_in clients[MAX_CLIENTS];
        int clients_known = 0;

        for (;;) {
            struct sockaddr_in sa_client;
            socklen_t sa_size = sizeof(sa_client);
            char client_addr[ADDR_LEN] = "<unknown IP>";

            char recv_buffer[BUF_SIZE] = "";
            if (recvfrom(server_socket, recv_buffer, BUF_SIZE, 0, (struct sockaddr*)&sa_client, &sa_size) == -1) {
                printf("Failed to read data on port %d, closing the server\n", server_port);
                close(server_socket);
                return 1;
            }

            // Save the client's info
            if (find_client(&sa_client, clients_known, clients) == -1) {
                clients[clients_known] = sa_client;
                clients_known++;
                clients_known %= MAX_CLIENTS; 
            }

            // Get the IP
            inet_ntop(AF_INET, &(sa_client.sin_addr), client_addr, INET_ADDRSTRLEN);
            if (show_client_port_param > 0) {
                // Get the port (snprintf for quick int to str conversion)
                snprintf(client_addr+strlen(client_addr), 6, ":%d", ntohs(sa_client.sin_port));
            }

            char send_buffer[ADDR_LEN + BUF_SIZE] = "";

            if (strcmp(recv_buffer, hello_msg) == 0) {        // 'hello' message handling
                if (quiet_mode_param > 0)                     // ignore it if '-q' is set
                    continue;
                strcat(send_buffer, "INFO> ");
                strcat(send_buffer, client_addr);
                strcat(send_buffer, " joined the chat");
            }
            else {
                if (keep_newlines_param > 0) {
                    strcat(send_buffer, client_addr);
                    strcat(send_buffer, "> ");
                    strcat(send_buffer, recv_buffer);
                }
                else {
                    char* line = strtok(recv_buffer, "\n");
                     while (line != NULL) {
                        strcat(send_buffer, client_addr);
                        strcat(send_buffer, "> ");
                        strcat(send_buffer, line);                       
                        line = strtok(NULL, "\n");
                        if (line != NULL)                   // Add '\n' after all lines except the last one
                            strcat(send_buffer, "\n");                        
                    }
                }
            }
            printf("%s\n", send_buffer);
            // Send the message to every client
            for (int i = 0; i < clients_known; i++) {
                sendto(server_socket, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&clients[i], sizeof(clients[i]));
            }
        }
        printf("Closing the server\n");
        close(server_socket);
    }
    else {
        printf("Client mode; server == %s:%d\n\n", server_addr, server_port);

        struct sockaddr_in sa;
        socklen_t sa_size = sizeof(sa);
        sa.sin_family = AF_INET;
        sa.sin_port = htons(server_port);
        if (inet_pton(AF_INET, server_addr, &sa.sin_addr) != 1) {
            printf("Failed to convert the address\n");
            return 1;
        };
        int client_socket = socket(PF_INET, SOCK_DGRAM, 0);
        if (client_socket == -1) {
            printf("Failed to create the client socket\n");
            return 1;
        }

        char request[BUF_SIZE] = "", response[BUF_SIZE] = "";
        int receiver_status = 0;

        if (system("[[ $- == *i* ]] && stty -icanon min 1") != 0)  // to read stdin without waiting for '\n'
            strcpy(prompt, "");                                    // Disable prompt for non-interactive mode

        // Start getting incoming messages
        pthread_t tid;
        struct msg_receiver_args receiver_args;
        receiver_args.socket = client_socket;
        receiver_args.sa = (struct sockaddr*)&sa;
        receiver_args.sa_size = &sa_size;
        receiver_args.typed_message = request;
        receiver_args.ret_value = &receiver_status;

        if (pthread_create(&tid, NULL, &message_receiver, (void*)&receiver_args) != 0) {
            printf("Unable to start the message receiver thread\n");
            return 1;
        }


        // 'hello' message (if not disabled)
        if (quiet_mode_param == -1) {
            sendto(client_socket, hello_msg, strlen(hello_msg), 0, (struct sockaddr*)&sa, sa_size);
        }
        printf("%s", prompt);

        // Send messages
        for (;;) {
            if (receiver_status != 0)    // Error in the message receiver
                return 1;
            memset(request, 0, strlen(request));           // Clean the message
            char c;
            while (strlen(request) < BUF_SIZE && request[strlen(request) - 1] != '\n') {
                c = getchar();                             // Read char by char from stdin
                if (c == -1) {
                    break;
                }
                else if (c == 8 || c == 127) {             // backspace or del character (yes, now this is our burden)
                    if (strlen(request) > 0) {
                        printf("\b\b\b   \b\b\b");         // erase "C^?" (replace char C with a space)
                        request[strlen(request) - 1] = 0;  // remove the last character from the string
                    }
                    else {
                        printf("\b\b  \b\b");              // erase "^?" only
                    } 
                }
                else {
                    request[strlen(request)] = c;
                }
            }
            if (strlen(request) > 0)
                sendto(client_socket, request, strlen(request) - 1, 0, (struct sockaddr*)&sa, sa_size); // -1 from length to trim '\n'
        }
        close(client_socket);
    }
    return 0;
}
