#ifndef DNSRELAY_H
#define DNSRELAY_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

// 默认配置
#define DEFAULT_DNS_PORT 53
#define DEFAULT_LOCAL_PORT 53
#define DEFAULT_DNS_SERVER "202.106.0.20"  
#define BUFFER_SIZE 512

#define MAX_DOMAIN_LENGTH 256
#define MAX_IP_LENGTH 64
#define MAX_ENTRIES 10000

// 外部变量声明
extern SOCKET server_socket;

// 域名映射结构体
typedef struct {
    char domain[MAX_DOMAIN_LENGTH];
    char ip[MAX_IP_LENGTH];
} DomainMapping;

// 全局变量声明
extern DomainMapping domain_mappings[MAX_ENTRIES];
extern int mapping_count;

// 函数声明
int init_dns_server();
void handle_dns_query(SOCKET sock, struct sockaddr_in* client_addr, char* buffer, int len);
int forward_to_dns_server(const char* buffer, int len, char* response);
void close_dns_server();
int load_dnsrelay_file(const char* filename);
int find_domain_mapping(const char* domain, char* ip);
void build_dns_response(const char* request, int req_len, char* response, int* resp_len,
                        char ip_list[][MAX_IP_LENGTH], int ip_count, int nxdomain);

#endif 