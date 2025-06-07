#include "dnsrelay.h"
#include "struct.h"
#include "cache.h"       // ✓ 使用哈希缓存替代旧数组
#include <stdio.h>
#include <string.h>
#include <ws2tcpip.h>
#include <time.h>

// 全局变量
extern int DEBUG_LEVEL;
extern char DEBUG_MODE[];
SOCKET server_socket = INVALID_SOCKET;
static SOCKET dns_socket = INVALID_SOCKET;
DomainMapping domain_mappings[MAX_ENTRIES];
int mapping_count = 0;
static int query_count = 0;

// 打印哈希表状态
void print_cache_status() {
    printf("========== DNS 缓存状态 ==========\n");
    printf("当前缓存总数: %d / %d\n", cache_total, MAX_CACHE_SIZE);
    for (int i = 0; i < HASH_SIZE; ++i) {
        CacheNode* p = hash_table[i];
        if (p == NULL) continue;

        printf("[桶 %4d] -> ", i);
        while (p) {
            printf("(%s -> %s, 剩余 %lds) -> ",
                   p->domain,
                   p->ip,
                   (long)(p->expire_time - time(NULL)));
            p = p->next;
        }
        printf("NULL\n");
    }
    printf("==================================\n");
}
void purge_expired_cache_entries() {
    time_t now = time(NULL);
    for (int i = 0; i < HASH_SIZE; ++i) {
        CacheNode** pp = &hash_table[i];
        while (*pp) {
            CacheNode* node = *pp;
            if (node->expire_time <= now) {
                *pp = node->next;
                free(node);
                cache_total--;
            } else {
                pp = &node->next;
            }
        }
    }
}

// 获取当前时间字符串
void get_current_time(char* time_str) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 解析 DNS 域名
int parse_dns_name(const unsigned char* buffer, int offset, char* name) {
    int i = 0, j = 0, len;
    while ((len = buffer[offset + i]) != 0) {
        if ((len & 0xC0) == 0xC0) {
            int ptr_offset = ((len & 0x3F) << 8) | buffer[offset + i + 1];
            char compressed[MAX_DOMAIN_LENGTH];
            if (parse_dns_name(buffer, ptr_offset, compressed) <= 0)
                return -1;
            strcat(name, compressed);
            return i + 2;
        }
        memcpy(name + j, buffer + offset + i + 1, len);
        j += len;
        name[j++] = '.';
        i += len + 1;
    }
    if (j > 0) name[j - 1] = '\0';
    else name[0] = '\0';
    return i + 1;
}

// 加载 dnsrelay.txt
int load_dnsrelay_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6: %s\n", filename);
        return -1;
    }
    char line[512];
    while (fgets(line, sizeof(line), file) && mapping_count < MAX_ENTRIES) {
        char ip[MAX_IP_LENGTH], domain[MAX_DOMAIN_LENGTH];
        if (sscanf(line, "%s %s", ip, domain) == 2) {
            strncpy(domain_mappings[mapping_count].ip, ip, MAX_IP_LENGTH - 1);
            strncpy(domain_mappings[mapping_count].domain, domain, MAX_DOMAIN_LENGTH - 1);
            domain_mappings[mapping_count].ip[MAX_IP_LENGTH - 1] = '\0';
            domain_mappings[mapping_count].domain[MAX_DOMAIN_LENGTH - 1] = '\0';
            mapping_count++;
        }
    }
    fclose(file);
    printf("\u6210\u529f\u52a0\u8f7d %d \u6761\u57df\u540d\u6620\u5c04\u8bb0\u5f55\n", mapping_count);
    return 0;
}

// 搜索域名 -> IP
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
    // 复制DNS头部，保持ID不变
    memcpy(response, request, sizeof(DNSHeader));
    DNSHeader* dns_header = (DNSHeader*)response;

    // 处理域名不存在的情况
    if (nxdomain || ip_count == 0 || ip_list == NULL) {
        // 设置域名不存在的响应标志
        dns_header->flags = htons(0x8183);  // RCODE=3
        dns_header->ancount = 0;  // 无回答记录
        dns_header->nscount = 0;  // 无授权记录
        dns_header->arcount = 0;  // 无附加记录

        // 复制问题部分
        int offset = sizeof(DNSHeader);
        while (request[offset] != 0) offset++;
        offset += 5;  // 跳过0、QTYPE、QCLASS
        memcpy(response + sizeof(DNSHeader), 
               request + sizeof(DNSHeader), 
               offset - sizeof(DNSHeader));
        *resp_len = offset;
        return;
    }

    // 设置标准响应标志
    dns_header->flags = htons(0x8180);  // 标准响应
    dns_header->ancount = htons(ip_count);
    dns_header->nscount = 0;
    dns_header->arcount = 0;

    // 复制问题部分
    int offset = sizeof(DNSHeader);
    while (request[offset] != 0) offset++;
    offset += 5;
    memcpy(response + sizeof(DNSHeader), 
           request + sizeof(DNSHeader), 
           offset - sizeof(DNSHeader));
    int resp_offset = offset;

    // 添加回答部分
    for (int i = 0; i < ip_count; ++i) {
        // 添加域名指针
        response[resp_offset++] = 0xC0;  // 压缩指针
        response[resp_offset++] = 0x0C;  // 指向问题部分的域名

        // 添加类型和类别
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x01;  // A记录
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x01;  // IN类别

        // 添加TTL
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x00;
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x3C;  // 60秒

        // 添加数据长度
        response[resp_offset++] = 0x00; response[resp_offset++] = 0x04;  // 4字节

        // 添加IP地址
        unsigned char ip_bytes[4];
        sscanf(ip_list[i], "%hhu.%hhu.%hhu.%hhu", 
               &ip_bytes[0], &ip_bytes[1], &ip_bytes[2], &ip_bytes[3]);
        memcpy(response + resp_offset, ip_bytes, 4);
        resp_offset += 4;
    }

    *resp_len = resp_offset;
}

// 解析DNS响应中的CNAME记录
int parse_cname_record(const char* response, int response_len, char* cname) {
    DNSHeader* dns_header = (DNSHeader*)response;
    int answer_count = ntohs(dns_header->ancount);
    
    if (answer_count == 0) return 0;

    int offset = sizeof(DNSHeader);
    // 跳过问题部分
    while (response[offset] != 0) offset++;
    offset += 5; // 跳过0、QTYPE(2)、QCLASS(2)

    // 查找CNAME记录
    for (int i = 0; i < answer_count; i++) {
        // 检查是否是压缩指针
        if ((response[offset] & 0xC0) == 0xC0) {
            offset += 2; // 跳过压缩指针
        } else {
            // 跳过域名
            while (response[offset] != 0) offset++;
            offset++;
        }

        uint16_t type = (response[offset] << 8) | response[offset + 1];
        uint16_t class = (response[offset + 2] << 8) | response[offset + 3];
        uint32_t ttl = (response[offset + 4] << 24) | (response[offset + 5] << 16) |
                      (response[offset + 6] << 8) | response[offset + 7];
        uint16_t rdlength = (response[offset + 8] << 8) | response[offset + 9];
        offset += 10;

        if (type == 5) { // CNAME记录
            // 解析CNAME
            int cname_offset = offset;
            int cname_len = 0;
            while (response[cname_offset] != 0) {
                int len = response[cname_offset];
                if ((len & 0xC0) == 0xC0) {
                    // 处理压缩指针
                    int ptr_offset = ((len & 0x3F) << 8) | response[cname_offset + 1];
                    char compressed_name[MAX_DOMAIN_LENGTH];
                    if (parse_dns_name(response, ptr_offset, compressed_name) <= 0) {
                        return 0;
                    }
                    strcat(cname, compressed_name);
                    return 1;
                }
                memcpy(cname + cname_len, response + cname_offset + 1, len);
                cname_len += len;
                cname[cname_len++] = '.';
                cname_offset += len + 1;
            }
            if (cname_len > 0) {
                cname[cname_len - 1] = '\0';
            }
            return 1;
        }
        offset += rdlength;
    }
    return 0;
}

// 解析DNS响应中的A记录
int parse_a_records(const char* response, int response_len, char ip_list[][MAX_IP_LENGTH], int max_count) {
    DNSHeader* dns_header = (DNSHeader*)response;
    int answer_count = ntohs(dns_header->ancount);
    int ip_count = 0;
    
    if (answer_count == 0) return 0;

    int offset = sizeof(DNSHeader);
    // 跳过问题部分
    while (response[offset] != 0) offset++;
    offset += 5; // 跳过0、QTYPE(2)、QCLASS(2)

    // 查找A记录
    for (int i = 0; i < answer_count && ip_count < max_count; i++) {
        // 检查是否是压缩指针
        if ((response[offset] & 0xC0) == 0xC0) {
            offset += 2; // 跳过压缩指针
        } else {
            // 跳过域名
            while (response[offset] != 0) offset++;
            offset++;
        }

        uint16_t type = (response[offset] << 8) | response[offset + 1];
        uint16_t class = (response[offset + 2] << 8) | response[offset + 3];
        uint32_t ttl = (response[offset + 4] << 24) | (response[offset + 5] << 16) |
                      (response[offset + 6] << 8) | response[offset + 7];
        uint16_t rdlength = (response[offset + 8] << 8) | response[offset + 9];
        offset += 10;

        if (type == 1) { // A记录
            // 解析IP地址
            sprintf(ip_list[ip_count], "%u.%u.%u.%u",
                    (unsigned char)response[offset],
                    (unsigned char)response[offset + 1],
                    (unsigned char)response[offset + 2],
                    (unsigned char)response[offset + 3]);
            ip_count++;
        }
        offset += rdlength;
    }
    return ip_count;
}

// 处理DNS查询
void handle_dns_query(SOCKET server_socket, struct sockaddr_in* client_addr, char* buffer, int len) {
    char domain[MAX_DOMAIN_LENGTH];
    int offset = sizeof(DNSHeader);
    int name_len = parse_dns_name(buffer, offset, domain);
    if (name_len <= 0) {
        if (DEBUG_LEVEL >= 2) printf("[DEBUG] 解析域名失败\n");
        return;
    }

    query_count++;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    char time_str[20];
    get_current_time(time_str);

    if (DEBUG_LEVEL >= 1) {
        printf("[%s] #%d %s 查询: %s\n", time_str, query_count, client_ip, domain);
    }

    int qtype_pos = offset + name_len;
    uint16_t qtype  = (buffer[qtype_pos] << 8) | buffer[qtype_pos + 1];
    uint16_t qclass = (buffer[qtype_pos + 2] << 8) | buffer[qtype_pos + 3];

    if (DEBUG_LEVEL >= 2) {
        printf("[DEBUG] QTYPE = 0x%04X, QCLASS = 0x%04X\n", qtype, qclass);
        // 输出DNS报文字段
        DNSHeader* dns_header = (DNSHeader*)buffer;
        printf("[DEBUG] DNS报文: ID=0x%04X, QR=%d, OPCODE=%d, AA=%d, TC=%d, RD=%d, RA=%d, Z=%d, RCODE=%d\n",
            ntohs(dns_header->id),
            (ntohs(dns_header->flags) >> 15) & 0x1,    // QR
            (ntohs(dns_header->flags) >> 11) & 0xF,    // OPCODE
            (ntohs(dns_header->flags) >> 10) & 0x1,    // AA
            (ntohs(dns_header->flags) >> 9) & 0x1,     // TC
            (ntohs(dns_header->flags) >> 8) & 0x1,     // RD
            (ntohs(dns_header->flags) >> 7) & 0x1,     // RA
            (ntohs(dns_header->flags) >> 4) & 0x7,     // Z
            ntohs(dns_header->flags) & 0xF             // RCODE
        );
    }

    char response[BUFFER_SIZE];
    int resp_len = 0;

    if (qtype != 1 || qclass != 1) {
        if (DEBUG_LEVEL >= 2) printf("[DEBUG] 非A记录查询，返回空响应\n");
        build_dns_response(buffer, len, response, &resp_len, NULL, 0, 0);
        sendto(server_socket, response, resp_len, 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
        return;
    }

    char ip_list[10][MAX_IP_LENGTH];
    int ip_count = 0;

    // 先查找本地文件
    ip_count = find_domain_mappings(domain, ip_list, 10);
    
    // 如果本地文件没有，再查找缓存
    if (ip_count == 0) {
        ip_count = find_in_cache(domain, ip_list, 10);
    }

    int nxdomain = 1;
    for (int i = 0; i < ip_count; ++i) {
        if (strcmp(ip_list[i], "0.0.0.0") != 0) {
            nxdomain = 0;
            break;
        }
    }

    if (ip_count > 0) {
        if (DEBUG_LEVEL >= 2) {
            // 检查是否来自本地文件
            char temp_ip_list[10][MAX_IP_LENGTH];
            int file_count = find_domain_mappings(domain, temp_ip_list, 10);
            if (file_count > 0) {
                printf("[DEBUG] 本地文件命中: %s ->", domain);
            } else {
                printf("[DEBUG] 本地缓存命中: %s ->", domain);
            }
            for (int i = 0; i < ip_count; ++i) printf(" %s", ip_list[i]);
            printf("\n");
        }

        build_dns_response(buffer, len, response, &resp_len, ip_list, ip_count, nxdomain);
        sendto(server_socket, response, resp_len, 0, (struct sockaddr*)client_addr, sizeof(*client_addr));

        if (!nxdomain) {
            for (int i = 0; i < ip_count; ++i) {
                add_to_cache(domain, ip_list[i]);
                purge_expired_cache_entries();
                print_cache_status();
            }
        }
    } else {
        if (DEBUG_LEVEL >= 2) printf("[DEBUG] 转发到上游: %s\n", domain);
        int response_len = forward_to_dns_server(buffer, len, response);
        if (response_len > 0) {
            // 检查是否有CNAME记录
            char cname[MAX_DOMAIN_LENGTH] = {0};
            if (parse_cname_record(response, response_len, cname)) {
                if (DEBUG_LEVEL >= 2) {
                    printf("[DEBUG] CNAME: %s -> %s\n", domain, cname);
                }
            }

            // 解析所有A记录
            char ip_list[10][MAX_IP_LENGTH];
            int ip_count = parse_a_records(response, response_len, ip_list, 10);
            
            if (DEBUG_LEVEL >= 2) {
                printf("[DEBUG] A记录: %s ->", cname[0] ? cname : domain);
                for (int i = 0; i < ip_count; i++) {
                    printf(" %s", ip_list[i]);
                }
                printf("\n");
            }

            // 缓存所有IP地址
            for (int i = 0; i < ip_count; i++) {
                add_to_cache(domain, ip_list[i]);
                purge_expired_cache_entries();
                print_cache_status();
            }

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
