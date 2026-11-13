/*
 * rpmt_os_file.c -- open() and close() under the OS lock (see rpmt.h and
 * rpmt_os_dir.c).
 */
#include <fcntl.h>
#include <unistd.h>
#include "rpmt.h"

int __fastcall__ rpmt_open(const char *name, int flags)
{
    int r;
    rpmt_os_lock();
    r = open(name, flags);
    rpmt_os_unlock();
    return r;
}

int __fastcall__ rpmt_close(int fd)
{
    int r;
    rpmt_os_lock();
    r = close(fd);
    rpmt_os_unlock();
    return r;
}
