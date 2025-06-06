#ifndef DNS_STRUCT_H
#define DNS_STRUCT_H

#include <stdint.h>

// DNS报文头部结构
typedef struct {
    uint16_t id;        // 标识符
    uint16_t flags;     // 各种标志位
    uint16_t qdcount;   // 问题数
    uint16_t ancount;   // 回答数
    uint16_t nscount;   // 授权记录数
    uint16_t arcount;   // 附加记录数
} DNSHeader;

// DNS查询类型
#define DNS_TYPE_A     1    // IPv4地址
#define DNS_CLASS_IN   1    // Internet

// DNS响应码
#define DNS_RCODE_NOERROR  0    // 无错误
#define DNS_RCODE_NXDOMAIN 3    // 域名不存在

// DNS报文标志位
#define DNS_QR_QUERY    0    // 查询
#define DNS_QR_RESPONSE 1    // 响应
#define DNS_OPCODE_QUERY 0   // 标准查询

// 函数声明
void parse_dns_header(const unsigned char* buffer, DNSHeader* header);
void build_dns_header(unsigned char* buffer, DNSHeader* header);
int parse_dns_name(const unsigned char* buffer, int offset, char* name);
int build_dns_name(unsigned char* buffer, int offset, const char* name);

#endif 