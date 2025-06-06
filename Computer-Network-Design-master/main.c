#include "dnsrelay.h"
#include "struct.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int DEBUG_LEVEL = 0; 
char DNS_SERVER[16] = "202.106.0.20";
char LOCAR_NAME[100] = "dnsrelay.txt";

int main(int argc, char* argv[]) {
    // 解析命令行参数
    for(int i = 0; i < argc; i++) {
        // 调试等级
        if(strcmp(argv[i], "-d") == 0 && i+1 < argc) {
            DEBUG_LEVEL = atoi(argv[i+1]);
            i++;
        }
        // DNS服务器
        else if(strcmp(argv[i], "-i") == 0 && i+1 < argc) {
            if (strlen(argv[i + 1]) >= sizeof(DNS_SERVER)) {
                printf("警告：DNS地址过长。\n");
                printf("将使用默认DNS服务器。\n");
                i++;
            }
            else{
                strncpy(DNS_SERVER, argv[i+1], sizeof(DNS_SERVER) - 1);
                DNS_SERVER[sizeof(DNS_SERVER) - 1] = '\0';
                i++;
            }
        }
        // 映射表文件名 
        else if(strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            if (strlen(argv[i + 1]) >= sizeof(LOCAR_NAME)) {
                printf("警告：映射表文件名过长。\n");
                printf("将使用默认本地映射表。\n");
                i++;
            }
            else{
                strncpy(LOCAR_NAME, argv[i+1], sizeof(LOCAR_NAME) - 1);
                LOCAR_NAME[sizeof(LOCAR_NAME) - 1] = '\0';
                i++;
            }
        }
    }

    // 设置编码为UTF-8
    system("chcp 65001");

    if(DEBUG_LEVEL > 0) {
        printf("调试信息级别： %d\n", DEBUG_LEVEL);
    }
    else{
        printf("调试信息级别： 默认\n");
    }
    printf("DNS服务器: %s\n", DNS_SERVER);
    printf("映射表文件: %s\n", LOCAR_NAME);

    // 加载映射表文件
    if (load_dnsrelay_file(LOCAR_NAME) != 0) {
        printf("映射表文件加载失败\n");
        return -1;
    }

    // 初始化DNS服务器
    if (init_dns_server() != 0) {
        printf("初始化DNS中继器失败\n");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    // 主循环
    while (1) {
        // 接收DNS查询
        int len = recvfrom(server_socket, buffer, BUFFER_SIZE, 0,
                          (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (len > 0) {
            // 处理DNS查询
            handle_dns_query(server_socket, &client_addr, buffer, len);
        }
    }

    // 关闭服务器
    close_dns_server();
    return 0;
} 