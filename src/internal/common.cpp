#include "internal/common.h"
#include <cstdarg>
#include <stdint.h>

void FormatString(char* buf, int buf_size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VFormatString(buf, buf_size, fmt, args);
    va_end(args);
}

//From http://www.cse.yorku.ca/~oz/hash.html
//djb2 hash function
int djb2_hash(unsigned char* str) {
    uint32_t hash_val = 5381;
    int c;
    while (c = *str++) {
        hash_val = ((hash_val << 5) + hash_val) + c; /* hash * 33 + c */
    }
    return *((int*)&hash_val);
}

//From http://www.cse.yorku.ca/~oz/hash.html
//djb2 hash function
int djb2_hash_len(unsigned char* str, int len) {
    uint32_t hash_val = 5381;
    for(int i=0; i<len; ++i) {
        hash_val = ((hash_val << 5) + hash_val) + *str++; /* hash * 33 + c */
    }
    return *((int*)&hash_val);
}

float MoveTowards(float val, float target, float amount) {
    float diff = val-target;
    if(diff < 0.0f){
        diff = -diff;
    }
    if(diff < amount) {
        return target;
    } else if(val > target) {
        return val - amount;
    } else {
        return val + amount;
    }
}
