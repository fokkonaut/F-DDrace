/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "system.h"
#include "lock_scope.h"

#include <sys/types.h>
#include <sys/stat.h>

#if defined(CONF_FAMILY_UNIX)
	#include <sys/time.h>
	#include <unistd.h>

	/* unix net includes */
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <errno.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <fcntl.h>
	#include <pthread.h>
	#include <arpa/inet.h>

	#include <dirent.h>

	#if defined(CONF_PLATFORM_MACOSX)
		#include <Carbon/Carbon.h>
	#endif

#elif defined(CONF_FAMILY_WINDOWS)
	#define WIN32_LEAN_AND_MEAN
	#undef _WIN32_WINNT
	//#define _WIN32_WINNT 0x0501 /* required for mingw to get getaddrinfo to work */
	#define _WIN32_WINNT 0x0600 // required for WC_ERR_INVALID_CHARS etc.
	#include <windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <fcntl.h>
	#include <direct.h>
	#include <errno.h>
	#include <process.h>
	#include <wincrypt.h>
#else
	#error NOT IMPLEMENTED
#endif

#if defined(CONF_ARCH_IA32) || defined(CONF_ARCH_AMD64)
	#include <immintrin.h> //_mm_pause
#endif

#if defined(CONF_PLATFORM_SOLARIS)
	#include <sys/filio.h>
#endif

IOHANDLE io_stdin() { return (IOHANDLE)stdin; }
IOHANDLE io_stdout() { return (IOHANDLE)stdout; }
IOHANDLE io_stderr() { return (IOHANDLE)stderr; }

static DBG_LOGGER loggers[16];
static int num_loggers = 0;

static NETSTATS network_stats = {0};

#define VLEN 128
#define PACKETSIZE 1400
typedef struct
{
#ifdef CONF_PLATFORM_LINUX
	int pos;
	int size;
	struct mmsghdr msgs[VLEN];
	struct iovec iovecs[VLEN];
	char bufs[VLEN][PACKETSIZE];
	char sockaddrs[VLEN][128];
#else
	char buf[PACKETSIZE];
#endif
} NETSOCKET_BUFFER;

void net_buffer_init(NETSOCKET_BUFFER *buffer);
void net_buffer_reinit(NETSOCKET_BUFFER *buffer);
void net_buffer_simple(NETSOCKET_BUFFER *buffer, char **buf, int *size);

struct NETSOCKET_INTERNAL
{
	int type;
	int ipv4sock;
	int ipv6sock;
	int web_ipv4sock;

	NETSOCKET_BUFFER buffer;
};
static NETSOCKET_INTERNAL invalid_socket = {NETTYPE_INVALID, -1, -1, -1};

void dbg_logger(DBG_LOGGER logger)
{
	loggers[num_loggers++] = logger;
}

void dbg_assert_imp(const char *filename, int line, int test, const char *msg)
{
	if(!test)
	{
		dbg_msg("assert", "%s(%d): %s", filename, line, msg);
		dbg_break();
	}
}

void dbg_break()
{
	*((volatile unsigned*)0) = 0x0;
}

void dbg_msg(const char *sys, const char *fmt, ...)
{
	va_list args;
	char str[1024*4];
	char *msg;
	int i, len;

	char timestr[80];
	str_timestamp_format(timestr, sizeof(timestr), FORMAT_SPACE);

	str_format(str, sizeof(str), "[%s][%s]: ", timestr, sys);

	len = strlen(str);
	msg = (char *)str + len;

	va_start(args, fmt);
#if defined(CONF_FAMILY_WINDOWS) && !defined(__GNUC__)
	_vsprintf_p(msg, sizeof(str)-len, fmt, args);
#else
	vsnprintf(msg, sizeof(str)-len, fmt, args);
#endif
	va_end(args);

	for(i = 0; i < num_loggers; i++)
		loggers[i](str);
}

#if defined(CONF_FAMILY_WINDOWS)
static void logger_win_console(const char *line)
{
	#define _MAX_LENGTH 1024
	#define _MAX_LENGTH_ERROR (_MAX_LENGTH+32)

	static const int UNICODE_REPLACEMENT_CHAR = 0xfffd;

	static const char *STR_TOO_LONG = "(str too long)";
	static const char *INVALID_UTF8 = "(invalid utf8)";

	wchar_t wline[_MAX_LENGTH_ERROR];
	size_t len = 0;

	const char *read = line;
	const char *error = STR_TOO_LONG;
	while(len < _MAX_LENGTH)
	{
		// Read a character. This also advances the read pointer
		int glyph = str_utf8_decode(&read);
		if(glyph < 0)
		{
			// If there was an error decoding the UTF-8 sequence,
			// emit a replacement character. Since the
			// str_utf8_decode function will not work after such
			// an error, end the string here.
			glyph = UNICODE_REPLACEMENT_CHAR;
			error = INVALID_UTF8;
			wline[len] = glyph;
			break;
		}
		else if(glyph == 0)
		{
			// A character code of 0 signals the end of the string.
			error = 0;
			break;
		}
		else if(glyph > 0xffff)
		{
			// Since the windows console does not really support
			// UTF-16, don't mind doing actual UTF-16 encoding,
			// but rather emit a replacement character.
			glyph = UNICODE_REPLACEMENT_CHAR;
		}
		else if(glyph == 0x2022)
		{
			// The 'bullet' character might get converted to a 'beep',
			// so it will be replaced by the 'bullet operator'.
			glyph = 0x2219;
		}

		// Again, since the windows console does not really support
		// UTF-16, but rather something along the lines of UCS-2,
		// simply put the character into the output.
		wline[len++] = glyph;
	}

	if(error)
	{
		read = error;
		while(1)
		{
			// Errors are simple ascii, no need for UTF-8
			// decoding
			char character = *read;
			if(character == 0)
				break;

			dbg_assert(len < _MAX_LENGTH_ERROR, "str too short for error");
			wline[len++] = character;
			read++;
		}
	}

	// Terminate the line
	dbg_assert(len < _MAX_LENGTH_ERROR, "str too short for \\r");
	wline[len++] = '\r';
	dbg_assert(len < _MAX_LENGTH_ERROR, "str too short for \\n");
	wline[len++] = '\n';

	// Ignore any error that might occur
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), wline, len, 0, 0);

	#undef _MAX_LENGTH
	#undef _MAX_LENGTH_ERROR
}
#endif

static void logger_stdout(const char *line)
{
	printf("%s\n", line);
	fflush(stdout);
}

static void logger_debugger(const char *line)
{
#if defined(CONF_FAMILY_WINDOWS)
	OutputDebugString(line);
	OutputDebugString("\n");
#endif
}


static IOHANDLE logfile = 0;
static void logger_file(const char *line)
{
	io_write(logfile, line, strlen(line));
	io_write_newline(logfile);
	io_flush(logfile);
}

void dbg_logger_stdout()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR)
	{
		dbg_logger(logger_win_console);
		return;
	}
#endif
	dbg_logger(logger_stdout);
}

void dbg_logger_debugger() { dbg_logger(logger_debugger); }
void dbg_logger_file(const char *filename)
{
	IOHANDLE handle = io_open(filename, IOFLAG_WRITE);
	if(handle)
		dbg_logger_filehandle(handle);
	else
		dbg_msg("dbg/logger", "failed to open '%s' for logging", filename);
}

void dbg_logger_filehandle(IOHANDLE handle)
{
	logfile = handle;
	if(logfile)
		dbg_logger(logger_file);
}

#if defined(CONF_FAMILY_WINDOWS)
static DWORD old_console_mode;

void dbg_console_init()
{
	HANDLE handle;
	DWORD console_mode;

	handle = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(handle, &old_console_mode);
	console_mode = old_console_mode & (~ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS);
	SetConsoleMode(handle, console_mode);
}
void dbg_console_cleanup()
{
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), old_console_mode);
}
#endif
/* */

typedef struct MEMHEADER
{
	const char *filename;
	int line;
	int size;
	struct MEMHEADER *prev;
	struct MEMHEADER *next;
} MEMHEADER;

typedef struct MEMTAIL
{
	int guard;
} MEMTAIL;

static struct MEMHEADER *first = 0;
static const int MEM_GUARD_VAL = 0xbaadc0de;

void *mem_alloc_debug(const char *filename, int line, unsigned size, unsigned alignment)
{
	return malloc(size);
}

void mem_free(void *p)
{
	free(p);
}

void mem_copy(void *dest, const void *source, size_t size)
{
	memcpy(dest, source, size);
}

void mem_move(void *dest, const void *source, size_t size)
{
	memmove(dest, source, size);
}

void mem_zero(void *block, size_t size)
{
	memset(block, 0, size);
}

int mem_check_imp()
{
	MEMHEADER *header = first;
	while(header)
	{
		MEMTAIL *tail = (MEMTAIL *)(((char*)(header+1))+header->size);
		if(tail->guard != MEM_GUARD_VAL)
		{
			dbg_msg("mem", "Memory check failed at %s(%d): %d", header->filename, header->line, header->size);
			return 0;
		}
		header = header->next;
	}

	return 1;
}

IOHANDLE io_open(const char *filename, int flags)
{
	if(flags == IOFLAG_READ)
	{
	#if defined(CONF_FAMILY_WINDOWS)
		// check for filename case sensitive
		WIN32_FIND_DATA finddata;
		HANDLE handle;
		int length;

		length = str_length(filename);
		if(!filename || !length || filename[length-1] == '\\')
			return 0x0;
		handle = FindFirstFile(filename, &finddata);
		if(handle == INVALID_HANDLE_VALUE)
			return 0x0;
		else if(str_comp(filename+length-str_length(finddata.cFileName), finddata.cFileName) != 0)
		{
			FindClose(handle);
			return 0x0;
		}
		FindClose(handle);
	#endif
		return (IOHANDLE)fopen(filename, "rb");
	}
	if(flags == IOFLAG_WRITE)
		return (IOHANDLE)fopen(filename, "wb");
	if(flags == IOFLAG_APPEND)
		return (IOHANDLE)fopen(filename, "ab");
	return 0x0;
}

unsigned io_read(IOHANDLE io, void *buffer, unsigned size)
{
	return fread(buffer, 1, size, (FILE*)io);
}

bool io_read_all(IOHANDLE io, void **result, unsigned *result_len)
{
	// Loading files larger than 1 GiB into memory is not supported.
	constexpr int64_t MAX_FILE_SIZE = (int64_t)1024 * 1024 * 1024;

	int64_t real_len = io_length(io);
	if(real_len > MAX_FILE_SIZE)
	{
		*result = nullptr;
		*result_len = 0;
		return false;
	}

	int64_t len = real_len < 0 ? 1024 : real_len; // use default initial size if we couldn't get the length
	char *buffer = (char *)malloc(len + 1);
	int64_t read = io_read(io, buffer, len + 1); // +1 to check if the file size is larger than expected
	if(read < len)
	{
		buffer = (char *)realloc(buffer, read + 1);
		len = read;
	}
	else if(read > len)
	{
		int64_t cap = 2 * read;
		if(cap > MAX_FILE_SIZE)
		{
			free(buffer);
			*result = nullptr;
			*result_len = 0;
			return false;
		}
		len = read;
		buffer = (char *)realloc(buffer, cap);
		while((read = io_read(io, buffer + len, cap - len)) != 0)
		{
			len += read;
			if(len == cap)
			{
				cap *= 2;
				if(cap > MAX_FILE_SIZE)
				{
					free(buffer);
					*result = nullptr;
					*result_len = 0;
					return false;
				}
				buffer = (char *)realloc(buffer, cap);
			}
		}
		buffer = (char *)realloc(buffer, len + 1);
	}
	buffer[len] = 0;
	*result = buffer;
	*result_len = len;
	return true;
}

char *io_read_all_str(IOHANDLE io)
{
	void *buffer;
	unsigned len;

	if(!io_read_all(io, &buffer, &len))
	{
		return nullptr;
	}
	if(mem_has_null(buffer, len))
	{
		free(buffer);
		return 0;
	}
	return (char *)buffer;
}

unsigned io_unread_byte(IOHANDLE io, unsigned char byte)
{
	return ungetc(byte, (FILE*)io) == EOF;
}

unsigned io_skip(IOHANDLE io, int size)
{
	fseek((FILE*)io, size, SEEK_CUR);
	return size;
}

int io_seek(IOHANDLE io, int offset, int origin)
{
	int real_origin;

	switch(origin)
	{
	case IOSEEK_START:
		real_origin = SEEK_SET;
		break;
	case IOSEEK_CUR:
		real_origin = SEEK_CUR;
		break;
	case IOSEEK_END:
		real_origin = SEEK_END;
		break;
	default:
		return -1;
	}

	return fseek((FILE*)io, offset, real_origin);
}

long int io_tell(IOHANDLE io)
{
	return ftell((FILE*)io);
}

long int io_length(IOHANDLE io)
{
	long int length;
	io_seek(io, 0, IOSEEK_END);
	length = io_tell(io);
	io_seek(io, 0, IOSEEK_START);
	return length;
}

unsigned io_write(IOHANDLE io, const void *buffer, unsigned size)
{
	return fwrite(buffer, 1, size, (FILE*)io);
}

unsigned io_write_newline(IOHANDLE io)
{
#if defined(CONF_FAMILY_WINDOWS)
	return fwrite("\r\n", 1, 2, (FILE*)io);
#else
	return fwrite("\n", 1, 1, (FILE*)io);
#endif
}

int io_close(IOHANDLE io)
{
	fclose((FILE*)io);
	return 0;
}

int io_flush(IOHANDLE io)
{
	fflush((FILE*)io);
	return 0;
}

struct THREAD_RUN
{
	void (*threadfunc)(void *);
	void *u;
};

#if defined(CONF_FAMILY_UNIX)
static void *thread_run(void *user)
#elif defined(CONF_FAMILY_WINDOWS)
static unsigned long __stdcall thread_run(void *user)
#else
#error not implemented
#endif
{
	struct THREAD_RUN *data = (THREAD_RUN *)user;
	void (*threadfunc)(void *) = data->threadfunc;
	void *u = data->u;
	free(data);
	threadfunc(u);
	return 0;
}

void *thread_init(void (*threadfunc)(void *), void *u, const char *name)
{
	struct THREAD_RUN *data = (THREAD_RUN *)malloc(sizeof(*data));
	data->threadfunc = threadfunc;
	data->u = u;
#if defined(CONF_FAMILY_UNIX)
	{
		pthread_t id;
		int result = pthread_create(&id, NULL, thread_run, data);
		if(result != 0)
		{
			dbg_msg("thread", "creating %s thread failed: %d", name, result);
			return 0;
		}
		return (void*)id;
	}
#elif defined(CONF_FAMILY_WINDOWS)
	return CreateThread(NULL, 0, thread_run, data, 0, NULL);
#else
	#error not implemented
#endif
}

void thread_wait(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	pthread_join((pthread_t)thread, NULL);
#elif defined(CONF_FAMILY_WINDOWS)
	WaitForSingleObject((HANDLE)thread, INFINITE);
#else
	#error not implemented
#endif
}

void thread_destroy(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	void *r = 0;
	pthread_join((pthread_t)thread, &r);
#else
	/*#error not implemented*/
#endif
}

void thread_yield()
{
#if defined(CONF_FAMILY_UNIX)
	sched_yield();
#elif defined(CONF_FAMILY_WINDOWS)
	Sleep(0);
#else
	#error not implemented
#endif
}

void thread_sleep(int milliseconds)
{
#if defined(CONF_FAMILY_UNIX)
	usleep(milliseconds*1000);
#elif defined(CONF_FAMILY_WINDOWS)
	Sleep(milliseconds);
#else
	#error not implemented
#endif
}

void thread_detach(void *thread)
{
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)(thread));
#elif defined(CONF_FAMILY_WINDOWS)
	CloseHandle(thread);
#else
	#error not implemented
#endif
}

void *thread_init_and_detach(void (*threadfunc)(void *), void *u, const char *name)
{
	void *thread = thread_init(threadfunc, u, name);
	if(thread)
		thread_detach(thread);
	return thread;
}

void cpu_relax()
{
#if defined(CONF_ARCH_IA32) || defined(CONF_ARCH_AMD64)
	_mm_pause();
#else
	(void) 0;
#endif
}



#if defined(CONF_FAMILY_UNIX)
typedef pthread_mutex_t LOCKINTERNAL;
#elif defined(CONF_FAMILY_WINDOWS)
typedef CRITICAL_SECTION LOCKINTERNAL;
#else
	#error not implemented on this platform
#endif

LOCK lock_create()
{
	LOCKINTERNAL *lock = (LOCKINTERNAL *)malloc(sizeof(*lock));
#if defined(CONF_FAMILY_UNIX)
	int result;
#endif

	if(!lock)
		return 0;

#if defined(CONF_FAMILY_UNIX)
	result = pthread_mutex_init(lock, 0x0);
	if(result != 0)
	{
		dbg_msg("lock", "init failed: %d", result);
		free(lock);
		return 0;
	}
#elif defined(CONF_FAMILY_WINDOWS)
	InitializeCriticalSection((LPCRITICAL_SECTION)lock);
#else
#error not implemented on this platform
#endif
	return (LOCK)lock;
}

void lock_destroy(LOCK lock)
{
#if defined(CONF_FAMILY_UNIX)
	int result = pthread_mutex_destroy((LOCKINTERNAL *)lock);
	if(result != 0)
		dbg_msg("lock", "destroy failed: %d", result);
#elif defined(CONF_FAMILY_WINDOWS)
	DeleteCriticalSection((LPCRITICAL_SECTION)lock);
#else
#error not implemented on this platform
#endif
	free(lock);
}

int lock_trylock(LOCK lock)
{
#if defined(CONF_FAMILY_UNIX)
	return pthread_mutex_trylock((LOCKINTERNAL *)lock);
#elif defined(CONF_FAMILY_WINDOWS)
	return !TryEnterCriticalSection((LPCRITICAL_SECTION)lock);
#else
#error not implemented on this platform
#endif
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
void lock_wait(LOCK lock)
{
#if defined(CONF_FAMILY_UNIX)
	int result = pthread_mutex_lock((LOCKINTERNAL *)lock);
	if(result != 0)
		dbg_msg("lock", "lock failed: %d", result);
#elif defined(CONF_FAMILY_WINDOWS)
	EnterCriticalSection((LPCRITICAL_SECTION)lock);
#else
#error not implemented on this platform
#endif
}

void lock_unlock(LOCK lock)
{
#if defined(CONF_FAMILY_UNIX)
	int result = pthread_mutex_unlock((LOCKINTERNAL *)lock);
	if(result != 0)
		dbg_msg("lock", "unlock failed: %d", result);
#elif defined(CONF_FAMILY_WINDOWS)
	LeaveCriticalSection((LPCRITICAL_SECTION)lock);
#else
#error not implemented on this platform
#endif
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if defined(CONF_FAMILY_WINDOWS)
void sphore_init(SEMAPHORE *sem)
{
	*sem = CreateSemaphore(0, 0, 10000, 0);
}
void sphore_wait(SEMAPHORE *sem) { WaitForSingleObject((HANDLE)*sem, INFINITE); }
void sphore_signal(SEMAPHORE *sem) { ReleaseSemaphore((HANDLE)*sem, 1, NULL); }
void sphore_destroy(SEMAPHORE *sem) { CloseHandle((HANDLE)*sem); }
#elif defined(CONF_PLATFORM_MACOS)
void sphore_init(SEMAPHORE *sem)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%p", (void *)sem);
	*sem = sem_open(aBuf, O_CREAT | O_EXCL, S_IRWXU | S_IRWXG, 0);
	if(*sem == SEM_FAILED)
		dbg_msg("sphore", "init failed: %d", errno);
}
void sphore_wait(SEMAPHORE *sem) { sem_wait(*sem); }
void sphore_signal(SEMAPHORE *sem) { sem_post(*sem); }
void sphore_destroy(SEMAPHORE *sem)
{
	char aBuf[64];
	sem_close(*sem);
	str_format(aBuf, sizeof(aBuf), "%p", (void *)sem);
	sem_unlink(aBuf);
}
#elif defined(CONF_FAMILY_UNIX)
void sphore_init(SEMAPHORE *sem)
{
	if(sem_init(sem, 0, 0) != 0)
		dbg_msg("sphore", "init failed: %d", errno);
}

void sphore_wait(SEMAPHORE *sem)
{
	if(sem_wait(sem) != 0)
		dbg_msg("sphore", "wait failed: %d", errno);
}

void sphore_signal(SEMAPHORE *sem)
{
	if(sem_post(sem) != 0)
		dbg_msg("sphore", "post failed: %d", errno);
}
void sphore_destroy(SEMAPHORE *sem)
{
	if(sem_destroy(sem) != 0)
		dbg_msg("sphore", "destroy failed: %d", errno);
}
#endif


/* -----  time ----- */
int64 time_get()
{
#if defined(CONF_FAMILY_UNIX)
	struct timeval val;
	gettimeofday(&val, NULL);
	return (int64)val.tv_sec*(int64)1000000+(int64)val.tv_usec;
#elif defined(CONF_FAMILY_WINDOWS)
	static int64 last = 0;
	int64 t;
	QueryPerformanceCounter((PLARGE_INTEGER)&t);
	if(t<last) /* for some reason, QPC can return values in the past */
		return last;
	last = t;
	return t;
#else
	#error not implemented
#endif
}

int64 time_freq()
{
#if defined(CONF_FAMILY_UNIX)
	return 1000000;
#elif defined(CONF_FAMILY_WINDOWS)
	int64 t;
	QueryPerformanceFrequency((PLARGE_INTEGER)&t);
	return t;
#else
	#error not implemented
#endif
}

/* -----  network ----- */
static void netaddr_to_sockaddr_in(const NETADDR *src, struct sockaddr_in *dest)
{
	mem_zero(dest, sizeof(struct sockaddr_in));
	if(src->type != NETTYPE_IPV4)
	{
		dbg_msg("system", "couldn't convert NETADDR of type %d to ipv4", src->type);
		return;
	}

	dest->sin_family = AF_INET;
	dest->sin_port = htons(src->port);
	mem_copy(&dest->sin_addr.s_addr, src->ip, 4);
}

static void netaddr_to_sockaddr_in6(const NETADDR *src, struct sockaddr_in6 *dest)
{
	mem_zero(dest, sizeof(struct sockaddr_in6));
	if(src->type != NETTYPE_IPV6)
	{
		dbg_msg("system", "couldn't not convert NETADDR of type %d to ipv6", src->type);
		return;
	}

	dest->sin6_family = AF_INET6;
	dest->sin6_port = htons(src->port);
	mem_copy(&dest->sin6_addr.s6_addr, src->ip, 16);
}

static void sockaddr_to_netaddr(const struct sockaddr *src, NETADDR *dst)
{
	if(src->sa_family == AF_INET)
	{
		mem_zero(dst, sizeof(NETADDR));
		dst->type = NETTYPE_IPV4;
		dst->port = htons(((struct sockaddr_in*)src)->sin_port);
		mem_copy(dst->ip, &((struct sockaddr_in*)src)->sin_addr.s_addr, 4);
	}
	else if(src->sa_family == AF_INET6)
	{
		mem_zero(dst, sizeof(NETADDR));
		dst->type = NETTYPE_IPV6;
		dst->port = htons(((struct sockaddr_in6*)src)->sin6_port);
		mem_copy(dst->ip, &((struct sockaddr_in6*)src)->sin6_addr.s6_addr, 16);
	}
	else
	{
		mem_zero(dst, sizeof(struct sockaddr));
		dbg_msg("system", "couldn't convert sockaddr of family %d", src->sa_family);
	}
}

int net_addr_comp(const NETADDR *a, const NETADDR *b, int check_port)
{
	if(a->type == b->type && mem_comp(a->ip, b->ip, a->type == NETTYPE_IPV4 ? NETADDR_SIZE_IPV4 : NETADDR_SIZE_IPV6) == 0 && (!check_port || a->port == b->port))
		return 0;
	return -1;
}

void net_addr_str(const NETADDR *addr, char *string, int max_length, int add_port)
{
	if(addr->type == NETTYPE_IPV4)
	{
		if(add_port != 0)
			str_format(string, max_length, "%d.%d.%d.%d:%d", addr->ip[0], addr->ip[1], addr->ip[2], addr->ip[3], addr->port);
		else
			str_format(string, max_length, "%d.%d.%d.%d", addr->ip[0], addr->ip[1], addr->ip[2], addr->ip[3]);
	}
	else if(addr->type == NETTYPE_IPV6)
	{
		if(add_port != 0)
			str_format(string, max_length, "[%x:%x:%x:%x:%x:%x:%x:%x]:%d",
				(addr->ip[0]<<8)|addr->ip[1], (addr->ip[2]<<8)|addr->ip[3], (addr->ip[4]<<8)|addr->ip[5], (addr->ip[6]<<8)|addr->ip[7],
				(addr->ip[8]<<8)|addr->ip[9], (addr->ip[10]<<8)|addr->ip[11], (addr->ip[12]<<8)|addr->ip[13], (addr->ip[14]<<8)|addr->ip[15],
				addr->port);
		else
			str_format(string, max_length, "[%x:%x:%x:%x:%x:%x:%x:%x]",
				(addr->ip[0]<<8)|addr->ip[1], (addr->ip[2]<<8)|addr->ip[3], (addr->ip[4]<<8)|addr->ip[5], (addr->ip[6]<<8)|addr->ip[7],
				(addr->ip[8]<<8)|addr->ip[9], (addr->ip[10]<<8)|addr->ip[11], (addr->ip[12]<<8)|addr->ip[13], (addr->ip[14]<<8)|addr->ip[15]);
	}
	else
		str_format(string, max_length, "unknown type %d", addr->type);
}

static int priv_net_extract(const char *hostname, char *host, int max_host, int *port)
{
	int i;

	*port = 0;
	host[0] = 0;

	if(hostname[0] == '[')
	{
		// ipv6 mode
		for(i = 1; i < max_host && hostname[i] && hostname[i] != ']'; i++)
			host[i-1] = hostname[i];
		host[i-1] = 0;
		if(hostname[i] != ']') // malformatted
			return -1;

		i++;
		if(hostname[i] == ':')
			*port = atol(hostname+i+1);
	}
	else
	{
		// generic mode (ipv4, hostname etc)
		for(i = 0; i < max_host-1 && hostname[i] && hostname[i] != ':'; i++)
			host[i] = hostname[i];
		host[i] = 0;

		if(hostname[i] == ':')
			*port = atol(hostname+i+1);
	}

	return 0;
}

int net_host_lookup(const char *hostname, NETADDR *addr, int types)
{
	struct addrinfo hints;
	struct addrinfo *result;
	int e;
	char host[256];
	int port = 0;

	if(priv_net_extract(hostname, host, sizeof(host), &port))
		return -1;
	/*
	dbg_msg("host lookup", "host='%s' port=%d %d", host, port, types);
	*/

	mem_zero(&hints, sizeof(hints));

	hints.ai_family = AF_UNSPEC;

	if(types == NETTYPE_IPV4)
		hints.ai_family = AF_INET;
	else if(types == NETTYPE_IPV6)
		hints.ai_family = AF_INET6;

	e = getaddrinfo(host, NULL, &hints, &result);
	if(e != 0 || !result)
		return -1;

	sockaddr_to_netaddr(result->ai_addr, addr);
	freeaddrinfo(result);
	addr->port = port;
	return 0;
}

static int parse_int(int *out, const char **str)
{
	int i = 0;
	*out = 0;
	if(**str < '0' || **str > '9')
		return -1;

	i = **str - '0';
	(*str)++;

	while(1)
	{
		if(**str < '0' || **str > '9')
		{
			*out = i;
			return 0;
		}

		i = (i*10) + (**str - '0');
		(*str)++;
	}

	return 0;
}

static int parse_char(char c, const char **str)
{
	if(**str != c) return -1;
	(*str)++;
	return 0;
}

static int parse_uint8(unsigned char *out, const char **str)
{
	int i;
	if(parse_int(&i, str) != 0) return -1;
	if(i < 0 || i > 0xff) return -1;
	*out = i;
	return 0;
}

static int parse_uint16(unsigned short *out, const char **str)
{
	int i;
	if(parse_int(&i, str) != 0) return -1;
	if(i < 0 || i > 0xffff) return -1;
	*out = i;
	return 0;
}

int net_addr_from_str(NETADDR *addr, const char *string)
{
	const char *str = string;
	mem_zero(addr, sizeof(NETADDR));

	if(str[0] == '[')
	{
		/* ipv6 */
		struct sockaddr_in6 sa6;
		char buf[128];
		int i;
		str++;
		for(i = 0; i < 127 && str[i] && str[i] != ']'; i++)
			buf[i] = str[i];
		buf[i] = 0;
		str += i;
#if defined(CONF_FAMILY_WINDOWS)
		{
			int size;
			sa6.sin6_family = AF_INET6;
			size = (int)sizeof(sa6);
			if(WSAStringToAddress(buf, AF_INET6, NULL, (struct sockaddr *)&sa6, &size) != 0)
				return -1;
		}
#else
		sa6.sin6_family = AF_INET6;

		if(inet_pton(AF_INET6, buf, &sa6.sin6_addr) != 1)
			return -1;
#endif
		sockaddr_to_netaddr((struct sockaddr *)&sa6, addr);

		if(*str == ']')
		{
			str++;
			if(*str == ':')
			{
				str++;
				if(parse_uint16(&addr->port, &str))
					return -1;
			}
		}
		else
			return -1;

		return 0;
	}
	else
	{
		/* ipv4 */
		if(parse_uint8(&addr->ip[0], &str)) return -1;
		if(parse_char('.', &str)) return -1;
		if(parse_uint8(&addr->ip[1], &str)) return -1;
		if(parse_char('.', &str)) return -1;
		if(parse_uint8(&addr->ip[2], &str)) return -1;
		if(parse_char('.', &str)) return -1;
		if(parse_uint8(&addr->ip[3], &str)) return -1;
		if(*str == ':')
		{
			str++;
			if(parse_uint16(&addr->port, &str)) return -1;
		}

		addr->type = NETTYPE_IPV4;
	}

	return 0;
}

static void priv_net_close_socket(int sock)
{
#if defined(CONF_FAMILY_WINDOWS)
	closesocket(sock);
#else
	close(sock);
#endif
}

static int priv_net_close_all_sockets(NETSOCKET sock)
{
	/* close down ipv4 */
	if(sock->ipv4sock >= 0)
	{
		priv_net_close_socket(sock->ipv4sock);
		sock->ipv4sock = -1;
		sock->type &= ~NETTYPE_IPV4;
	}

	/* close down ipv6 */
	if(sock->ipv6sock >= 0)
	{
		priv_net_close_socket(sock->ipv6sock);
		sock->ipv6sock = -1;
		sock->type &= ~NETTYPE_IPV6;
	}

	free(sock);
	return 0;
}

static int priv_net_create_socket(int domain, int type, struct sockaddr *addr, int sockaddrlen, int use_random_port)
{
	int sock, e;

	/* create socket */
	sock = socket(domain, type, 0);
	if(sock < 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		char buf[128];
		int error = WSAGetLastError();
		if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 0, error, 0, buf, sizeof(buf), 0) == 0)
			buf[0] = 0;
		dbg_msg("net", "failed to create socket with domain %d and type %d (%d '%s')", domain, type, error, buf);
#else
		dbg_msg("net", "failed to create socket with domain %d and type %d (%d '%s')", domain, type, errno, strerror(errno));
#endif
		return -1;
	}

	/* set to IPv6 only if thats what we are creating */
#if defined(IPV6_V6ONLY)	/* windows sdk 6.1 and higher */
	if(domain == AF_INET6)
	{
		int ipv6only = 1;
		setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&ipv6only, sizeof(ipv6only));
	}
#endif

	/* bind the socket */
	while(1)
	{
		/* pick random port */
		if(use_random_port)
		{
			int port = htons(rand()%16384+49152);	/* 49152 to 65535 */
			if(domain == AF_INET)
				((struct sockaddr_in *)(addr))->sin_port = port;
			else
				((struct sockaddr_in6 *)(addr))->sin6_port = port;
		}

		e = bind(sock, addr, sockaddrlen);
		if(e == 0)
			break;
		else
		{
#if defined(CONF_FAMILY_WINDOWS)
			char buf[128];
			int error = WSAGetLastError();
			if(error == WSAEADDRINUSE && use_random_port)
				continue;
			if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 0, error, 0, buf, sizeof(buf), 0) == 0)
				buf[0] = 0;
			dbg_msg("net", "failed to bind socket with domain %d and type %d (%d '%s')", domain, type, error, buf);
#else
			if(errno == EADDRINUSE && use_random_port)
				continue;
			dbg_msg("net", "failed to bind socket with domain %d and type %d (%d '%s')", domain, type, errno, strerror(errno));
#endif
			priv_net_close_socket(sock);
			return -1;
		}
	}

	/* return the newly created socket */
	return sock;
}

int net_socket_type(NETSOCKET sock)
{
	return sock->type;
}

NETSOCKET net_udp_create(NETADDR bindaddr, int use_random_port)
{
	NETSOCKET sock = (NETSOCKET_INTERNAL *)malloc(sizeof(*sock));
	*sock = invalid_socket;
	NETADDR tmpbindaddr = bindaddr;
	int broadcast = 1;
	int socket = -1;
	int recvsize = 65536;

	if(bindaddr.type&NETTYPE_IPV4)
	{
		struct sockaddr_in addr;

		/* bind, we should check for error */
		tmpbindaddr.type = NETTYPE_IPV4;
		netaddr_to_sockaddr_in(&tmpbindaddr, &addr);
		socket = priv_net_create_socket(AF_INET, SOCK_DGRAM, (struct sockaddr *)&addr, sizeof(addr), use_random_port);
		if(socket >= 0)
		{
			sock->type |= NETTYPE_IPV4;
			sock->ipv4sock = socket;

			/* set broadcast */
			setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

			/* set receive buffer size */
			setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvsize, sizeof(recvsize));
		}
	}

	if(bindaddr.type&NETTYPE_IPV6)
	{
		struct sockaddr_in6 addr;

		/* bind, we should check for error */
		tmpbindaddr.type = NETTYPE_IPV6;
		netaddr_to_sockaddr_in6(&tmpbindaddr, &addr);
		socket = priv_net_create_socket(AF_INET6, SOCK_DGRAM, (struct sockaddr *)&addr, sizeof(addr), use_random_port);
		if(socket >= 0)
		{
			sock->type |= NETTYPE_IPV6;
			sock->ipv6sock = socket;

			/* set broadcast */
			setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

			/* set receive buffer size */
			setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvsize, sizeof(recvsize));
		}
	}

	if(socket < 0)
	{
		free(sock);
		sock = nullptr;
	}
	else
	{
		/* set non-blocking */
		net_set_non_blocking(sock);

		net_buffer_init(&sock->buffer);
	}

	/* return */
	return sock;
}

int net_udp_send(NETSOCKET sock, const NETADDR *addr, const void *data, int size)
{
	int d = -1;

	if(addr->type&NETTYPE_IPV4)
	{
		if(sock->ipv4sock >= 0)
		{
			struct sockaddr_in sa;
			if(addr->type&NETTYPE_LINK_BROADCAST)
			{
				mem_zero(&sa, sizeof(sa));
				sa.sin_port = htons(addr->port);
				sa.sin_family = AF_INET;
				sa.sin_addr.s_addr = INADDR_BROADCAST;
			}
			else
				netaddr_to_sockaddr_in(addr, &sa);

			d = sendto((int)sock->ipv4sock, (const char*)data, size, 0, (struct sockaddr *)&sa, sizeof(sa));
		}
		else
			dbg_msg("net", "can't sent ipv4 traffic to this socket");
	}

	if(addr->type&NETTYPE_IPV6)
	{
		if(sock->ipv6sock >= 0)
		{
			struct sockaddr_in6 sa;
			if(addr->type&NETTYPE_LINK_BROADCAST)
			{
				mem_zero(&sa, sizeof(sa));
				sa.sin6_port = htons(addr->port);
				sa.sin6_family = AF_INET6;
				sa.sin6_addr.s6_addr[0] = 0xff; /* multicast */
				sa.sin6_addr.s6_addr[1] = 0x02; /* link local scope */
				sa.sin6_addr.s6_addr[15] = 1; /* all nodes */
			}
			else
				netaddr_to_sockaddr_in6(addr, &sa);

			d = sendto((int)sock->ipv6sock, (const char*)data, size, 0, (struct sockaddr *)&sa, sizeof(sa));
		}
		else
			dbg_msg("net", "can't sent ipv6 traffic to this socket");
	}
	/*
	else
		dbg_msg("net", "can't sent to network of type %d", addr->type);
		*/

	/*if(d < 0)
	{
		char addrstr[256];
		net_addr_str(addr, addrstr, sizeof(addrstr));

		dbg_msg("net", "sendto error (%d '%s')", errno, strerror(errno));
		dbg_msg("net", "\tsock = %d %x", sock, sock);
		dbg_msg("net", "\tsize = %d %x", size, size);
		dbg_msg("net", "\taddr = %s", addrstr);

	}*/
	network_stats.sent_bytes += size;
	network_stats.sent_packets++;
	return d;
}

void net_buffer_init(NETSOCKET_BUFFER *buffer)
{
#if defined(CONF_PLATFORM_LINUX)
	int i;
	buffer->pos = 0;
	buffer->size = 0;
	mem_zero(buffer->msgs, sizeof(buffer->msgs));
	mem_zero(buffer->iovecs, sizeof(buffer->iovecs));
	mem_zero(buffer->sockaddrs, sizeof(buffer->sockaddrs));
	for(i = 0; i < VLEN; ++i)
	{
		buffer->iovecs[i].iov_base = buffer->bufs[i];
		buffer->iovecs[i].iov_len = PACKETSIZE;
		buffer->msgs[i].msg_hdr.msg_iov = &(buffer->iovecs[i]);
		buffer->msgs[i].msg_hdr.msg_iovlen = 1;
		buffer->msgs[i].msg_hdr.msg_name = &(buffer->sockaddrs[i]);
		buffer->msgs[i].msg_hdr.msg_namelen = sizeof(buffer->sockaddrs[i]);
	}
#endif
}

void net_buffer_reinit(NETSOCKET_BUFFER *buffer)
{
#if defined(CONF_PLATFORM_LINUX)
	for(int i = 0; i < VLEN; i++)
	{
		buffer->msgs[i].msg_hdr.msg_namelen = sizeof(buffer->sockaddrs[i]);
	}
#endif
}

void net_buffer_simple(NETSOCKET_BUFFER *buffer, char **buf, int *size)
{
#if defined(CONF_PLATFORM_LINUX)
	*buf = buffer->bufs[0];
	*size = sizeof(buffer->bufs[0]);
#else
	*buf = buffer->buf;
	*size = sizeof(buffer->buf);
#endif
}

int net_udp_recv(NETSOCKET sock, NETADDR *addr, unsigned char **data)
{
	char sockaddrbuf[128];
	int bytes = 0;

	#if defined(CONF_PLATFORM_LINUX)
	if(sock->ipv4sock >= 0)
	{
		if(sock->buffer.pos >= sock->buffer.size)
		{
			net_buffer_reinit(&sock->buffer);
			sock->buffer.size = recvmmsg(sock->ipv4sock, sock->buffer.msgs, VLEN, 0, NULL);
			sock->buffer.pos = 0;
		}
	}

	if(sock->ipv6sock >= 0)
	{
		if(sock->buffer.pos >= sock->buffer.size)
		{
			net_buffer_reinit(&sock->buffer);
			sock->buffer.size = recvmmsg(sock->ipv6sock, sock->buffer.msgs, VLEN, 0, NULL);
			sock->buffer.pos = 0;
		}
	}

	if(sock->buffer.pos < sock->buffer.size)
	{
		sockaddr_to_netaddr((struct sockaddr *)&(sock->buffer.sockaddrs[sock->buffer.pos]), addr);
		bytes = sock->buffer.msgs[sock->buffer.pos].msg_len;
		*data = (unsigned char *)sock->buffer.bufs[sock->buffer.pos];
		sock->buffer.pos++;
		network_stats.recv_bytes += bytes;
		network_stats.recv_packets++;
		return bytes;
	}
#else
	if(sock->ipv4sock >= 0)
	{
		socklen_t fromlen = sizeof(struct sockaddr_in);
		bytes = recvfrom(sock->ipv4sock, sock->buffer.buf, sizeof(sock->buffer.buf), 0, (struct sockaddr *)&sockaddrbuf, &fromlen);
		*data = (unsigned char *)sock->buffer.buf;
	}

	if(bytes <= 0 && sock->ipv6sock >= 0)
	{
		socklen_t fromlen = sizeof(struct sockaddr_in6);
		bytes = recvfrom(sock->ipv6sock, sock->buffer.buf, sizeof(sock->buffer.buf), 0, (struct sockaddr *)&sockaddrbuf, &fromlen);
		*data = (unsigned char *)sock->buffer.buf;
	}
#endif

	if(bytes > 0)
	{
		sockaddr_to_netaddr((struct sockaddr *)&sockaddrbuf, addr);
		network_stats.recv_bytes += bytes;
		network_stats.recv_packets++;
		return bytes;
	}
	else if(bytes == 0)
		return 0;
	return -1; /* error */
}

int net_udp_close(NETSOCKET sock)
{
	return priv_net_close_all_sockets(sock);
}

NETSOCKET net_tcp_create(NETADDR bindaddr)
{
	NETSOCKET sock = (NETSOCKET_INTERNAL *)malloc(sizeof(*sock));
	*sock = invalid_socket;
	NETADDR tmpbindaddr = bindaddr;

	if(bindaddr.type&NETTYPE_IPV4)
	{
		struct sockaddr_in addr;
		int socket = -1;

		/* bind, we should check for error */
		tmpbindaddr.type = NETTYPE_IPV4;
		netaddr_to_sockaddr_in(&tmpbindaddr, &addr);
		socket = priv_net_create_socket(AF_INET, SOCK_STREAM, (struct sockaddr *)&addr, sizeof(addr), 0);
		if(socket >= 0)
		{
			sock->type |= NETTYPE_IPV4;
			sock->ipv4sock = socket;
		}
	}

	if(bindaddr.type&NETTYPE_IPV6)
	{
		struct sockaddr_in6 addr;
		int socket = -1;

		/* bind, we should check for error */
		tmpbindaddr.type = NETTYPE_IPV6;
		netaddr_to_sockaddr_in6(&tmpbindaddr, &addr);
		socket = priv_net_create_socket(AF_INET6, SOCK_STREAM, (struct sockaddr *)&addr, sizeof(addr), 0);
		if(socket >= 0)
		{
			sock->type |= NETTYPE_IPV6;
			sock->ipv6sock = socket;
		}
	}

	/* return */
	return sock;
}

int net_tcp_set_linger(NETSOCKET sock, int state)
{
	struct linger linger_state;
	linger_state.l_onoff = state;
	linger_state.l_linger = 0;

	if(sock->ipv4sock >= 0)
	{
		/*	set linger	*/
		setsockopt(sock->ipv4sock, SOL_SOCKET, SO_LINGER, (const char*)&linger_state, sizeof(linger_state));
	}

	if(sock->ipv6sock >= 0)
	{
		/*	set linger	*/
		setsockopt(sock->ipv6sock, SOL_SOCKET, SO_LINGER, (const char*)&linger_state, sizeof(linger_state));
	}

	return 0;
}

int net_set_non_blocking(NETSOCKET sock)
{
	unsigned long mode = 1;
	if(sock->ipv4sock >= 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		ioctlsocket(sock->ipv4sock, FIONBIO, (unsigned long *)&mode);
#else
		ioctl(sock->ipv4sock, FIONBIO, (unsigned long *)&mode);
#endif
	}

	if(sock->ipv6sock >= 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		ioctlsocket(sock->ipv6sock, FIONBIO, (unsigned long *)&mode);
#else
		ioctl(sock->ipv6sock, FIONBIO, (unsigned long *)&mode);
#endif
	}

	return 0;
}

int net_set_blocking(NETSOCKET sock)
{
	unsigned long mode = 0;
	if(sock->ipv4sock >= 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		ioctlsocket(sock->ipv4sock, FIONBIO, (unsigned long *)&mode);
#else
		ioctl(sock->ipv4sock, FIONBIO, (unsigned long *)&mode);
#endif
	}

	if(sock->ipv6sock >= 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		ioctlsocket(sock->ipv6sock, FIONBIO, (unsigned long *)&mode);
#else
		ioctl(sock->ipv6sock, FIONBIO, (unsigned long *)&mode);
#endif
	}

	return 0;
}

int net_tcp_listen(NETSOCKET sock, int backlog)
{
	int err = -1;
	if(sock->ipv4sock >= 0)
		err = listen(sock->ipv4sock, backlog);
	if(sock->ipv6sock >= 0)
		err = listen(sock->ipv6sock, backlog);
	return err;
}

int net_tcp_accept(NETSOCKET sock, NETSOCKET *new_sock, NETADDR *a)
{
	int s;
	socklen_t sockaddr_len;

	*new_sock = nullptr;

	if(sock->ipv4sock >= 0)
	{
		struct sockaddr_in addr;
		sockaddr_len = sizeof(addr);

		s = accept(sock->ipv4sock, (struct sockaddr *)&addr, &sockaddr_len);

		if (s != -1)
		{
			sockaddr_to_netaddr((const struct sockaddr *)&addr, a);

			*new_sock = (NETSOCKET_INTERNAL *)malloc(sizeof(**new_sock));
			**new_sock = invalid_socket;
			(*new_sock)->type = NETTYPE_IPV4;
			(*new_sock)->ipv4sock = s;
			return s;
		}
	}

	if(sock->ipv6sock >= 0)
	{
		struct sockaddr_in6 addr;
		sockaddr_len = sizeof(addr);

		s = accept(sock->ipv6sock, (struct sockaddr *)&addr, &sockaddr_len);

		if (s != -1)
		{
			sockaddr_to_netaddr((const struct sockaddr *)&addr, a);

			*new_sock = (NETSOCKET_INTERNAL *)malloc(sizeof(**new_sock));
			**new_sock = invalid_socket;
			(*new_sock)->type = NETTYPE_IPV6;
			(*new_sock)->ipv6sock = s;
			return s;
		}
	}

	return -1;
}

int net_tcp_connect(NETSOCKET sock, const NETADDR *a)
{
	if(a->type&NETTYPE_IPV4)
	{
		struct sockaddr_in addr;
		netaddr_to_sockaddr_in(a, &addr);
		return connect(sock->ipv4sock, (struct sockaddr *)&addr, sizeof(addr));
	}

	if(a->type&NETTYPE_IPV6)
	{
		struct sockaddr_in6 addr;
		netaddr_to_sockaddr_in6(a, &addr);
		return connect(sock->ipv6sock, (struct sockaddr *)&addr, sizeof(addr));
	}

	return -1;
}

int net_tcp_connect_non_blocking(NETSOCKET sock, NETADDR bindaddr)
{
	int res = 0;

	net_set_non_blocking(sock);
	res = net_tcp_connect(sock, &bindaddr);
	net_set_blocking(sock);

	return res;
}

int net_tcp_send(NETSOCKET sock, const void *data, int size)
{
	int bytes = -1;

	if(sock->ipv4sock >= 0)
		bytes = send((int)sock->ipv4sock, (const char*)data, size, 0);
	if(sock->ipv6sock >= 0)
		bytes = send((int)sock->ipv6sock, (const char*)data, size, 0);

	return bytes;
}

int net_tcp_recv(NETSOCKET sock, void *data, int maxsize)
{
	int bytes = -1;

	if(sock->ipv4sock >= 0)
		bytes = recv((int)sock->ipv4sock, (char*)data, maxsize, 0);
	if(sock->ipv6sock >= 0)
		bytes = recv((int)sock->ipv6sock, (char*)data, maxsize, 0);

	return bytes;
}

int net_tcp_close(NETSOCKET sock)
{
	return priv_net_close_all_sockets(sock);
}

int net_errno()
{
#if defined(CONF_FAMILY_WINDOWS)
	return WSAGetLastError();
#else
	return errno;
#endif
}

int net_would_block()
{
#if defined(CONF_FAMILY_WINDOWS)
	return net_errno() == WSAEWOULDBLOCK;
#else
	return net_errno() == EWOULDBLOCK;
#endif
}

#if defined(CONF_FAMILY_UNIX)
UNIXSOCKET net_unix_create_unnamed()
{
	return socket(AF_UNIX, SOCK_DGRAM, 0);
}

int net_unix_send(UNIXSOCKET sock, UNIXSOCKETADDR *addr, void *data, int size)
{
	return sendto(sock, data, size, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_un));
}

void net_unix_set_addr(UNIXSOCKETADDR *addr, const char *path)
{
	mem_zero(addr, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	str_copy(addr->sun_path, path);
}

void net_unix_close(UNIXSOCKET sock)
{
	close(sock);
}
#endif

void net_invalidate_socket(NETSOCKET *socket)
{
	*socket = nullptr;
}

int net_init()
{
#if defined(CONF_FAMILY_WINDOWS)
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(1, 1), &wsaData);
	dbg_assert(err == 0, "network initialization failed.");
	return err==0?0:1;
#endif

	return 0;
}

#if defined (CONF_FAMILY_WINDOWS)
static inline time_t filetime_to_unixtime(LPFILETIME filetime)
{
	time_t t;
	ULARGE_INTEGER li;
	li.LowPart = filetime->dwLowDateTime;
	li.HighPart = filetime->dwHighDateTime;

	li.QuadPart /= 10000000; // 100ns to 1s
	li.QuadPart -= 11644473600LL; // Windows epoch is in the past

	t = li.QuadPart;
	return t == li.QuadPart ? t : (time_t)-1;
}
#endif

void fs_listdir(const char *dir, FS_LISTDIR_CALLBACK cb, int type, void *user)
{
#if defined(CONF_FAMILY_WINDOWS)
	WIN32_FIND_DATA finddata;
	HANDLE handle;
	char buffer[1024*2];
	int length;
	str_format(buffer, sizeof(buffer), "%s/*", dir);

	handle = FindFirstFileA(buffer, &finddata);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	str_format(buffer, sizeof(buffer), "%s/", dir);
	length = str_length(buffer);

	/* add all the entries */
	do
	{
		str_copy(buffer+length, finddata.cFileName, (int)sizeof(buffer)-length);
		if(cb(finddata.cFileName, fs_is_dir(buffer), type, user))
			break;
	}
	while (FindNextFileA(handle, &finddata));

	FindClose(handle);
	return;
#else
	struct dirent *entry;
	char buffer[1024*2];
	int length;
	DIR *d = opendir(dir);

	if(!d)
		return;

	str_format(buffer, sizeof(buffer), "%s/", dir);
	length = str_length(buffer);

	while((entry = readdir(d)) != NULL)
	{
		str_copy(buffer+length, entry->d_name, (int)sizeof(buffer)-length);
		if(cb(entry->d_name, fs_is_dir(buffer), type, user))
			break;
	}

	/* close the directory and return */
	closedir(d);
	return;
#endif
}

void fs_listdir_fileinfo(const char* dir, FS_LISTDIR_CALLBACK_FILEINFO cb, int type, void* user)
{
#if defined(CONF_FAMILY_WINDOWS)
	WIN32_FIND_DATA finddata;
	HANDLE handle;
	char buffer[1024*2];
	int length;
	str_format(buffer, sizeof(buffer), "%s/*", dir);

	handle = FindFirstFileA(buffer, &finddata);

	if (handle == INVALID_HANDLE_VALUE)
		return;

	str_format(buffer, sizeof(buffer), "%s/", dir);
	length = str_length(buffer);

	/* add all the entries */
	do
	{
		str_copy(buffer+length, finddata.cFileName, (int)sizeof(buffer)-length);

		CFsFileInfo info;
		info.m_pName = finddata.cFileName;
		info.m_TimeCreated = filetime_to_unixtime(&finddata.ftCreationTime);
		info.m_TimeModified = filetime_to_unixtime(&finddata.ftLastWriteTime);

		if(cb(&info, fs_is_dir(buffer), type, user))
			break;
	}
	while (FindNextFileA(handle, &finddata));

	FindClose(handle);
	return;
#else
	struct dirent *entry;
	time_t created = -1, modified = -1;
	char buffer[1024*2];
	int length;
	DIR *d = opendir(dir);

	if(!d)
		return;

	str_format(buffer, sizeof(buffer), "%s/", dir);
	length = str_length(buffer);

	while((entry = readdir(d)) != NULL)
	{
		CFsFileInfo info;

		str_copy(buffer+length, entry->d_name, (int)sizeof(buffer)-length);
		fs_file_time(buffer, &created, &modified);

		info.m_pName = entry->d_name;
		info.m_TimeCreated = created;
		info.m_TimeModified = modified;

		if(cb(&info, fs_is_dir(buffer), type, user))
			break;
	}

	/* close the directory and return */
	closedir(d);
	return;
#endif
}

int fs_storage_path(const char *appname, char *path, int max)
{
#if defined(CONF_FAMILY_WINDOWS)
	char *home = getenv("APPDATA");
	if(!home)
		return -1;
	str_format(path, max, "%s/%s", home, appname);
	return 0;
#else
	char *home = getenv("HOME");
	int i;
	char *xdgdatahome = getenv("XDG_DATA_HOME");
	char xdgpath[max];

	if(!home)
		return -1;

#if defined(CONF_PLATFORM_MACOSX)
	str_format(path, max, "%s/Library/Application Support/%s", home, appname);
	return 0;
#endif

	/* old folder location */
	str_format(path, max, "%s/.%s", home, appname);
	for(i = strlen(home)+2; path[i]; i++)
		path[i] = tolower(path[i]);

	if(!xdgdatahome)
	{
		/* use default location */
		str_format(xdgpath, max, "%s/.local/share/%s", home, appname);
		for(i = strlen(home)+14; xdgpath[i]; i++)
			xdgpath[i] = tolower(xdgpath[i]);
	}
	else
	{
		str_format(xdgpath, max, "%s/%s", xdgdatahome, appname);
		for(i = strlen(xdgdatahome)+1; xdgpath[i]; i++)
			xdgpath[i] = tolower(xdgpath[i]);
	}

	/* check for old location / backward compatibility */
	if(fs_is_dir(path))
	{
		/* use old folder path */
		/* for backward compatibility */
		return 0;
	}

	str_format(path, max, "%s", xdgpath);

	return 0;
#endif
}

int fs_makedir_rec_for(const char *path)
{
	char buffer[1024 * 2];
	char *p;
	str_copy(buffer, path);
	for(p = buffer + 1; *p != '\0'; p++)
	{
		if(*p == '/' && *(p + 1) != '\0')
		{
			*p = '\0';
			if(fs_makedir(buffer) < 0)
				return -1;
			*p = '/';
		}
	}
	return 0;
}

int fs_makedir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	if(_mkdir(path) == 0)
			return 0;
	if(errno == EEXIST)
		return 0;
	return -1;
#else
	if(mkdir(path, 0755) == 0)
		return 0;
	if(errno == EEXIST)
		return 0;
	return -1;
#endif
}

int fs_makedir_recursive(const char *path)
{
	char buffer[2048];
	int len;
	int i;
	str_copy(buffer, path, sizeof(buffer));
	len = str_length(buffer);
	// ignore a leading slash
	for(i = 1; i < len; i++)
	{
		char b = buffer[i];
		if(b == '/' || (b == '\\' && buffer[i-1] != ':'))
		{
			buffer[i] = 0;
			if(fs_makedir(buffer) < 0)
			{
				return -1;
			}
			buffer[i] = b;

		}
	}
	return fs_makedir(path);
}

int fs_is_dir(const char *path)
{
#if defined(CONF_FAMILY_WINDOWS)
	/* TODO: do this smarter */
	WIN32_FIND_DATA finddata;
	HANDLE handle;
	char buffer[1024*2];
	str_format(buffer, sizeof(buffer), "%s/*", path);

	if ((handle = FindFirstFileA(buffer, &finddata)) == INVALID_HANDLE_VALUE)
		return 0;

	FindClose(handle);
	return 1;
#else
	struct stat sb;
	if (stat(path, &sb) == -1)
		return 0;

	if (S_ISDIR(sb.st_mode))
		return 1;
	else
		return 0;
#endif
}

time_t fs_getmtime(const char *path)
{
	struct stat sb;
	if(stat(path, &sb) == -1)
		return 0;

	return sb.st_mtime;
}

int fs_chdir(const char *path)
{
	if(fs_is_dir(path))
	{
		if(chdir(path))
			return 1;
		else
			return 0;
	}
	else
		return 1;
}

char *fs_getcwd(char *buffer, int buffer_size)
{
	if(buffer == 0)
		return 0;
#if defined(CONF_FAMILY_WINDOWS)
	return _getcwd(buffer, buffer_size);
#else
	return getcwd(buffer, buffer_size);
#endif
}

int fs_parent_dir(char *path)
{
	char *parent = 0;
	for(; *path; ++path)
	{
		if(*path == '/' || *path == '\\')
			parent = path;
	}

	if(parent)
	{
		*parent = 0;
		return 0;
	}
	return 1;
}

int fs_remove(const char *filename)
{
	if(remove(filename) != 0)
		return 1;
	return 0;
}

int fs_rename(const char *oldname, const char *newname)
{
	if(rename(oldname, newname) != 0)
		return 1;
	return 0;
}

int fs_read(const char *name, void **result, unsigned *result_len)
{
	IOHANDLE file = io_open(name, IOFLAG_READ);
	*result = 0;
	*result_len = 0;
	if(!file)
	{
		return 1;
	}
	io_read_all(file, result, result_len);
	io_close(file);
	return 0;
}

char *fs_read_str(const char *name)
{
	IOHANDLE file = io_open(name, IOFLAG_READ);
	char *result;
	if(!file)
	{
		return 0;
	}
	result = io_read_all_str(file);
	io_close(file);
	return result;
}

int fs_file_time(const char *name, time_t *created, time_t *modified)
{
#if defined(CONF_FAMILY_WINDOWS)
	WIN32_FIND_DATA finddata;
	HANDLE handle = FindFirstFile(name, &finddata);
	if(handle == INVALID_HANDLE_VALUE)
		return 1;

	*created = filetime_to_unixtime(&finddata.ftCreationTime);
	*modified = filetime_to_unixtime(&finddata.ftLastWriteTime);
#elif defined(CONF_FAMILY_UNIX)
	struct stat sb;
	if(stat(name, &sb))
		return 1;

	*created = sb.st_ctime;
	*modified = sb.st_mtime;
#else
	#error not implemented
#endif

	return 0;
}

void swap_endian(void *data, unsigned elem_size, unsigned num)
{
	char *src = (char*) data;
	char *dst = src + (elem_size - 1);

	while(num)
	{
		unsigned n = elem_size>>1;
		char tmp;
		while(n)
		{
			tmp = *src;
			*src = *dst;
			*dst = tmp;

			src++;
			dst--;
			n--;
		}

		src = src + (elem_size>>1);
		dst = src + (elem_size - 1);
		num--;
	}
}

int net_socket_read_wait(NETSOCKET sock, int time)
{
	struct timeval tv;
	fd_set readfds;
	int sockid;

	tv.tv_sec = 0;
	tv.tv_usec = 1000*time;
	sockid = 0;

	FD_ZERO(&readfds);
	if(sock->ipv4sock >= 0)
	{
		FD_SET(sock->ipv4sock, &readfds);
		sockid = sock->ipv4sock;
	}
	if(sock->ipv6sock >= 0)
	{
		FD_SET(sock->ipv6sock, &readfds);
		if(sock->ipv6sock > sockid)
			sockid = sock->ipv6sock;
	}

	/* don't care about writefds and exceptfds */
	select(sockid+1, &readfds, NULL, NULL, &tv);

	if(sock->ipv4sock >= 0 && FD_ISSET(sock->ipv4sock, &readfds))
		return 1;

	if(sock->ipv6sock >= 0 && FD_ISSET(sock->ipv6sock, &readfds))
		return 1;

	return 0;
}

int time_timestamp()
{
	return time(0);
}

int time_houroftheday()
{
	time_t time_data;
	struct tm *time_info;

	time(&time_data);
	time_info = localtime(&time_data);
	return time_info->tm_hour;
}

int time_season()
{
	time_t time_data;
	struct tm *time_info;

	time(&time_data);
	time_info = localtime(&time_data);

	switch(time_info->tm_mon)
	{
		case 11:
		case 0:
		case 1:
			return SEASON_WINTER;
		case 2:
		case 3:
		case 4:
			return SEASON_SPRING;
		case 5:
		case 6:
		case 7:
			return SEASON_SUMMER;
		case 8:
		case 9:
		case 10:
			return SEASON_AUTUMN;
	}
	return SEASON_SPRING; // should never happen
}

int time_isxmasday()
{
	time_t time_data;
	struct tm *time_info;

	time(&time_data);
	time_info = localtime(&time_data);
	if(time_info->tm_mon == 11 && time_info->tm_mday >= 24 && time_info->tm_mday <= 26)
		return 1;
	return 0;
}

int time_iseasterday()
{
	time_t time_data_now, time_data;
	struct tm *time_info;
	int Y, a, b, c, d, e, f, g, h, i, k, L, m, month, day, day_offset;

	time(&time_data_now);
	time_info = localtime(&time_data_now);

	// compute Easter day (Sunday) using https://en.wikipedia.org/w/index.php?title=Computus&oldid=890710285#Anonymous_Gregorian_algorithm
	Y = time_info->tm_year + 1900;
	a = Y % 19;
	b = Y / 100;
	c = Y % 100;
	d = b / 4;
	e = b % 4;
	f = (b + 8) / 25;
	g = (b - f + 1) / 3;
	h = (19 * a + b - d - g + 15) % 30;
	i = c / 4;
	k = c % 4;
	L = (32 + 2 * e + 2 * i - h - k) % 7;
	m = (a + 11 * h + 22 * L) / 451;
	month = (h + L - 7 * m + 114) / 31;
	day = ((h + L - 7 * m + 114) % 31) + 1;

	// (now-1d ≤ easter ≤ now+2d) <=> (easter-2d ≤ now ≤ easter+1d) <=> (Good Friday ≤ now ≤ Easter Monday)
	for(day_offset = -1; day_offset <= 2; day_offset++)
	{
		time_data = time_data_now + day_offset*(60*60*24);
		time_info = localtime(&time_data);

		if(time_info->tm_mon == month-1 && time_info->tm_mday == day)
			return 1;
	}
	return 0;
}

void str_append(char *dst, const char *src, int dst_size)
{
	int s = strlen(dst);
	int i = 0;
	while(s < dst_size)
	{
		dst[s] = src[i];
		if(!src[i]) /* check for null termination */
			break;
		s++;
		i++;
	}

	dst[dst_size-1] = 0; /* assure null termination */
	str_utf8_fix_truncation(dst);
}

void str_copy(char *dst, const char *src, int dst_size)
{
	dst[0] = '\0';
	strncat(dst, src, dst_size - 1);
	str_utf8_fix_truncation(dst);
}

void str_utf8_truncate(char *dst, int dst_size, const char *src, int truncation_len)
{
	int size = -1;
	const char *cursor = src;
	int pos = 0;
	while(pos <= truncation_len && cursor - src < dst_size && size != cursor - src)
	{
		size = cursor - src;
		if(str_utf8_decode(&cursor) == 0)
		{
			break;
		}
		pos++;
	}
	str_copy(dst, src, size + 1);
}

void str_truncate(char *dst, int dst_size, const char *src, int truncation_len)
{
	int size = dst_size;
	if(truncation_len < size)
	{
		size = truncation_len + 1;
	}
	str_copy(dst, src, size);
}

int str_length(const char *str)
{
	return (int)strlen(str);
}

int str_format_v(char* buffer, int buffer_size, const char* format, va_list args)
{
#if defined(CONF_FAMILY_WINDOWS)
	_vsprintf_p(buffer, buffer_size, format, args);
	buffer[buffer_size - 1] = 0; /* assure null termination */
#else
	vsnprintf(buffer, buffer_size, format, args);
	/* null termination is assured by definition of vsnprintf */
#endif
	return str_utf8_fix_truncation(buffer);
}

int str_format(char* buffer, int buffer_size, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int length = str_format_v(buffer, buffer_size, format, args);
	va_end(args);
	return length;
}

FILE *pipe_open(const char *cmd, const char *mode)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _popen(cmd, mode);
#else
	return popen(cmd, mode);
#endif
}

int pipe_close(FILE *stream)
{
#if defined(CONF_FAMILY_WINDOWS)
	return _pclose(stream);
#else
	return pclose(stream);
#endif
}

/* makes sure that the string only contains the characters between 32 and 127 */
void str_sanitize_strong(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		*str &= 0x7f;
		if(*str < 32)
			*str = 32;
		str++;
	}
}

/* makes sure that the string only contains the characters between 32 and 255 */
void str_sanitize_cc(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32)
			*str = ' ';
		str++;
	}
}

/* check if the string contains '..' (parent directory) paths */
int str_path_unsafe(const char *str)
{
	// State machine. 0 means that we're at the beginning
	// of a new directory/filename, and a positive number represents the number of
	// dots ('.') we found. -1 means we encountered a different character
	// since the last path separator (or the beginning of the string).
	int parse_counter = 0;
	while(*str)
	{
		if(*str == '\\' || *str == '/')
		{
			// A path separator. Check how many dots we found since
			// the last path separator.
			//
			// Two dots => ".." contained in the path. Return an
			// error.
			if(parse_counter == 2)
				return -1;
			else
				parse_counter = 0;
		}
		else if(parse_counter >= 0)
		{
			// If we have not encountered a non-dot character since
			// the last path separator, count the dots.
			if(*str == '.')
				parse_counter++;
			else
				parse_counter = -1;
		}

		++str;
	}
	// If there's a ".." at the end, fail too.
	if(parse_counter == 2)
		return -1;
	return 0;
}

/* makes sure that the string only contains the characters between 32 and 255 + \r\n\t */
void str_sanitize(char *str_in)
{
	unsigned char *str = (unsigned char *)str_in;
	while(*str)
	{
		if(*str < 32 && !(*str == '\r') && !(*str == '\n') && !(*str == '\t'))
			*str = ' ';
		str++;
	}
}

/* removes all forbidden windows/unix characters in filenames*/
char* str_sanitize_filename(char* aName)
{
	char *str = (char *)aName;
	while(*str)
	{
		// replace forbidden characters with a whispace
		if(*str == '/' || *str == '<' || *str == '>' || *str == ':' || *str == '"'
			|| *str == '/' || *str == '\\' || *str == '|' || *str == '?' || *str == '*')
 			*str = ' ';
		str++;
	}
	str_clean_whitespaces(aName);
	return aName;
}

/* removes leading and trailing spaces and limits the use of multiple spaces */
void str_clean_whitespaces(char *str_in)
{
	char *read = str_in;
	char *write = str_in;

	/* skip initial whitespace */
	while(*read == ' ')
		read++;

	/* end of read string is detected in the loop */
	while(1)
	{
		/* skip whitespace */
		int found_whitespace = 0;
		for(; *read == ' '; read++)
			found_whitespace = 1;
		/* if not at the end of the string, put a found whitespace here */
		if(*read)
		{
			if(found_whitespace)
				*write++ = ' ';
			*write++ = *read++;
		}
		else
		{
			*write = 0;
			break;
		}
	}
}

/* removes leading and trailing spaces */
void str_clean_whitespaces_simple(char *str_in)
{
	char *read = str_in;
	char *write = str_in;

	/* skip initial whitespace */
	while(*read == ' ')
		read++;

	/* end of read string is detected in the loop */
	while(1)
	{
		/* skip whitespace */
		int found_whitespace = 0;
		for(; *read == ' ' && !found_whitespace; read++)
			found_whitespace = 1;
		/* if not at the end of the string, put a found whitespace here */
		if(*read)
		{
			if(found_whitespace)
				*write++ = ' ';
			*write++ = *read++;
		}
		else
		{
			*write = 0;
			break;
		}
	}
}

char *str_skip_to_whitespace(char *str)
{
	while(*str && (*str != ' ' && *str != '\t' && *str != '\n'))
		str++;
	return str;
}

const char *str_skip_to_whitespace_const(const char *str)
{
	while(*str && (*str != ' ' && *str != '\t' && *str != '\n'))
		str++;
	return str;
}

char *str_skip_whitespaces(char *str)
{
	while(*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r'))
		str++;
	return str;
}

const char *str_skip_whitespaces_const(const char *str)
{
	while(*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r'))
		str++;
	return str;
}

/* case */
int str_comp_nocase(const char *a, const char *b)
{
#if defined(CONF_FAMILY_WINDOWS) && !defined(__GNUC__)
	return _stricmp(a,b);
#else
	return strcasecmp(a,b);
#endif
}

int str_comp_nocase_num(const char *a, const char *b, const int num)
{
#if defined(CONF_FAMILY_WINDOWS) && !defined(__GNUC__)
	return _strnicmp(a, b, num);
#else
	return strncasecmp(a, b, num);
#endif
}

int str_comp(const char *a, const char *b)
{
	return strcmp(a, b);
}

int str_comp_num(const char *a, const char *b, const int num)
{
	return strncmp(a, b, num);
}

int str_comp_filenames(const char *a, const char *b)
{
	int result;

	for(; *a && *b; ++a, ++b)
	{
		if(*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9')
		{
			result = 0;
			do
			{
				if(!result)
					result = *a - *b;
				++a; ++b;
			}
			while(*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9');

			if(*a >= '0' && *a <= '9')
				return 1;
			else if(*b >= '0' && *b <= '9')
				return -1;
			else if(result)
				return result;
		}

		if(tolower(*a) != tolower(*b))
			break;
	}
	return tolower(*a) - tolower(*b);
}

const char *str_startswith_nocase(const char *str, const char *prefix)
{
	int prefixl = str_length(prefix);
	if(str_comp_nocase_num(str, prefix, prefixl) == 0)
	{
		return str + prefixl;
	}
	else
	{
		return 0;
	}
}

const char *str_startswith(const char *str, const char *prefix)
{
	int prefixl = str_length(prefix);
	if(str_comp_num(str, prefix, prefixl) == 0)
	{
		return str + prefixl;
	}
	else
	{
		return 0;
	}
}

const char *str_endswith_nocase(const char *str, const char *suffix)
{
	int strl = str_length(str);
	int suffixl = str_length(suffix);
	const char *strsuffix;
	if(strl < suffixl)
	{
		return 0;
	}
	strsuffix = str + strl - suffixl;
	if(str_comp_nocase(strsuffix, suffix) == 0)
	{
		return strsuffix;
	}
	else
	{
		return 0;
	}
}

const char *str_endswith(const char *str, const char *suffix)
{
	int strl = str_length(str);
	int suffixl = str_length(suffix);
	const char *strsuffix;
	if(strl < suffixl)
	{
		return 0;
	}
	strsuffix = str + strl - suffixl;
	if(str_comp(strsuffix, suffix) == 0)
	{
		return strsuffix;
	}
	else
	{
		return 0;
	}
}

const char *str_find_nocase(const char *haystack, const char *needle)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		while(*a && *b && tolower(*a) == tolower(*b))
		{
			a++;
			b++;
		}
		if(!(*b))
			return haystack;
		haystack++;
	}

	return 0;
}


const char *str_find(const char *haystack, const char *needle)
{
	while(*haystack) /* native implementation */
	{
		const char *a = haystack;
		const char *b = needle;
		while(*a && *b && *a == *b)
		{
			a++;
			b++;
		}
		if(!(*b))
			return haystack;
		haystack++;
	}

	return 0;
}

void str_hex(char *dst, int dst_size, const void *data, int data_size)
{
	static const char hex[] = "0123456789ABCDEF";
	int b;

	for(b = 0; b < data_size && b < dst_size/4-4; b++)
	{
		dst[b*3] = hex[((const unsigned char *)data)[b]>>4];
		dst[b*3+1] = hex[((const unsigned char *)data)[b]&0xf];
		dst[b*3+2] = ' ';
		dst[b*3+3] = 0;
	}
}

static int hexval(char x)
{
    switch(x)
    {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a':
    case 'A': return 10;
    case 'b':
    case 'B': return 11;
    case 'c':
    case 'C': return 12;
    case 'd':
    case 'D': return 13;
    case 'e':
    case 'E': return 14;
    case 'f':
    case 'F': return 15;
    default: return -1;
    }
}

static int byteval(const char *byte, unsigned char *dst)
{
	int v1 = -1, v2 = -1;
	v1 = hexval(byte[0]);
	v2 = hexval(byte[1]);

	if(v1 < 0 || v2 < 0)
		return 1;

	*dst = v1 * 16 + v2;
	return 0;
}

int str_hex_decode(void *dst, int dst_size, const char *src)
{
	unsigned char *cdst = (unsigned char *)dst;
	int slen = str_length(src);
	int len = slen / 2;
	int i;
	if(slen != dst_size * 2)
		return 2;

	for(i = 0; i < len && dst_size; i++, dst_size--)
	{
		if(byteval(src + i * 2, cdst++))
			return 1;
	}
	return 0;
}

bool str_isnum(char c)
{
	return c >= '0' && c <= '9';
}

int str_is_number(const char *str)
{
	while(*str)
	{
		if(!(*str >= '0' && *str <= '9'))
			return -1;
		str++;
	}
	return 0;
}
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
void str_timestamp_ex(time_t time_data, char *buffer, int buffer_size, const char *format)
{
	struct tm *time_info;
	time_info = localtime(&time_data);
	strftime(buffer, buffer_size, format, time_info);
	buffer[buffer_size-1] = 0;	/* assure null termination */
}

void str_timestamp_format(char *buffer, int buffer_size, const char *format)
{
	time_t time_data;
	time(&time_data);
	str_timestamp_ex(time_data, buffer, buffer_size, format);
}

void str_timestamp(char *buffer, int buffer_size)
{
	str_timestamp_format(buffer, buffer_size, FORMAT_NOSPACE);
}

int str_format_args(char *pBuffer, int BufferSize, const char *pFormat, CFormatArg *pArgs, int NumArgs)
{
	// Input parameter validation
	if (!pBuffer || BufferSize <= 0 || !pFormat)
		return -1;

	// If there are no arguments, simply copy the format string
	if (!pArgs || NumArgs <= 0)
	{
		str_copy(pBuffer, pFormat, BufferSize);
		return str_length(pBuffer);
	}

	char *pOut = pBuffer;
	const char *pIn = pFormat;
	int ArgIndex = 0;
	int RemainingSize = BufferSize - 1; // Reserve 1 byte for the null terminator
	
	while (*pIn && RemainingSize > 0)
	{
		// Process formatting options
		if (*pIn == '%' && *(pIn + 1) != '\0')
		{
			pIn++; // Skip '%' character
			
			// Handle escaped percent signs (%%)
			if (*pIn == '%')
			{
				*pOut++ = '%';
				pIn++;
				RemainingSize--;
				continue;
			}
			
			// Check if there are still arguments available
			if (ArgIndex < NumArgs)
			{
				char aTemp[512] = {0}; // Larger buffer for temporary results
				char aFormat[64] = {0}; // Larger and initialized format string buffer
				
				aFormat[0] = '%'; // Format string always begins with '%'
				int FormatLen = 1;
				
				// Safe function to append to the format string
				#define APPEND_FORMAT(ch) do { \
					if (FormatLen < (int)sizeof(aFormat) - 2) { \
						aFormat[FormatLen++] = (ch); \
						aFormat[FormatLen] = '\0'; \
					} \
				} while (0)
				
				// Parse width, flags, and precision for all types
				while (*pIn && (*pIn == '+' || *pIn == '-' || *pIn == ' ' || 
						*pIn == '#' || *pIn == '0' || isdigit(*pIn) || *pIn == '.')) {
					APPEND_FORMAT(*pIn);
					pIn++;
				}
				
				// Helper macro for valid format specifiers
				#define HANDLE_SPECIFIER(valid_chars, default_char) do { \
					if (*pIn && strchr(valid_chars, *pIn)) { \
						APPEND_FORMAT(*pIn); \
						pIn++; \
					} else { \
						APPEND_FORMAT(default_char); \
					} \
				} while (0)
				
				// Process format based on argument type
				switch (pArgs[ArgIndex].m_Type)
				{
					case CFormatArg::FORMAT_INT:
						HANDLE_SPECIFIER("dioxXu", 'd');
						str_format(aTemp, sizeof(aTemp), aFormat, pArgs[ArgIndex].m_Int);
						break;
					
					case CFormatArg::FORMAT_STRING:
						HANDLE_SPECIFIER("s", 's');
						str_format(aTemp, sizeof(aTemp), aFormat, pArgs[ArgIndex].m_pStr ? pArgs[ArgIndex].m_pStr : "(null)");
						break;
					
					case CFormatArg::FORMAT_INT64:
						// Check if 'll' is already in the format
						if (*pIn == 'l' && *(pIn + 1) == 'l') {
							APPEND_FORMAT('l');
							APPEND_FORMAT('l');
							pIn += 2;
						} else {
							// Add 'll' modifier if not already present
							APPEND_FORMAT('l');
							APPEND_FORMAT('l');
						}
						
						HANDLE_SPECIFIER("dioxXu", 'd');
						str_format(aTemp, sizeof(aTemp), aFormat, pArgs[ArgIndex].m_Int64);
						break;
					
					case CFormatArg::FORMAT_FLOAT:
						HANDLE_SPECIFIER("fFeEgGaA", 'f');
						str_format(aTemp, sizeof(aTemp), aFormat, pArgs[ArgIndex].m_Float);
						break;
					
					default:
						str_copy(aTemp, "[invalid type]", sizeof(aTemp));
						// Skip remaining format specifiers
						while (*pIn && !isspace(*pIn) && *pIn != '%')
							pIn++;
						break;
				}
				
				#undef APPEND_FORMAT
				#undef HANDLE_SPECIFIER
				
				// Copy the formatted result to the output buffer
				int TempLen = str_length(aTemp);
				int CopyLen = TempLen < RemainingSize ? TempLen : RemainingSize;
				
				if (CopyLen > 0)
				{
					memcpy(pOut, aTemp, CopyLen);
					pOut += CopyLen;
					RemainingSize -= CopyLen;
				}
				
				ArgIndex++;
			}
			else
			{
				// No argument available, simply copy the format specifier
				*pOut++ = '%';
				RemainingSize--;
				if (RemainingSize > 0 && *pIn)
				{
					*pOut++ = *pIn++;
					RemainingSize--;
				}
			}
		}
		else
		{
			// Copy normal characters
			*pOut++ = *pIn++;
			RemainingSize--;
		}
	}
	
	// Ensure the string is null-terminated
	*pOut = '\0';
	
	return (int)(pOut - pBuffer);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

int str_span(const char *str, const char *set)
{
	return strcspn(str, set);
}

int mem_comp(const void *a, const void *b, size_t size)
{
	return memcmp(a,b,size);
}

int mem_has_null(const void *block, size_t size)
{
	const unsigned char *bytes = (const unsigned char *)block;
	unsigned i;        
	for(i = 0; i < size; i++)
	{
		if(bytes[i] == 0)
		{
			return 1;
		}
	}
	return 0;
}

void net_stats(NETSTATS *stats_inout)
{
	*stats_inout = network_stats;
}

int str_isspace(char c) { return c == ' ' || c == '\n' || c == '\t'; }

char str_uppercase(char c)
{
	if(c >= 'a' && c <= 'z')
		return 'A' + (c-'a');
	return c;
}

int str_toint(const char *str) { return atoi(str); }
float str_tofloat(const char *str) { return atof(str); }

int str_utf8_comp_nocase(const char* a, const char* b)
{
	int code_a;
	int code_b;

	while (*a && *b)
	{
		code_a = str_utf8_tolower(str_utf8_decode(&a));
		code_b = str_utf8_tolower(str_utf8_decode(&b));

		if (code_a != code_b)
			return code_a - code_b;
	}
	return (unsigned char)* a - (unsigned char)* b;
}

int str_utf8_comp_nocase_num(const char* a, const char* b, int num)
{
	int code_a;
	int code_b;
	const char* old_a = a;

	if (num <= 0)
		return 0;

	while (*a && *b)
	{
		code_a = str_utf8_tolower(str_utf8_decode(&a));
		code_b = str_utf8_tolower(str_utf8_decode(&b));

		if (code_a != code_b)
			return code_a - code_b;

		if (a - old_a >= num)
			return 0;
	}

	return (unsigned char)* a - (unsigned char)* b;
}

const char* str_utf8_find_nocase(const char* haystack, const char* needle)
{
	while (*haystack) /* native implementation */
	{
		const char* a = haystack;
		const char* b = needle;
		const char* a_next = a;
		const char* b_next = b;
		while (*a && *b && str_utf8_tolower(str_utf8_decode(&a_next)) == str_utf8_tolower(str_utf8_decode(&b_next)))
		{
			a = a_next;
			b = b_next;
		}
		if (!(*b))
			return haystack;
		str_utf8_decode(&haystack);
	}

	return 0;
}

int str_utf8_isspace(int code)
{
	return code <= 0x0020 || code == 0x0085 || code == 0x00A0 || code == 0x034F ||
	       code == 0x115F || code == 0x1160 || code == 0x1680 || code == 0x180E ||
	       (code >= 0x2000 && code <= 0x200F) || (code >= 0x2028 && code <= 0x202F) ||
	       (code >= 0x205F && code <= 0x2064) || (code >= 0x206A && code <= 0x206F) ||
	       code == 0x2800 || code == 0x3000 || code == 0x3164 ||
	       (code >= 0xFE00 && code <= 0xFE0F) || code == 0xFEFF || code == 0xFFA0 ||
	       (code >= 0xFFF9 && code <= 0xFFFC);
}

int str_utf8_is_whitespace(int code)
{
	// check if unicode is not empty
	if(code > 0x20 && code != 0xA0 && code != 0x034F && (code < 0x2000 || code > 0x200F) && (code < 0x2028 || code > 0x202F) &&
		(code < 0x205F || code > 0x2064) && (code < 0x206A || code > 0x206F) && code != 0x3000  && (code < 0xFE00 || code > 0xFE0F) &&
		code != 0xFEFF && (code < 0xFFF9 || code > 0xFFFC))
	{
		return 0;
	}
	return 1;
}

const char *str_utf8_skip_whitespaces(const char *str)
{
	const char *str_old;
	int code;

	while(*str)
	{
		str_old = str;
		code = str_utf8_decode(&str);

		if(!str_utf8_is_whitespace(code))
		{
			return str_old;
		}
	}

	return str;
}

void str_utf8_trim_right(char* param)
{
	const char* str = param;
	char* end = 0;
	while (*str)
	{
		char* str_old = (char*)str;
		int code = str_utf8_decode(&str);

		// check if unicode is not empty
		if (!str_utf8_isspace(code))
		{
			end = 0;
		}
		else if (!end)
		{
			end = str_old;
		}
	}
	if (end)
	{
		*end = 0;
	}
}

void str_utf8_trim_whitespaces_right(char *str)
{
	int cursor = str_length(str);
	const char *last = str + cursor;
	while(str_utf8_is_whitespace(str_utf8_decode(&last)))
	{
		str[cursor] = 0;
		cursor = str_utf8_rewind(str, cursor);
		last = str + cursor;
		if(cursor == 0)
		{
			break;
		}
	}
}

int str_utf8_isstart(char c)
{
	if((c&0xC0) == 0x80) /* 10xxxxxx */
		return 0;
	return 1;
}

int str_utf8_rewind(const char *str, int cursor)
{
	while(cursor)
	{
		cursor--;
		if(str_utf8_isstart(*(str + cursor)))
			break;
	}
	return cursor;
}

int str_utf8_fix_truncation(char *str)
{
	int len = str_length(str);
	if(len > 0)
	{
		int last_char_index = str_utf8_rewind(str, len);
		const char *last_char = str + last_char_index;
		// Fix truncated UTF-8.
		if(str_utf8_decode(&last_char) == -1)
		{
			str[last_char_index] = 0;
			return last_char_index;
		}
	}
	return len;
}

int str_utf8_forward(const char *str, int cursor)
{
	const char *ptr = str + cursor;
	if(str_utf8_decode(&ptr) == 0)
	{
		return cursor;
	}
	return ptr - str;
}

int str_utf8_encode(char *ptr, int chr)
{
	/* encode */
	if(chr <= 0x7F)
	{
		ptr[0] = (char)chr;
		return 1;
	}
	else if(chr <= 0x7FF)
	{
		ptr[0] = 0xC0|((chr>>6)&0x1F);
		ptr[1] = 0x80|(chr&0x3F);
		return 2;
	}
	else if(chr <= 0xFFFF)
	{
		ptr[0] = 0xE0|((chr>>12)&0x0F);
		ptr[1] = 0x80|((chr>>6)&0x3F);
		ptr[2] = 0x80|(chr&0x3F);
		return 3;
	}
	else if(chr <= 0x10FFFF)
	{
		ptr[0] = 0xF0|((chr>>18)&0x07);
		ptr[1] = 0x80|((chr>>12)&0x3F);
		ptr[2] = 0x80|((chr>>6)&0x3F);
		ptr[3] = 0x80|(chr&0x3F);
		return 4;
	}

	return 0;
}

int str_utf8_decode(const char **ptr)
{
	const char *buf = *ptr;
	int ch = 0;

	do
	{
		if((*buf&0x80) == 0x0)  /* 0xxxxxxx */
		{
			ch = *buf;
			buf++;
		}
		else if((*buf&0xE0) == 0xC0) /* 110xxxxx */
		{
			ch  = (*buf++ & 0x3F) << 6; if(!(*buf) || (*buf&0xC0) != 0x80) break;
			ch += (*buf++ & 0x3F);
			if(ch < 0x80 || ch > 0x7FF) ch = -1;
		}
		else  if((*buf & 0xF0) == 0xE0)	/* 1110xxxx */
		{
			ch  = (*buf++ & 0x1F) << 12; if(!(*buf) || (*buf&0xC0) != 0x80) break;
			ch += (*buf++ & 0x3F) <<  6; if(!(*buf) || (*buf&0xC0) != 0x80) break;
			ch += (*buf++ & 0x3F);
			if(ch < 0x800 || ch > 0xFFFF) ch = -1;
		}
		else if((*buf & 0xF8) == 0xF0)	/* 11110xxx */
		{
			ch  = (*buf++ & 0x0F) << 18; if(!(*buf) || (*buf&0xC0) != 0x80) break;
			ch += (*buf++ & 0x3F) << 12; if(!(*buf) || (*buf&0xC0) != 0x80) break;
			ch += (*buf++ & 0x3F) <<  6; if(!(*buf) || (*buf&0xC0) != 0x80) break;
			ch += (*buf++ & 0x3F);
			if(ch < 0x10000 || ch > 0x10FFFF) ch = -1;
		}
		else
		{
			/* invalid */
			buf++;
			break;
		}

		*ptr = buf;
		return ch;
	} while(0);

	/* out of bounds */
	*ptr = buf;
	return -1;

}

const char *str_skip_voting_menu_prefixes(const char *pStr)
{
	if (!pStr || !pStr[0])
		return 0;

	const char *pPrefixes[] = { "•", "☒", "☐", "│", "╭", "─", ">", "⇨" };
	const char *pTemp = pStr;
	while (1)
	{
		bool Break = true;
		for (unsigned int p = 0; p < sizeof(pPrefixes)/sizeof(pPrefixes[0]); p++)
		{
			const char *pPrefix = str_utf8_find_nocase(pTemp, pPrefixes[p]);
			if (pPrefix)
			{
				int NewCursor = str_utf8_forward(pPrefix, 0);
				if (NewCursor != 0)
				{
					pTemp = pPrefix + NewCursor;
					Break = false;
					break;
				}
			}
		}
		if (Break)
			break;
	}
	return str_skip_whitespaces_const(pTemp);
}

int str_check_special_chars(const char *pStr)
{
	while(*pStr)
	{
		if (!((*pStr >= 'a' && *pStr <= 'z') || (*pStr >= 'A' && *pStr <= 'Z') || (*pStr >= '0' && *pStr <= '9')))
			return 0;
		pStr++;
	}
	return 1;
}

int str_utf8_check(const char *str)
{
	while(*str)
	{
		if((*str&0x80) == 0x0)
			str++;
		else if((*str&0xE0) == 0xC0 && (*(str+1)&0xC0) == 0x80)
			str += 2;
		else if((*str&0xF0) == 0xE0 && (*(str+1)&0xC0) == 0x80 && (*(str+2)&0xC0) == 0x80)
			str += 3;
		else if((*str&0xF8) == 0xF0 && (*(str+1)&0xC0) == 0x80 && (*(str+2)&0xC0) == 0x80 && (*(str+3)&0xC0) == 0x80)
			str += 4;
		else
			return 0;
	}
	return 1;
}

void str_utf8_copy_num(char *dst, const char *src, int dst_size, int num)
{
	int new_cursor;
	int cursor = 0;

	while(src[cursor] && num > 0)
	{
		new_cursor = str_utf8_forward(src, cursor);
		if(new_cursor >= dst_size)			// reserve 1 byte for the null termination
			break;
		else
			cursor = new_cursor;
		--num;
	}

	str_copy(dst, src, cursor < dst_size ? cursor+1 : dst_size);
}

unsigned str_quickhash(const char *str)
{
	unsigned hash = 5381;
	for(; *str; str++)
		hash = ((hash << 5) + hash) + (*str); /* hash * 33 + c */
	return hash;
}

unsigned str_quickhash_raw(const char *str)
{
	unsigned hash = 5381;
	for(; *str; str++)
	{
		if(*str == '\n')
		{
			hash = ((hash << 5) + hash) + '\\';
			hash = ((hash << 5) + hash) + 'n';
		}
		else
		{
			hash = ((hash << 5) + hash) + (*str); /* hash * 33 + c */
		}
	}
	return hash;
}

uint32_t fnv1a(const char *str)
{
	uint32_t hash = 2166136261u;
    while (*str)
    {
        hash ^= (uint8_t)(*str++);
        hash *= 16777619u;
    }
    return hash;
}

static const char *str_token_get(const char *str, const char *delim, int *length)
{
	size_t len = strspn(str, delim);
	if(len > 1)
		str++;
	else
		str += len;
	if(!*str)
		return NULL;

	*length = strcspn(str, delim);
	return str;
}

int str_in_list(const char *list, const char *delim, const char *needle)
{
	const char *tok = list;
	int len = 0, notfound = 1, needlelen = str_length(needle);

	while(notfound && (tok = str_token_get(tok, delim, &len)))
	{
		notfound = needlelen != len || str_comp_num(tok, needle, len);
		tok = tok + len;
	}

	return !notfound;
}

const char *str_next_token(const char *str, const char *delim, char *buffer, int buffer_size)
{
	int len = 0;
	const char *tok = str_token_get(str, delim, &len);
	if(len < 0)
		return NULL;

	len = buffer_size > len ? len : buffer_size - 1;
	mem_copy(buffer, tok, len);
	buffer[len] = '\0';

	return tok + len;
}

#if defined(CONF_FAMILY_WINDOWS)
std::optional<std::string> windows_wide_to_utf8(const wchar_t *wide_str)
{
	const int orig_length = wcslen(wide_str);
	if(orig_length == 0)
		return "";
	const int size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_str, orig_length, nullptr, 0, nullptr, nullptr);
	if(size_needed == 0)
		return {};
	std::string string(size_needed, '\0');
	dbg_assert(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_str, orig_length, string.data(), size_needed, nullptr, nullptr) == size_needed, "WideCharToMultiByte failure");
	return string;
}
#endif

void os_locale_str(char *locale, size_t length)
{
#if defined(CONF_FAMILY_WINDOWS)
	wchar_t wide_buffer[LOCALE_NAME_MAX_LENGTH];
	dbg_assert(GetUserDefaultLocaleName(wide_buffer, std::size(wide_buffer)) > 0, "GetUserDefaultLocaleName failure");

	const std::optional<std::string> buffer = windows_wide_to_utf8(wide_buffer);
	dbg_assert(buffer.has_value(), "GetUserDefaultLocaleName returned invalid UTF-16");
	str_copy(locale, buffer.value().c_str(), length);
#elif defined(CONF_PLATFORM_MACOS)
	CFLocaleRef locale_ref = CFLocaleCopyCurrent();
	CFStringRef locale_identifier_ref = static_cast<CFStringRef>(CFLocaleGetValue(locale_ref, kCFLocaleIdentifier));

	// Count number of UTF16 codepoints, +1 for zero-termination.
	// Assume maximum possible length for encoding as UTF-8.
	CFIndex locale_identifier_size = (UTF8_BYTE_LENGTH * CFStringGetLength(locale_identifier_ref) + 1) * sizeof(char);
	char *locale_identifier = (char *)malloc(locale_identifier_size);
	dbg_assert(CFStringGetCString(locale_identifier_ref, locale_identifier, locale_identifier_size, kCFStringEncodingUTF8), "CFStringGetCString failure");

	str_copy(locale, locale_identifier, length);

	free(locale_identifier);
	CFRelease(locale_ref);
#else
	static const char *ENV_VARIABLES[] = {
		"LC_ALL",
		"LC_MESSAGES",
		"LANG",
	};

	locale[0] = '\0';
	for(const char *env_variable : ENV_VARIABLES)
	{
		const char *env_value = getenv(env_variable);
		if(env_value)
		{
			str_copy(locale, env_value, length);
			break;
		}
	}
#endif

	// Ensure RFC 3066 format:
	// - use hyphens instead of underscores
	// - truncate locale string after first non-standard letter
	for(int i = 0; i < str_length(locale); ++i)
	{
		if(locale[i] == '_')
		{
			locale[i] = '-';
		}
		else if(locale[i] != '-' && !(locale[i] >= 'a' && locale[i] <= 'z') && !(locale[i] >= 'A' && locale[i] <= 'Z') && !(str_isnum(locale[i])))
		{
			locale[i] = '\0';
			break;
		}
	}

	// Use default if we could not determine the locale,
	// i.e. if only the C or POSIX locale is available.
	if(locale[0] == '\0' || str_comp(locale, "C") == 0 || str_comp(locale, "POSIX") == 0)
		str_copy(locale, "en-US", length);
}

struct SECURE_RANDOM_DATA
{
	int initialized;
#if defined(CONF_FAMILY_WINDOWS)
	HCRYPTPROV provider;
#else
	IOHANDLE urandom;
#endif
};

static struct SECURE_RANDOM_DATA secure_random_data = { 0 };

int secure_random_init()
{
	if(secure_random_data.initialized)
	{
		return 0;
	}
#if defined(CONF_FAMILY_WINDOWS)
	if(CryptAcquireContext(&secure_random_data.provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		secure_random_data.initialized = 1;
		return 0;
	}
	else
	{
		return 1;
	}
#else
	secure_random_data.urandom = io_open("/dev/urandom", IOFLAG_READ);
	if(secure_random_data.urandom)
	{
		secure_random_data.initialized = 1;
		return 0;
	}
	else
	{
		return 1;
	}
#endif
}

void secure_random_fill(void *bytes, unsigned length)
{
	if(!secure_random_data.initialized)
	{
		dbg_msg("secure", "called secure_random_fill before secure_random_init");
		dbg_break();
	}
#if defined(CONF_FAMILY_WINDOWS)
	if(!CryptGenRandom(secure_random_data.provider, length, (unsigned char *)bytes))
	{
		dbg_msg("secure", "CryptGenRandom failed, last_error=%lu", GetLastError());
		dbg_break();
	}
#else
	if(length != io_read(secure_random_data.urandom, bytes, length))
	{
		dbg_msg("secure", "io_read returned with a short read");
		dbg_break();
	}
#endif
}

void generate_password(char *buffer, unsigned length, unsigned short *random, unsigned random_length)
{
	static const char VALUES[] = "ABCDEFGHKLMNPRSTUVWXYZabcdefghjkmnopqt23456789";
	static const size_t NUM_VALUES = sizeof(VALUES) - 1; // Disregard the '\0'.
	unsigned i;
	dbg_assert(length >= random_length * 2 + 1, "too small buffer");
	dbg_assert(NUM_VALUES * NUM_VALUES >= 2048, "need at least 2048 possibilities for 2-character sequences");

	buffer[random_length * 2] = 0;

	for(i = 0; i < random_length; i++)
	{
		unsigned short random_number = random[i] % 2048;
		buffer[2 * i + 0] = VALUES[random_number / NUM_VALUES];
		buffer[2 * i + 1] = VALUES[random_number % NUM_VALUES];
	}
}

#define MAX_PASSWORD_LENGTH 128

void secure_random_password(char *buffer, unsigned length, unsigned pw_length)
{
	unsigned short random[MAX_PASSWORD_LENGTH / 2];
	// With 6 characters, we get a password entropy of log(2048) * 6/2 = 33bit.
	dbg_assert(length >= pw_length + 1, "too small buffer");
	dbg_assert(pw_length >= 6, "too small password length");
	dbg_assert(pw_length % 2 == 0, "need an even password length");
	dbg_assert(pw_length <= MAX_PASSWORD_LENGTH, "too large password length");

	secure_random_fill(random, pw_length);

	generate_password(buffer, length, random, pw_length / 2);
}

#undef MAX_PASSWORD_LENGTH

int pid()
{
#if defined(CONF_FAMILY_WINDOWS)
	return _getpid();
#else
	return getpid();
#endif
}

unsigned bytes_be_to_uint(const unsigned char *bytes)
{
	return (bytes[0]<<24) | (bytes[1]<<16) | (bytes[2]<<8) | bytes[3];
}

void uint_to_bytes_be(unsigned char *bytes, unsigned value)
{
	bytes[0] = (value>>24)&0xff;
	bytes[1] = (value>>16)&0xff;
	bytes[2] = (value>>8)&0xff;
	bytes[3] = value&0xff;
}
