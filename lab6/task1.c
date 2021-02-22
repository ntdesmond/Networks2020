#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // for inet_ntop()
#define BUF_SIZE 1024

int find_param(char* param_name, int argc, char* args[]) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(param_name, args[i]) == 0)
            return i;                        // index in the array
    }
    return -1;                               // param not found
}

int main(int argc, char* argv[]) {
    int index_client_param = find_param("-c", argc, argv);
    char* server_addr = NULL;
    int server_port = atoi(argv[argc - 1]);

    if (index_client_param == 1 && argc == 4 && server_port != 0) {
        server_addr = argv[2];
    }
    else if (argc != 2 || server_port == 0) {
        printf("Usage: %s [-c ADDRESS] PORT\n\n", argv[0]);
        printf("\t-c\tEnable client mode, connecting to server on ADDRESS\n");
        printf("\t\tIf not specified, current instance will be the server.\n");
        printf("\t\tNote: use CTRL+D or CTRL+Z to pass EOF in order to send the data in client mode.\n\n");
        printf("\tPORT\tthe port to connect to (client) or to listen on (server)\n");
        printf("\t\tValue must be in range 1-65535\n");
        return 1;
    }

    if (server_addr == NULL) {
        printf("Server mode; listening on %d\n\n", server_port);
        int server_socket = socket(PF_INET, SOCK_STREAM, 0);
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

        int listen_code = listen(server_socket, 10);
        if (listen_code == -1) {
            printf("Failed to start listening\n");
            return 1;
        }

        for (;;) {
            struct sockaddr_in sa_client;
            socklen_t sa_size = sizeof(sa_client);
            int connection_socket = accept(server_socket, (struct sockaddr*)&sa_client, &sa_size);

            if (connection_socket == -1) {
                printf("Failed to connect to the client\n");
                close(server_socket);
                return 1;
            }
            else {
                char client_addr[16] = "<unknown IP>";
                inet_ntop(AF_INET, &(sa_client.sin_addr), client_addr, INET_ADDRSTRLEN);
                printf("Got the new connection from %s\n", client_addr);
            }

            char recv_buffer[BUF_SIZE] = "";
            if (recv(connection_socket, recv_buffer, BUF_SIZE, 0) == -1) {
                printf("Failed to get data from the server\n");
                close(connection_socket);
                close(server_socket);
                return 1;
            }
            else {
                // Echo back
                send(connection_socket, recv_buffer, strlen(recv_buffer), 0);
                printf("Received data: %s (%ld bytes)\n", recv_buffer, strlen(recv_buffer));
                printf("Shutting down and closing the connection\n\n");
            }

            int shutdown_code = shutdown(connection_socket, SHUT_RDWR);
            if (shutdown_code == -1) {
                printf("Failed to shutdown the connection properly\n");
                close(connection_socket);
                close(server_socket);
                return 1;
            }
            close(connection_socket);
        }
        printf("Closing the server\n");
        close(server_socket);
    }
    else {
        printf("Client mode; server == %s:%d\nEOF is required to send the data (CTRL+D or CTRL+Z)\n\n", server_addr, server_port);
        int client_socket = socket(PF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            printf("Failed to create the client socket\n");
            return 1;
        }

        struct sockaddr_in sa;
        //memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(server_port);
        int res = inet_pton(AF_INET, server_addr, &sa.sin_addr);

        int connect_code = connect(client_socket, (struct sockaddr*)&sa, sizeof(sa));
        if (connect_code == -1) {
            printf("Failed to connect to the server\n");
            return 1;
        }

        char buffer[BUF_SIZE] = "";
        fread(buffer, 1, BUF_SIZE, stdin);
        send(client_socket, buffer, strlen(buffer), 0);

        printf("Sent %ld bytes, waiting for reply\n", strlen(buffer));
        if (recv(client_socket, buffer, BUF_SIZE, 0) == -1) {
            printf("Failed to get the reply from the server\n");
            return 1;
        }
        else {
            printf("Got the reply: %s (%ld bytes)\nClosing the connection\n", buffer, strlen(buffer));
        }
        close(client_socket);
    }
}
