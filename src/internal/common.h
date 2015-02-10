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

inline void swap(void* &a, void* &b){
	void* temp = a;
	a = b;
	b = temp;
}

inline void VFormatString(char* buf, int buf_size, const char* fmt, va_list args) {
	vsnprintf(buf, buf_size, fmt, args);
}

void FormatString(char* buf, int buf_size, const char* fmt, ...);

#endif