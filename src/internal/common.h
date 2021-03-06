#pragma once
#ifndef INTERNAL_COMMON_H
#define INTERNAL_COMMON_H

#include <cstdio>

inline int max(int a, int b){
    return a>b?a:b;
}

inline int min(int a, int b){
    return a<b?a:b;
}

inline double max(double a, double b){
    return a>b?a:b;
}

inline double min(double a, double b){
    return a<b?a:b;
}

inline float max(float a, float b){
    return a>b?a:b;
}

inline float min(float a, float b){
    return a<b?a:b;
}

inline void swap(void* &a, void* &b){
    void* temp = a;
    a = b;
    b = temp;
}

float MoveTowards(float val, float target, float amount);

inline void VFormatString(char* buf, int buf_size, const char* fmt, va_list args) {
    int val = vsnprintf(buf, buf_size, fmt, args);
    if(val == -1 || val >= buf_size){
        buf[buf_size-1] = '\0';
    }
}

void FormatString(char* buf, int buf_size, const char* fmt, ...);

int djb2_hash(unsigned char* str);
int djb2_hash_len(unsigned char* str, int len);

#endif