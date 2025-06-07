#include "struct.h"
#include <string.h>

#define MAX_MAPPING 1024

static char domain_list[MAX_MAPPING][MAX_DOMAIN_LENGTH];
static char ip_list[MAX_MAPPING][MAX_IP_LENGTH];
static int mapping_count = 0;

int insert_to_mapping(const char* domain, const char* ip) {
    if (mapping_count >= MAX_MAPPING) return -1;
    strncpy(domain_list[mapping_count], domain, MAX_DOMAIN_LENGTH);
    strncpy(ip_list[mapping_count], ip, MAX_IP_LENGTH);
    mapping_count++;
    return 0;
}

int find_domainmappings(const char* domain, char out[][MAX_IP_LENGTH], int max_count) {
    int found = 0;
    for (int i = 0; i < mapping_count && found < max_count; ++i) {
        if (strcmp(domain_list[i], domain) == 0) {
            strncpy(out[found], ip_list[i], MAX_IP_LENGTH);
            found++;
        }
    }
    return found;
}
