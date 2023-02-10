#ifndef NETWORK_LIB
#define NETWORK_LIB
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#if defined (WINSOCK_INCLUDES)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>

typedef struct {
	SOCKET       socket;
	unsigned int port;
	unsigned int flags;
} UDP_Connection;

typedef struct {
	SOCKET socket;
	unsigned int port;
	unsigned int flags;
	struct sockaddr_in addr;
} TCP_Connection;

#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct {
	int          socket;
	unsigned int port;
	unsigned int flags;
} UDP_Connection;

#endif

#define PACKET_SIZE 2048

typedef struct {
	uint8_t ip[16];
} ipv6_t;
typedef uint32_t ipv4_t;

typedef struct {
	uint8_t data[PACKET_SIZE];
	int     length_bytes;
	struct sockaddr_in sender_info;
} UDP_Packet;

typedef struct {
	uint8_t data[PACKET_SIZE];
	int     length_bytes;
} TCP_Packet;

typedef enum {
    NETWORK_CONN_RESET_BY_PEER = -18,
    NETWORK_ACCESS_DENIED = -17,
	NETWORK_ALREADY_LISTENING = -16,
	NETWORK_CONN_REFUSED = -15,
	NETWORK_ALREADY_CONNECTED = -14,
	NETWORK_SOCKET_NOT_BOUND = -13,
	NETWORK_ADDR_IN_USE = -12,
	NETWORK_NOT_LISTENING = -11,
	NETWORK_UNINITIALIZED = -10,
	NETWORK_FORCED_CLOSED = -9,
	NETWORK_UNREACHABLE = -8,
	NETWORK_INVALID_ADDRESS = -7,
	NETWORK_HOST_UNREACHEABLE = -6,
	NETWORK_PORT_UNREACHEABLE = -5,
	NETWORK_FORCED_SHUTDOWN = -4,
	NETWORK_CONN_CLOSED = -3,
	NETWORK_CONN_TIMEOUT = -2,
	NETWORK_PACKET_ERROR = -1,
	NETWORK_ERROR = -1,
	NETWORK_PACKET_NONE = 0,
	NETWORK_OK = 0,
} Net_Status;

#define NETWORK_FLAG_SOCKET_ASYNC (1 << 0)
#define NETWORK_FLAG_UDP_VALID_CONN (1 << 1)

// Initialize/create/destroy
int network_init(FILE* log_stream);
int network_destroy();
int network_create_udp_socket(UDP_Connection* out_conn, int async);
int network_create_udp_bound_socket(UDP_Connection* out_conn, unsigned short port, int async);
int network_close_udp_connection(UDP_Connection* udp_conn);
int network_close_tcp_connection(TCP_Connection* tcp_conn);
int network_create_tcp_socket(TCP_Connection* out_conn, int async);
int network_create_tcp_bound_socket(TCP_Connection* out_conn, unsigned short port, int async);
Net_Status network_listen(TCP_Connection* tcp_conn, int count);
Net_Status network_accept(TCP_Connection* tcp_conn, TCP_Connection* new_conn);
Net_Status network_connect_tcp(TCP_Connection* tcp_conn, struct sockaddr_in* addr);

// Configuration
int network_socket_set_async(UDP_Connection* out_conn);

// Send/Recv
Net_Status network_receive_udp_packets(UDP_Connection* udp_conn, UDP_Packet* out_packet);
Net_Status network_send_udp_packet(UDP_Connection* udp_conn, struct sockaddr_in* to, const char* data, int length);

Net_Status network_receive_tcp_packets(TCP_Connection* tcp_conn, TCP_Packet* out_packet);
Net_Status network_send_tcp_packet(TCP_Connection* tcp_conn, const char* data, int length);

// Utils
int network_dns_ipv4(const char* name_address, ipv4_t* net_ipv4);
int network_dns_ipv6(const char* name_address, ipv6_t* net_ipv6);
int network_dns_info_ipv4(const char* name_address, struct sockaddr_in* info);
int network_dns_info_ipv6(const char* name_address, struct sockaddr_in6* info);
int network_sockaddr_fill(struct sockaddr_in* out_addr, int port, const char* ip);

// Status
int  network_addr_equal(struct sockaddr_in* a1, struct sockaddr_in* a2);

int net_log_error(int error, const char* fmt, ...);

#endif // NETWORK_LIB