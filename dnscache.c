#include "dnscache.h"
#include <stdio.h>
#include <string.h>

#define MAX_IP_LENGTH 16
#define MAX_DOMAIN_LENGTH 256

// 写入缓存文件
void add_to_cache(const char* domain, const char* ip) {
    FILE* cache = fopen("dnscache.txt", "a+");
    if (!cache) return;
    char line[512], exist = 0;
    rewind(cache);
    while (fgets(line, sizeof(line), cache)) {
        char cip[MAX_IP_LENGTH], cdomain[MAX_DOMAIN_LENGTH];
        if (sscanf(line, "%s %s", cip, cdomain) == 2 && strcmp(cdomain, domain) == 0) {
            exist = 1; break;
        }
    }
    if (!exist) fprintf(cache, "%s %s\n", ip, domain);
    fclose(cache);
}

// 清空缓存文件
void clear_cache() {
    FILE* cache = fopen("dnscache.txt", "w");
    if (cache) fclose(cache);
}

// 将缓存内容追加到本地映射表文件
void save_cache_to_local() {
    FILE* cache = fopen("dnscache.txt", "r");
    FILE* local = fopen("dnsrelay.txt", "a");
    if (!cache || !local) { if (cache) fclose(cache); if (local) fclose(local); return; }
    char line[512];
    while (fgets(line, sizeof(line), cache)) fputs(line, local);
    fclose(cache); fclose(local);
    clear_cache();
    printf("缓存已保存到本地映射表并清空。\n");
} 