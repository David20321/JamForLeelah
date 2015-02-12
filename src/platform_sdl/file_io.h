#pragma once
#ifndef PLATFORM_SDL_FILE_IO_HPP
#define PLATFORM_SDL_FILE_IO_HPP

#include <SDL.h>

struct FileRequest {
	static const int kMaxFileRequestPathLen = 512;
	char path[kMaxFileRequestPathLen];
	SDL_cond* condition; 
};

struct FileRequestQueue {
	static const int kMaxFileRequests = 100;
	FileRequest requests[kMaxFileRequests];
	int start, end;
	FileRequest* AddNewRequest();
	FileRequest* PopFrontRequest();
	FileRequestQueue();
};

class FileLoadThreadData {
public:
	// Err info
    static const int kMaxErrMsgLen = 1024;
    char err_title[kMaxErrMsgLen];
    char err_msg[kMaxErrMsgLen];
    bool err;
	// Thread info
	bool wants_to_quit;
	SDL_mutex *mutex;
	static const int kMaxFileLoadSize = 32*1024*1024;
	void *memory;
	int memory_len;
	// File memory
	FileRequestQueue queue;
    static bool LoadFile(const char* path, void* memory, int* memory_len, char* err_title, char* err_msg);
	int Run();
};

int FileLoadAsync(void* data);

bool ChangeWorkingDirectory(const char* path);
static int GetFileSize(const char* path);

#endif