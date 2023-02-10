#include "../network.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

/*
  Network
*/

static FILE* net_log_stream;

int
network_destroy()
{
    return 0;
}

int
network_init(FILE* stream)
{
    net_log_stream = stream;
    fprintf(net_log_stream, "linux sockets library initialized\n");
    return 0;
}

int
network_socket_set_timeout(UDP_Connection* out_conn, double time_ms)
{
    struct timeval timeout = { 0 };
    timeout.tv_sec = (long int)(time_ms / 1000.0);
    timeout.tv_usec = (long int)(1000.0 * (time_ms - (timeout.tv_sec * 1000.0)));
    int s = setsockopt(out_conn->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    return s;
}

int
network_socket_set_async(UDP_Connection* out_conn)
{
    int flags = fcntl(out_conn->socket, F_GETFL, 0);
    flags &= ~(O_NONBLOCK);
    if (fcntl(out_conn->socket, F_SETFL, flags) != 0) {
        fprintf(net_log_stream, "Could not set peer socket to non blocking: %s\n", strerror(errno));
        return -1;
    }
    out_conn->flags |= NETWORK_FLAG_SOCKET_ASYNC;
    return 0;
}

int
network_create_udp_socket(UDP_Connection* out_conn, int async)
{
    // create socket
    int connection_socket = 0;
    if ((connection_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(net_log_stream, "failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    if (async) network_socket_set_async(out_conn);
    fprintf(net_log_stream, "successfully created socket\n");

    out_conn->socket = connection_socket;
    out_conn->port = 0;

    return 0;
}

int
network_create_udp_bound_socket(UDP_Connection* out_conn, unsigned short port, int async)
{
    // create socket
    int connection_socket = 0;
    if ((connection_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(net_log_stream, "failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    if (async) network_socket_set_async(out_conn);
    fprintf(net_log_stream, "successfully created socket\n");

    // bind it to port
    struct sockaddr_in server_info = { 0 };
    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(port);

    if (bind(connection_socket, (struct sockaddr*)&server_info, sizeof(server_info)) < 0)
    {
        fprintf(net_log_stream, "failed to bind udp socket to port %d: %s\n", port, strerror(errno));
        close(connection_socket);
        return -1;
    }
    fprintf(net_log_stream, "successfully bound udp socket to port %d\n", port);
    if (out_conn)
    {
        out_conn->socket = connection_socket;
        out_conn->port = port;
    }
    return 0;
}

Net_Status
network_receive_udp_packets_from_peer(UDP_Connection* udp_conn, struct sockaddr_in* peer, UDP_Packet* out_packet)
{
    int len = sizeof(out_packet->data);
    int n = recvfrom(udp_conn->socket, (char*)out_packet->data, len,
        (udp_conn->flags & NETWORK_FLAG_SOCKET_ASYNC) ? MSG_DONTWAIT : 0, (struct sockaddr*)peer, &len);

    switch (n)
    {
    case -1: {
        if (errno == EAGAIN)
        {
            // this is an async socket and no data is available
            if (udp_conn->flags & NETWORK_FLAG_SOCKET_ASYNC)
                return NETWORK_PACKET_NONE;
            else
                return NETWORK_CONN_TIMEOUT;
        }
        else return NETWORK_PACKET_ERROR;
    } break;
    case 0:  return NETWORK_FORCED_SHUTDOWN;
    default: {
        out_packet->length_bytes = n;
        return n;
    }
    }
}

Net_Status
network_receive_udp_packets_from_addr(UDP_Connection* udp_conn, const char* ip, int port, UDP_Packet* out_packet)
{
    struct sockaddr_in servaddr = { 0 };

    // Filling server information 
    servaddr.sin_family = AF_INET; // IPv4 
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &servaddr.sin_addr.s_addr);

    int len = sizeof(out_packet->data);
    int n = recvfrom(udp_conn->socket, (char*)out_packet->data, len,
        (udp_conn->flags & NETWORK_FLAG_SOCKET_ASYNC) ? MSG_DONTWAIT : 0, (struct sockaddr*)&servaddr, &len);

    switch (n)
    {
    case -1: {
        if (errno == EAGAIN)
        {
            // this is an async socket and no data is available
            if (udp_conn->flags & NETWORK_FLAG_SOCKET_ASYNC)
                return NETWORK_PACKET_NONE;
            else
                return NETWORK_CONN_TIMEOUT;
        }
        else return NETWORK_PACKET_ERROR;
    } break;
    case 0:  return NETWORK_FORCED_SHUTDOWN;
    default: return n;
    }
}

int
network_send_udp_packet(UDP_Connection* udp_conn, struct sockaddr_in* to, const char* data, int length)
{
    return sendto(udp_conn->socket, data, length, MSG_DONTWAIT, (struct sockaddr*)to, sizeof(struct sockaddr));
}

Net_Status
network_receive_udp_packets(UDP_Connection* udp_conn, UDP_Packet* out_packet)
{
#define BUFFER_SIZE 1024
    char buffer[BUFFER_SIZE] = { 0 };
    struct sockaddr_in client_info = { 0 };
    int client_info_size_bytes = sizeof(client_info);

    int flags = (udp_conn->flags & NETWORK_FLAG_SOCKET_ASYNC) ? MSG_DONTWAIT : 0;
    int status = recvfrom(udp_conn->socket, buffer, BUFFER_SIZE, flags, (struct sockaddr*)&client_info, &client_info_size_bytes);

    switch (status)
    {
    case -1: {
        if (errno == EAGAIN)
        {
            // this is an async socket and no data is available
            if (udp_conn->flags & NETWORK_FLAG_SOCKET_ASYNC)
                return NETWORK_PACKET_NONE;
            else
                return NETWORK_CONN_TIMEOUT;
        }
        else return NETWORK_PACKET_ERROR;
    } break;
    case 0:  return NETWORK_FORCED_SHUTDOWN;
    default: {
        #if 0
        printf("received message(%d bytes) from %s:%d\n", status, inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));
        #endif
        out_packet->sender_info = client_info;
        out_packet->length_bytes = status;
        memcpy(out_packet->data, buffer, out_packet->length_bytes);
        out_packet->data[out_packet->length_bytes] = 0;
        return out_packet->length_bytes;
    } break;
    }
}

int
network_close_udp_connection(UDP_Connection* udp_conn)
{
    return close(udp_conn->socket);
}

int
network_close_tcp_connection(TCP_Connection* tcp_conn)
{
    return close(tcp_conn->socket);
}

void
network_print_ipv4(unsigned int ip)
{
    fprintf(net_log_stream, "%d.%d.%d.%d", (ip & 0xff), ((ip & 0xff00) >> 8), ((ip & 0xff0000) >> 16), ((ip & 0xff000000) >> 24));
}

void
network_print_port(unsigned short port)
{
    fprintf(net_log_stream, "%d", (port >> 8) | ((port & 0xff) << 8));
}

int
network_dns_ipv4(const char* server_addr, ipv4_t* out_ip)
{
    struct sockaddr_in result = { 0 };
    struct hostent* h = gethostbyname(server_addr);
    if (!h)
    {
        fprintf(net_log_stream, "could not perform dns lookup: %s\n", strerror(errno));
        return -1;
    }
    char** aux = h->h_addr_list;
    if (*aux)
    {
        *out_ip = *(unsigned int*)*aux;
    }
    return 0;
}

const char*
network_host_string(struct sockaddr_in* host)
{
    static char buffer[256];
    unsigned int ip = host->sin_addr.s_addr;
    unsigned short port = host->sin_port;
    sprintf(buffer, "%d.%d.%d.%d:%d\0",
        (ip & 0xff), ((ip & 0xff00) >> 8), ((ip & 0xff0000) >> 16), ((ip & 0xff000000) >> 24),
        (port >> 8) | ((port & 0xff) << 8));
    return buffer;
}

int
network_addr_equal(struct sockaddr_in* a1, struct sockaddr_in* a2)
{
    return (a1->sin_addr.s_addr == a2->sin_addr.s_addr && a1->sin_port == a2->sin_port);
}

// TCP

int 
network_create_tcp_socket(TCP_Connection* out_conn, int async)
{
    int connection_socket = 0;
    if ((connection_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(net_log_stream, "Failed to create tcp socket: %s\n", strerror(errno));
        return -1;
    }

    out_conn->socket = connection_socket;
    out_conn->port = 0;

    if (async)
    {
        if (network_socket_set_async((UDP_Connection*)out_conn) == -1) return -1;
    }

    fprintf(net_log_stream, "Created net tcp socket (%s)\n", (async) ? "async" : "blocking");
    return 0;
}

int
network_create_tcp_bound_socket(TCP_Connection* out_conn, unsigned short port, int async)
{
    // create
    int connection_socket = 0;
    if ((connection_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(net_log_stream, "Failed to create tcp socket: %s\n", strerror(errno));
        return -1;
    }
    out_conn->socket = connection_socket;

    if (async)
    {
        if (network_socket_set_async((UDP_Connection*)out_conn) == -1) return -1;
    }

    fprintf(net_log_stream, "Created net tcp socket (%s) bound to %d\n", (async) ? "async" : "blocking", port);

    // bind it to port
    struct sockaddr_in server_info = { 0 };
    server_info.sin_family = AF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(port);

    if (bind(connection_socket, (struct sockaddr*)&server_info, sizeof(server_info)) == -1)
    {
        fprintf(net_log_stream, "Failed to bind socket to port %d: %s\n", port, strerror(errno));
        close(connection_socket);
        return -1;
    }

    fprintf(net_log_stream, "Bound net tcp socket (%s) to %d\n", (async) ? "async" : "blocking", port);

    if (out_conn)
    {
        out_conn->socket = connection_socket;
        out_conn->port = port;
    }
    return 0;
}

Net_Status
network_connect_tcp(TCP_Connection* conn, struct sockaddr_in* addr)
{
    if (connect(conn->socket, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) != 0) 
    {
        Net_Status result = NETWORK_OK;
        switch(errno)
        {
            case EACCES:         result = NETWORK_ACCESS_DENIED;
            case ENOTSOCK:
            case EBADF:          result = NETWORK_UNINITIALIZED;
            case EADDRINUSE:     result = NETWORK_ADDR_IN_USE;
            case EFAULT:
            case EAFNOSUPPORT:
            case EADDRNOTAVAIL:  result = NETWORK_INVALID_ADDRESS;
            case ECONNREFUSED:   result = NETWORK_CONN_REFUSED;
            case EISCONN:        result = NETWORK_ALREADY_CONNECTED;
            case ENETUNREACH:    result = NETWORK_UNREACHABLE;
            case ETIMEDOUT:      result = NETWORK_CONN_TIMEOUT;
            case EALREADY:
            case EINPROGRESS:
            case EAGAIN:    result = NETWORK_PACKET_NONE;
            default: {
                fprintf(net_log_stream, "Failed to connect to server: %s\n", strerror(errno));
                return NETWORK_ERROR;
            };
        }
        return -1;
    }
    conn->addr = *addr;
    return 0;
}

Net_Status
network_send_tcp_packet(TCP_Connection* tcp_conn, const char* data, int length)
{
    int bytes_sent = send(tcp_conn->socket, data, length, 0);
    if (bytes_sent == -1)
    {
        switch (errno)
        {
            case EACCES:      return NETWORK_ACCESS_DENIED;            
            case ENOTSOCK:
            case EFAULT:
            case EBADF:       return NETWORK_UNINITIALIZED;
            case ECONNRESET:  return NETWORK_CONN_RESET_BY_PEER;
            case EINVAL:      return NETWORK_INVALID_ADDRESS;
            case EWOULDBLOCK: return NETWORK_PACKET_NONE;
            default: return NETWORK_PACKET_ERROR;
        }
    }
    return bytes_sent;
}

Net_Status
network_receive_tcp_packets(TCP_Connection* tcp_conn, TCP_Packet* out_packet)
{
    char buffer[PACKET_SIZE] = { 0 };

    int status = recv(tcp_conn->socket, buffer, PACKET_SIZE, 0);
    if (status == 0)
    {
        // Connection was gracefully closed
        return NETWORK_CONN_CLOSED;
    }
    else if (status == -1)
    {
        switch (errno)
        {
            case ENOTSOCK:
            case ENOTCONN:
            case EBADF:        return NETWORK_UNINITIALIZED;
            case ECONNREFUSED: return NETWORK_CONN_REFUSED;
            case EINVAL:       return NETWORK_INVALID_ADDRESS;
            case EWOULDBLOCK:  return NETWORK_PACKET_NONE;

            // Errors that should not happen
            default: {
                fprintf(net_log_stream, "Unexpected error code receiving packet %s\n", strerror(errno));
                return NETWORK_PACKET_ERROR;
            }
        }
    }
    else
    {
        out_packet->length_bytes = status;
        memcpy(out_packet->data, buffer, out_packet->length_bytes);
        assert(out_packet->length_bytes < PACKET_SIZE);
        out_packet->data[out_packet->length_bytes] = 0;
        return out_packet->length_bytes;
    }
}

int
network_listen(TCP_Connection* tcp_conn, int count)
{
    if (listen(tcp_conn->socket, count) == -1) {
        Net_Status result = NETWORK_PACKET_NONE;
        switch(errno)
        {
            case EOPNOTSUPP:
            case ENOTSOCK:
            case EBADF: result = NETWORK_UNINITIALIZED;
            case EADDRINUSE: result = NETWORK_ADDR_IN_USE;
            default: {
                fprintf(net_log_stream, "Failed to listen for connection: %s\n", strerror(errno));
                return NETWORK_ERROR;
            } break;
        }
        
        return result;
    }
    return NETWORK_OK;
}

Net_Status
network_accept(TCP_Connection* tcp_conn, TCP_Connection* new_conn)
{
    int len = sizeof(struct sockaddr_in);
    new_conn->socket = accept(tcp_conn->socket, (struct sockaddr*)&new_conn->addr, &len);
    if (new_conn->socket == -1) {
        Net_Status result = NETWORK_PACKET_NONE;
        switch(errno)
        {
            case ENOTSOCK:
            case EOPNOTSUPP:
            case EBADF:        return NETWORK_UNINITIALIZED;
            case EPERM:        return NETWORK_ACCESS_DENIED;
            case ECONNABORTED: return NETWORK_CONN_REFUSED;
            case EINVAL:       return NETWORK_NOT_LISTENING;
            case EWOULDBLOCK:  return NETWORK_PACKET_NONE;
            default: {
                fprintf(net_log_stream, "Failed to accept connection: %s\n", strerror(errno));
                return NETWORK_ERROR;
            };
        }            
    }
    return NETWORK_OK;
}