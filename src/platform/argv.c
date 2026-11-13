/* cc65 mainargs asks for storage while the monitor's argv is on XSTACK.
 * Do not make RIA calls here: they would overwrite the pending arguments. */
#include <stddef.h>
#define ARGV_MAX 512
static char argv_buf[ARGV_MAX];
void *__fastcall__ __argv_mem(size_t size)
{
    return size<=ARGV_MAX ? argv_buf : NULL;
}
