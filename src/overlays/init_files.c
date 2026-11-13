#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../sidera.h"
#include "overlay.h"
#define buffer ((char *)OVL_SCRATCH)
#define CONFIG_BYTES 224u
extern int asset_fd;
#pragma code-name ("INIT_CODE")
#pragma rodata-name ("INIT_RODATA")
#include "init_files.inc"
