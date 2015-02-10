#include "platform_sdl/file_io.h"
#include "platform_sdl/error.h"
#include "internal/common.h"
#include <SDL.h>
#include <sys/stat.h>
#include <cstring>
#include <errno.h>
#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif
#ifdef WIN32
#include <direct.h>
#endif

static int GetFileSize(const char* path){
	struct stat st;
	if(stat(path, &st) == -1){
		FormattedError("stat failed", "Could not get stats of file: %s\nError: %s", path, strerror(errno));
		exit(1);
	}
	return st.st_size;
}

bool FileLoadData::LoadFile(const char* path, void* memory, int* memory_len, char* err_title, char* err_msg) {
	int file_size;
	{
		struct stat st;
        if(stat(path, &st) == -1){
            FormatString(err_title, FileLoadData::kMaxErrMsgLen, 
                "stat failed");
            FormatString(err_msg, FileLoadData::kMaxErrMsgLen, 
                "Could not get stats of file: %s\nError: %s", path, strerror(errno));
            return false;
		}
		file_size = st.st_size;
	}
    if(file_size > kMaxFileLoadSize){
        FormatString(err_title, FileLoadData::kMaxErrMsgLen, 
            "LoadFile failed");
        FormatString(err_msg, FileLoadData::kMaxErrMsgLen, 
            "File %s is too big\nIt is %d bytes, max is %d", path, file_size, kMaxFileLoadSize);
        return false;
	}
	SDL_RWops *file = SDL_RWFromFile(path, "rb");
    if(!file){
        FormatString(err_title, FileLoadData::kMaxErrMsgLen, 
            "SDL_RWFromFile failed");
        FormatString(err_msg, FileLoadData::kMaxErrMsgLen, 
            "Could not load %s\nError: %s", path, SDL_GetError());
        return false;
	}
	Sint64 length = SDL_RWseek(file, 0, RW_SEEK_END);
	SDL_RWseek(file, 0, RW_SEEK_SET);
	SDL_RWread(file, memory, (size_t)length, 1);
	SDL_RWclose(file);
	*memory_len = (int)length;
    return true;
}

int FileLoadAsync(void* data) {
	FileLoadData* file_load_data = (FileLoadData*)data;
	bool is_running = true;
	while(is_running){
		SDL_Delay(1);
		if (SDL_LockMutex(file_load_data->mutex) == 0) {
            file_load_data->err = false;
			FileRequest* request = file_load_data->queue.PopFrontRequest();
			if(request){
				SDL_Log("File loader thread processing request \"%s\"", request->path);
				file_load_data->err = 
                    !FileLoadData::LoadFile(request->path, 
                                            file_load_data->memory, 
                                            &file_load_data->memory_len, 
                                            file_load_data->err_title, 
                                            file_load_data->err_msg);
				SDL_CondSignal(request->condition);
                if(!file_load_data->err){
				    SDL_Log("File \"%s\" loaded into RAM", request->path);
                }
			}
			is_running = !file_load_data->err && !file_load_data->wants_to_quit;
			SDL_UnlockMutex(file_load_data->mutex);
		} else {
            FormatString(file_load_data->err_title, FileLoadData::kMaxErrMsgLen, 
                "SDL_LockMutex failed");
            FormatString(file_load_data->err_msg, FileLoadData::kMaxErrMsgLen, 
                "Could not lock file loader mutex: %s", SDL_GetError());
			file_load_data->err = true;
            exit(1);
		}
	}
	return 0;
}

FileRequest* FileRequestQueue::AddNewRequest() {
	FileRequest* request = &requests[end];
	end = (end+1)%kMaxFileRequests;
	if(end == start){
		FormattedError("Too many file requests", "More than %d file requests in queue.", kMaxFileRequests);
		exit(1);
	}
	return request;
}

FileRequestQueue::FileRequestQueue():
	start(0), end(0)
{}

FileRequest* FileRequestQueue::PopFrontRequest() {
	if(start == end){
		return NULL;
	}
	FileRequest* request = &requests[start];
	start = (start+1)%kMaxFileRequests;
	return request;
}

bool ChangeWorkingDirectory(const char* path)
{
#ifdef WIN32
    return _chdir(path) == 0;
#else
	return chdir(path) == 0;
#endif
}