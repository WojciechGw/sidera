#ifndef SIDERA_HOST_PLATFORM_H
#define SIDERA_HOST_PLATFORM_H
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
#endif
