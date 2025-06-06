#include "dnsrelay.h"
#include "struct.h"
#include "dnscache.h"
#include <stdio.h>
#include <string.h>
#include <ws2tcpip.h>  // 必须保留
           


extern int DEBUG_LEVEL;
extern char DEBUG_MODE[];

SOCKET server_socket = INVALID_SOCKET;
static SOCKET dns_socket = INVALID_SOCKET;

DomainMapping domain_mappings[MAX_ENTRIES];
int mapping_count = 0;

// 解析DNS报文中的域名
int parse_dns_name(const unsigned char* buffer, int offset, char* name) {
    int i = 0;
    int j = 0;
    int len;
    
    while ((len = buffer[offset + i]) != 0) {
        // 检查是否是压缩指针
        if ((len & 0xC0) == 0xC0) {
            // 获取压缩指针的偏移量
            int ptr_offset = ((len & 0x3F) << 8) | buffer[offset + i + 1];
            // 递归解析压缩的域名
            char compressed_name[MAX_DOMAIN_LENGTH];
            if (parse_dns_name(buffer, ptr_offset, compressed_name) <= 0) {
                return -1;
            }
            // 将压缩的域名添加到结果中
            strcat(name, compressed_name);
            return i + 2;  // 返回压缩指针的长度
        }
        
        // 复制标签
        memcpy(name + j, buffer + offset + i + 1, len);
        j += len;
        name[j++] = '.';
        i += len + 1;
    }
    
    // 移除最后一个点
    if (j > 0) {
        name[j - 1] = '\0';
    } else {
        name[0] = '\0';
    }
    
    return i + 1;  // 返回解析的字节数
}

// 加载dnsrelay.txt文件
int load_dnsrelay_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("无法打开文件: %s\n", filename);
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file) && mapping_count < MAX_ENTRIES) {
        char ip[MAX_IP_LENGTH];
        char domain[MAX_DOMAIN_LENGTH];
        
        // 解析每行的IP和域名
        if (sscanf(line, "%s %s", ip, domain) == 2) {
            strncpy(domain_mappings[mapping_count].ip, ip, MAX_IP_LENGTH - 1);
            strncpy(domain_mappings[mapping_count].domain, domain, MAX_DOMAIN_LENGTH - 1);
            domain_mappings[mapping_count].ip[MAX_IP_LENGTH - 1] = '\0';
            domain_mappings[mapping_count].domain[MAX_DOMAIN_LENGTH - 1] = '\0';
            mapping_count++;
        }
    }

    fclose(file);
    printf("成功加载 %d 条域名映射记录\n", mapping_count);
    return 0;
}

// 查找域名对应的IP
int find_domain_mappings(const char* domain, char ip_list[][MAX_IP_LENGTH], int max_count) {
    int count = 0;
    for (int i = 0; i < mapping_count && count < max_count; ++i) {
        if (strcmp(domain_mappings[i].domain, domain) == 0) {
            strncpy(ip_list[count], domain_mappings[i].ip, MAX_IP_LENGTH - 1);
            ip_list[count][MAX_IP_LENGTH - 1] = '\0';
            ++count;
        }
    }
    return count;
}


// 初始化DNS服务器
int init_dns_server() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return -1;
    }

    // 创建本地服务器socket
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket == INVALID_SOCKET) {
        printf("Create socket failed\n");
        return -1;
    }

    // 绑定本地地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DEFAULT_LOCAL_PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("绑定端口失败\n");
        closesocket(server_socket);
        return -1;
    }

    // 创建DNS服务器socket
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_socket == INVALID_SOCKET) {
        printf("创建socket失败\n");
        closesocket(server_socket);
        return -1;
    }

    printf("DNS中继服务器初始化成功！\n");
    printf("本地监听端口: %d\n", DEFAULT_LOCAL_PORT);
    printf("上游DNS服务器: %s:%d\n", DEFAULT_DNS_SERVER, DEFAULT_DNS_PORT);

    return 0;
}

// 构造标准DNS响应包
void build_dns_response(const char* request, int req_len, char* response, int* resp_len,
                        char ip_list[][MAX_IP_LENGTH], int ip_count, int nxdomain) {
    memcpy(response, request, sizeof(DNSHeader));
    DNSHeader* dns_header = (DNSHeader*)response;

    if (nxdomain || ip_count == 0 || ip_list == NULL) {
        dns_header->flags = htons(0x8183); // RCODE=3 or empty answer
        dns_header->ancount = 0;
        dns_header->nscount = 0;
        dns_header->arcount = 0;

        // 拷贝问题部分
        int offset = sizeof(DNSHeader);
        while (request[offset] != 0) offset++;
        offset += 5; // 跳过0、QTYPE(2)、QCLASS(2)
        memcpy(response + sizeof(DNSHeader), request + sizeof(DNSHeader), offset - sizeof(DNSHeader));
        *resp_len = offset;
        return;
    }

    dns_header->flags = htons(0x8180);           // 标准响应
    dns_header->ancount = htons(ip_count);
    dns_header->nscount = 0;
    dns_header->arcount = 0;

    int offset = sizeof(DNSHeader);
    while (request[offset] != 0) offset++;
    offset += 5;
    memcpy(response + sizeof(DNSHeader), request + sizeof(DNSHeader), offset - sizeof(DNSHeader));
    int resp_offset = offset;

    for (int i = 0; i < ip_count; ++i) {
        response[resp_offset++] = 0xC0;  // 指针
        response[resp_offset++] = 0x0C;
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x01;
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x01;
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x00;
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x3C;
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x04;

        unsigned char ip_bytes[4];
        sscanf(ip_list[i], "%hhu.%hhu.%hhu.%hhu", &ip_bytes[0], &ip_bytes[1], &ip_bytes[2], &ip_bytes[3]);
        memcpy(response + resp_offset, ip_bytes, 4);
        resp_offset += 4;
    }

    *resp_len = resp_offset;
}



// 处理DNS查询
void handle_dns_query(SOCKET server_socket, struct sockaddr_in* client_addr, char* buffer, int len) {
    char domain[MAX_DOMAIN_LENGTH];
    int offset = sizeof(DNSHeader);
    int name_len = parse_dns_name(buffer, offset, domain);
    if (name_len <= 0) {
        if (DEBUG_LEVEL) printf("[DEBUG] 解析域名失败\n");
        return;
    }

    int qtype_pos = offset + name_len;
    uint16_t qtype  = (buffer[qtype_pos] << 8) | buffer[qtype_pos + 1];
    uint16_t qclass = (buffer[qtype_pos + 2] << 8) | buffer[qtype_pos + 3];

    if (DEBUG_LEVEL) printf("[DEBUG] 收到 %s, QTYPE = 0x%04X\n", domain, qtype);

    char response[BUFFER_SIZE];
    int resp_len = 0;

    if (qtype != 1 || qclass != 1) {
        build_dns_response(buffer, len, response, &resp_len, NULL, 0, 0);
        sendto(server_socket, response, resp_len, 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
        return;
    }

    char ip_list[10][MAX_IP_LENGTH];
    int ip_count = find_domain_mappings(domain, ip_list, 10);

    int nxdomain = 1;
    for (int i = 0; i < ip_count; ++i) {
        if (strcmp(ip_list[i], "0.0.0.0") != 0) {
            nxdomain = 0;
            break;
        }
    }

    if (ip_count > 0) {
        if (DEBUG_LEVEL) {
            printf("[DEBUG] 本地命中: %s ->", domain);
            for (int i = 0; i < ip_count; ++i) printf(" %s", ip_list[i]);
            printf("\n");
        }

        build_dns_response(buffer, len, response, &resp_len, ip_list, ip_count, nxdomain);
        sendto(server_socket, response, resp_len, 0, (struct sockaddr*)client_addr, sizeof(*client_addr));

        if (!nxdomain) {
            for (int i = 0; i < ip_count; ++i) {
                add_to_cache(domain, ip_list[i]);
            }
        }
    } else {
        if (DEBUG_LEVEL) printf("[DEBUG] 转发到上游: %s\n", domain);
        int response_len = forward_to_dns_server(buffer, len, response);
        if (response_len > 0) {
            // 提取最后 4 字节作为 A 记录（简化逻辑）
            int ans_offset = response_len - 4;
            char answer_ip[MAX_IP_LENGTH];
            sprintf(answer_ip, "%u.%u.%u.%u",
                    (unsigned char)response[ans_offset],
                    (unsigned char)response[ans_offset + 1],
                    (unsigned char)response[ans_offset + 2],
                    (unsigned char)response[ans_offset + 3]);
            add_to_cache(domain, answer_ip);
            sendto(server_socket, response, response_len, 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
        }
    }
}



// 转发到DNS服务器
int forward_to_dns_server(const char* buffer, int len, char* response) {
    struct sockaddr_in dns_addr;
    memset(&dns_addr, 0, sizeof(dns_addr));
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(DEFAULT_DNS_PORT);
    inet_pton(AF_INET, DEFAULT_DNS_SERVER, &dns_addr.sin_addr);

    // 发送查询到DNS服务器
    if (sendto(dns_socket, buffer, len, 0, 
               (struct sockaddr*)&dns_addr, sizeof(dns_addr)) == SOCKET_ERROR) {
        printf("向DNS服务器发送查询失败\n");
        return -1;
    }

    // 接收DNS服务器响应
    int response_len = recvfrom(dns_socket, response, BUFFER_SIZE, 0, NULL, NULL);
    if (response_len == SOCKET_ERROR) {
        printf("从DNS服务器接收响应失败\n");
        return -1;
    }

    return response_len;
}

// 关闭DNS服务器
void close_dns_server() {
    if (server_socket != INVALID_SOCKET) {
        closesocket(server_socket);
    }
    if (dns_socket != INVALID_SOCKET) {
        closesocket(dns_socket);
    }
    WSACleanup();
}
