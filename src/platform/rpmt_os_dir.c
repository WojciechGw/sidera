/*
 * rpmt_os_dir.c -- cc65's RIA calls for directories, each under the OS
 * lock (see rpmt.h). Files are rpmt_os_file.c: apart, so that a program
 * linked against rpmt.lib takes open() and close() only if it opens files.
 */
#include <unistd.h>
#include "rpmt.h"

int __fastcall__ rpmt_opendir(const char *name)
{
    int r;
    rpmt_os_lock();
    r = f_opendir(name);
    rpmt_os_unlock();
    return r;
}

int __fastcall__ rpmt_readdir(f_stat_t *dirent, int dirdes)
{
    int r;
    rpmt_os_lock();
    r = f_readdir(dirent, dirdes);
    rpmt_os_unlock();
    return r;
}

int __fastcall__ rpmt_closedir(int dirdes)
{
    int r;
    rpmt_os_lock();
    r = f_closedir(dirdes);
    rpmt_os_unlock();
    return r;
}

int __fastcall__ rpmt_chdir(const char *name)
{
    int r;
    rpmt_os_lock();
    r = chdir(name);
    rpmt_os_unlock();
    return r;
}

int __fastcall__ rpmt_getcwd(char *name, int size)
{
    int r;
    rpmt_os_lock();
    r = getcwd(name, size) ? 0 : -1;
    rpmt_os_unlock();
    return r;
}
