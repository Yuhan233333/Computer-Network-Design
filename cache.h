/* ==== cache.h ====  */
#ifndef CACHE_H
#define CACHE_H
#include <time.h>
#include "dnsrelay.h"
#define HASH_SIZE 2048          /* 选择 2 的幂，模运算可优化为位与 */
#define MAX_CACHE_SIZE 1000     /* 总容量上限 */

typedef struct CacheNode {
    char domain[MAX_DOMAIN_LENGTH];
    char ip[MAX_IP_LENGTH];
    time_t expire_time;
    struct CacheNode* next;
} CacheNode;

extern CacheNode* hash_table[HASH_SIZE];
extern int cache_total;         /* 当前节点总数 */

unsigned     hash_domain(const char* s);          /* djb2 */
int          find_in_cache(const char* domain,
                            char ip_list[][MAX_IP_LENGTH],
                            int max_count);
void      add_to_cache(const char* domain, const char* ip);

#endif
