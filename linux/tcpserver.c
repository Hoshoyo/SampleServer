#include <windows.h>
#include <stdio.h>
#include "../network.h"

#define MAX_CLIENTS 10
static TCP_Connection clients[MAX_CLIENTS];
static int client_index;
static int running;

unsigned int WINAPI receive_thread(void* lpParameter)
{
    while(running)
    {
        for(int i = 0; i < MAX_CLIENTS; ++i) {
            if(clients[i].socket > 0) {
                TCP_Packet packet = {0};
                Net_Status status = network_receive_tcp_packets(&clients[i], &packet);
                if(status > 0) {
                    printf("received %d bytes from client %d\n", status, i);
                    // Send back data possibly...                    
                } else if (status == NETWORK_CONN_CLOSED) {
                    printf("client %d closed connection\n", i);
                    network_close_tcp_connection(&clients[i]);
                    clients[i].socket = 0;
                } else if (status != NETWORK_PACKET_NONE) {
                    printf("receive error: %d\n", status);
                }
            }
        }
        Sleep(10);
    }

    return 0;
}

// windows only
void create_receive_thread()
{
    unsigned int thread_id = 0;
    HANDLE recv_thread = CreateThread(0, 0, receive_thread, 0, 0, &thread_id);
}

int main()
{
    if(network_init(stdout) == NETWORK_OK) 
    {
        TCP_Connection listen_conn = {0};
        if(network_create_tcp_bound_socket(&listen_conn, 8888, 0) != NETWORK_OK)
            return -1;

        create_receive_thread();

        running = 1;

        Net_Status err = NETWORK_OK;
        while(running)
        {
            err = network_listen(&listen_conn, MAX_CLIENTS);
            if(err == NETWORK_OK) {
                err = network_accept(&listen_conn, &clients[client_index]);
                if(err == NETWORK_OK) {
                    network_socket_set_async((UDP_Connection*)&clients[client_index].socket);
                    printf("Client connected %d!\n", client_index);
                    client_index = (client_index + 1) % MAX_CLIENTS;
                } else {
                    printf("Failed to accept connection Error=%d", err);
                }
            } else {
                printf("Failed to listen, exiting. Error=%d", err);
                return -1;
            }
        }
    }
    return 0;
}