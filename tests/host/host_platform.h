#ifndef SIDERA_HOST_PLATFORM_H
#define SIDERA_HOST_PLATFORM_H
#ifdef _WIN32
#include <stdlib.h>
#include <io.h>
#include <direct.h>
#include <crtdbg.h>
static void host_assert_stderr(void)
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
}
#else
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
/* Capture native I/O before rpmt.h instruments read/lseek in app sources. */
static inline int host_native_read(int fd, void *p, unsigned n) { return (int)read(fd, p, n); }
static inline int host_native_write(int fd, const void *p, unsigned n) { return (int)write(fd, p, n); }
static inline long host_native_lseek(int fd, long pos, int origin) { return (long)lseek(fd, pos, origin); }
static inline void host_assert_stderr(void) {}
#define _read host_native_read
#define _write host_native_write
#define _lseek host_native_lseek
#define _open open
#define _close close
#define _access access
#define _mkdir(path) mkdir(path, 0700)
#define _chdir chdir
#define _getcwd getcwd
#define _rmdir rmdir
#define _mkgmtime timegm
#define _O_BINARY 0
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif
#endif
