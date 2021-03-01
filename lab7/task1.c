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

int client_get_response(char* dst_addr, int dst_port, char* request, char* response) {
}

int main(int argc, char* argv[]) {
    int index_client_param = find_param("-c", argc, argv);

    char* server_addr = NULL, *forward_addr = NULL;
    int server_port = atoi(argv[argc - 1]), forward_port = 0;
    
    if (index_client_param == 1 && argc == 4 && server_port != 0) {
        server_addr = argv[2];
    }
    else if (argc != 2) {
        printf("Usage: %s [-c ADDRESS | -f ADDRESS PORT] PORT\n\n", argv[0]);
        printf("\t-c\tEnable client mode, connecting to server on ADDRESS\n");
        printf("\t\tIf not specified, current instance will be the server.\n");
        printf("\tADDRESS\tthe server's address to connect/forward to)\n");
        printf("\t\tAddress must be in the IPv4 format (xxx.xxx.xxx.xxx).\n\n");
        printf("\tPORT\tthe port to connect/forward to or to listen on (in server mode)\n");
        printf("\t\tValue must be in range 1-65535\n");
        return 1;
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

        for (;;) {
            struct sockaddr_in sa_client;
            socklen_t sa_size = sizeof(sa_client);
            char client_addr[16] = "<unknown IP>";
            int client_port = 0;

            char recv_buffer[BUF_SIZE] = "";
            if (recvfrom(server_socket, recv_buffer, BUF_SIZE, 0, (struct sockaddr*)&sa_client, &sa_size) == -1) {
                printf("Failed to read data on port %d, closing the server\n", server_port);
                close(server_socket);
                return 1;
            }
            else {
                // Get the IP
                inet_ntop(AF_INET, &(sa_client.sin_addr), client_addr, INET_ADDRSTRLEN);
                // Get the port
                client_port = ntohs(sa_client.sin_port);
                printf("(%s:%d): %s\n", client_addr, client_port, recv_buffer);
                sendto(server_socket, recv_buffer, strlen(recv_buffer), 0, (struct sockaddr*)&sa_client, sa_size);
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
            return -1;
        };
        int client_socket = socket(PF_INET, SOCK_DGRAM, 0);
        if (client_socket == -1) {
            printf("Failed to create the client socket\n");
            return -1;
        }

        for (;;) {
            char request[BUF_SIZE] = "", response[BUF_SIZE] = "";
            fgets(request, BUF_SIZE, stdin);

            sendto(client_socket, request, strlen(request), 0, (struct sockaddr*)&sa, sa_size);
            if (recvfrom(client_socket, response, BUF_SIZE, 0, (struct sockaddr*)&sa, &sa_size) == -1) {
                printf("Failed to get the reply from the server\n");
                return -1;
            }
            else {
                if (strcmp(request, response) == 0)
                    printf("OK\n");
                else
                    printf("%s\n", response);
            }
        }
        close(client_socket);
    }
    return 0;
}
