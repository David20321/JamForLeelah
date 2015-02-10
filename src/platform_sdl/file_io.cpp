#include "platform_sdl/file_io.h"
#include "platform_sdl/error.h"
#include <SDL.h>
#include <sys/stat.h>
#include <cstring>

static int GetFileSize(const char* path){
	struct stat st;
	if(stat(path, &st) == -1){
		FormattedError("stat failed", "Could not get stats of file: %s\nError: %s", path, strerror(errno));
		exit(1);
	}
	return st.st_size;
}

void FileLoadData::LoadFile(const char* path, void* memory, int* memory_len) {
	int file_size;
	{
		struct stat st;
		if(stat(path, &st) == -1){
			FormattedError("stat failed", "Could not get stats of file: %s\nError: %s", path, strerror(errno));
			exit(1);
		}
		file_size = st.st_size;
	}
	if(file_size > kMaxFileLoadSize){
		FormattedError("LoadFile failed", "File %s is too big\nIt is %d bytes, max is %d", path, file_size, kMaxFileLoadSize);
		exit(1);
	}
	SDL_RWops *file = SDL_RWFromFile(path, "rb");
	if(!file){
		FormattedError("SDL_RWFromFile failed", "Could not load %s\nError: %s", path, SDL_GetError());
		exit(1);
	}
	Sint64 length = SDL_RWseek(file, 0, RW_SEEK_END);
	SDL_RWseek(file, 0, RW_SEEK_SET);
	SDL_RWread(file, memory, (size_t)length, 1);
	SDL_RWclose(file);
	*memory_len = (int)length;
}

int FileLoadAsync(void* data) {
	FileLoadData* file_load_data = (FileLoadData*)data;
	bool is_running = true;
	while(is_running){
		SDL_Delay(1);
		if (SDL_LockMutex(file_load_data->mutex) == 0) {
			FileRequest* request = file_load_data->queue.PopFrontRequest();
			if(request){
				SDL_Log("File loader thread processing request \"%s\"", request->path);
				FileLoadData::LoadFile(request->path, file_load_data->memory, &file_load_data->memory_len);
				SDL_CondSignal(request->condition);
				SDL_Log("File \"%s\" loaded into RAM", request->path);
			}
			is_running = !file_load_data->wants_to_quit;
			SDL_UnlockMutex(file_load_data->mutex);
		} else {
			FormattedError("SDL_LockMutex failed", "Could not lock file loader mutex: %s", SDL_GetError());
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