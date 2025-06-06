#ifndef DNSCACHE_H
#define DNSCACHE_H

#include "struct.h"

void add_to_cache(const char* domain, const char* ip);
void clear_cache();
void save_cache_to_local();

#endif 