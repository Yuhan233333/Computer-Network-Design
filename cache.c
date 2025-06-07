#include "cache.h"
#include <string.h>
#include <stdlib.h>

CacheNode* hash_table[HASH_SIZE] = {0};
int cache_total = 0;

/* 经典 djb2，效果与性能都够用 */
unsigned hash_domain(const char* s)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *s++))
        hash = ((hash << 5) + hash) + (unsigned char)c;
    return (unsigned)hash;
}

/* 查询：把桶里所有未过期的匹配节点收集出来 */
int find_in_cache(const char* domain,
                  char ip_list[][MAX_IP_LENGTH],
                  int max_count)
{
    int found = 0;
    time_t now = time(NULL);
    unsigned idx = hash_domain(domain) & (HASH_SIZE - 1);

    CacheNode* p = hash_table[idx];
    CacheNode* prev = NULL;
    while (p && found < max_count) {
        if (p->expire_time <= now) {          /* 已过期，顺手删除 */
            CacheNode* tmp = p;
            if (prev) prev->next = p->next;
            else      hash_table[idx] = p->next;
            p = p->next;
            free(tmp);
            cache_total--;
            continue;
        }
        if (strcmp(p->domain, domain) == 0) {
            strncpy(ip_list[found], p->ip, MAX_IP_LENGTH - 1);
            ip_list[found][MAX_IP_LENGTH - 1] = '\0';
            found++;
        }
        prev = p;
        p = p->next;
    }
    return found;
}

/* 插入 / 刷新 TTL */
void add_to_cache(const char* domain, const char* ip)
{
    time_t now = time(NULL);
    unsigned idx = hash_domain(domain) & (HASH_SIZE - 1);

    /* 先看桶里有没有同名同 IP，若有更新 TTL */
    for (CacheNode* p = hash_table[idx]; p; p = p->next) {
        if (strcmp(p->domain, domain) == 0 && strcmp(p->ip, ip) == 0) {
            p->expire_time = now + 60;    /* refresh */
            return;
        }
    }

    /* 如已满 → 先做一次简单清扫，仍满则直接拒绝（也可换成替换最旧策略） */
    if (cache_total >= MAX_CACHE_SIZE) {
        for (int i = 0; i < HASH_SIZE && cache_total >= MAX_CACHE_SIZE; ++i) {
            CacheNode* cur = hash_table[i];
            CacheNode* prev = NULL;
            while (cur) {
                if (cur->expire_time <= now) {
                    CacheNode* tmp = cur;
                    if (prev) prev->next = cur->next;
                    else      hash_table[i] = cur->next;
                    cur = cur->next;
                    free(tmp);
                    cache_total--;
                } else {
                    prev = cur;
                    cur = cur->next;
                }
            }
        }
        if (cache_total >= MAX_CACHE_SIZE) return; /* 仍满就放弃插入 */
    }

    /* 插入到桶头（O(1)） */
    CacheNode* node = (CacheNode*)malloc(sizeof(CacheNode));
    strncpy(node->domain, domain, MAX_DOMAIN_LENGTH - 1);
    node->domain[MAX_DOMAIN_LENGTH - 1] = '\0';
    strncpy(node->ip, ip, MAX_IP_LENGTH - 1);
    node->ip[MAX_IP_LENGTH - 1] = '\0';
    node->expire_time = now + 60;
    node->next = hash_table[idx];
    hash_table[idx] = node;
    cache_total++;
}
