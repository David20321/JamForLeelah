#include "platform_sdl/error.h"
#include "internal/common.h"
#include <cstdarg>
#include <SDL.h>

void FormattedError(const char* title, const char* msg_fmt, ...) {
	static const int kBufSize = 256;
	char error_msg[kBufSize];
	va_list args;
	va_start(args, msg_fmt);
	VFormatString(error_msg, kBufSize, msg_fmt, args);
	va_end(args);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, error_msg, NULL);
}