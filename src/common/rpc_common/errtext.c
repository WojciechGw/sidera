/*
 * errtext.c -- an errno as a few words (util.h).
 */
#include <errno.h>
#include "util.h"

const char *err_text(int e)
{
    switch (e)
    {
    case ENOENT:
        return "not found";
    case EACCES:
        return "access denied";
    case EEXIST:
        return "already exists";
    case EBUSY:
        return "busy";
    case ENODEV:
        return "no such drive";
    case ENOSPC:
        return "disk full";
    case EINVAL:
        return "invalid name";
    case EMFILE:
        return "too many open files";
    case EIO:
        return "read or write failed";
    case EAGAIN:
        return "the network failed"; /* rpcw's osnet.c */
    case ENOSYS:
        return "no /edit there"; /* NX_EDIT, osnet.c */
    }
    return "error";
}
