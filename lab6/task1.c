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
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(dst_port);
    if (inet_pton(AF_INET, dst_addr, &sa.sin_addr) != 1) {
        printf("Failed to convert the address\n");
        return -1;
    };
    int client_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        printf("Failed to create the client socket\n");
        return -1;
    }

    int connect_code = connect(client_socket, (struct sockaddr*)&sa, sizeof(sa));
    if (connect_code == -1) {
        printf("Failed to connect to the server\n");
        return -1;
    }

    send(client_socket, request, strlen(request), 0);
    printf("Sent %ld bytes, waiting for reply\n", strlen(request)); 
    if (recv(client_socket, response, BUF_SIZE, 0) == -1) {
        printf("Failed to get the reply from the server\n");
        return -1;
    }
    else {
        printf("Got the reply: %s (%ld bytes)\nClosing the connection with the server %s:%d\n",
               response, strlen(response), dst_addr, dst_port);
    }

    close(client_socket);
    return strlen(response);
}

int main(int argc, char* argv[]) {
    int index_client_param = find_param("-c", argc, argv);
    int index_forward_param = find_param("-f", argc, argv);

    char* server_addr = NULL, *forward_addr = NULL;
    int server_port = atoi(argv[argc - 1]), forward_port = 0;
    
    if (index_forward_param == 1 && index_client_param == -1 && argc == 5) {
        forward_addr = argv[2];
        forward_port = atoi(argv[3]);
    }
    else if (index_client_param == 1 && index_forward_param == -1 && argc == 4 && server_port != 0) {
        server_addr = argv[2];
    }
    else if (argc != 2 || server_port == 0 || index_forward_param == 1 && forward_port == 0) {
        printf("Usage: %s [-c ADDRESS | -f ADDRESS PORT] PORT\n\n", argv[0]);
        printf("\t-c\tEnable client mode, connecting to server on ADDRESS\n");
        printf("\t\tIf not specified, current instance will be the server.\n");
        printf("\t\tNote: use CTRL+D or CTRL+Z to pass EOF in order to send the data in client mode.\n\n");
        printf("\t-f\tEnable forwarding (acting as a new client after getting data from a client)\n");
        printf("\t\tNote: Incompatible with -c option.\n\n");
        printf("\tADDRESS\tthe server's address to connect/forward to)\n");
        printf("\t\tAddress must be in the IPv4 format (xxx.xxx.xxx.xxx).\n\n");
        printf("\tPORT\tthe port to connect/forward to or to listen on (in server mode)\n");
        printf("\t\tValue must be in range 1-65535\n");
        return 1;
    }

    if (server_addr == NULL) {
        printf("Server mode; listening on port %d", server_port);
        if (forward_addr != NULL) {
            printf("; forwarding data to %s:%d", forward_addr, forward_port);
        }
        printf("\n\n");

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
            char client_addr[16] = "<unknown IP>";
            int client_port = 0;

            if (connection_socket == -1) {
                printf("Failed to connect to the client, closing the server\n");
                close(server_socket);
                return 1;
            }
            else {
                // Get the IP
                inet_ntop(AF_INET, &(sa_client.sin_addr), client_addr, INET_ADDRSTRLEN);
                // Get the port
                client_port = ntohs(sa_client.sin_port);
                printf("Got the new connection from %s:%d\n", client_addr, client_port);
            }

            char recv_buffer[BUF_SIZE] = "";
            if (recv(connection_socket, recv_buffer, BUF_SIZE, 0) == -1) {
                printf("Failed to get data from the client, closing the server\n");
                close(connection_socket);
                close(server_socket);
                return 1;
            }
            else {
                printf("Received data: %s (%ld bytes)\n", recv_buffer, strlen(recv_buffer));
                if (forward_addr == NULL) {
                    // Echo back
                    send(connection_socket, recv_buffer, strlen(recv_buffer), 0);
                }
                else {
                    // Pass data to the 2nd server and get the response
                    printf("Forwarding data to %s:%d and waiting for the response\n", forward_addr, forward_port);

                    char final_response[BUF_SIZE] = "";
                    if (client_get_response(forward_addr, forward_port, recv_buffer, final_response) == -1) {
                        printf("Closing the server due to the error\n");
                        close(connection_socket);
                        close(server_socket);
                        return 1;
                    }

                    // Send the received data to the client
                    send(connection_socket, final_response, strlen(final_response), 0);
                }
                printf("Sent the data back to the client %s:%d\n", client_addr, client_port);
                printf("Shutting down and closing the connection with %s:%d\n\n", client_addr, client_port);
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

        char buffer[BUF_SIZE] = "", response[BUF_SIZE] = "";
        fread(buffer, 1, BUF_SIZE, stdin);
        printf("\n");
        if (client_get_response(server_addr, server_port, buffer, response) == -1) {
            return 1; // error already reported, just quit
        }
    }
}
