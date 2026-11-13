/*
 * scr_text.c -- a dialog with lines of text, until a key (scr.h).
 */
#include "rpmt.h"
#include "key.h"
#include "scr.h"

void dlg_text(const char *title, const char *const *lines)
{
    unsigned char n = 0;
    while (lines[n])
        ++n;
    box(title, dlg_width(), (unsigned char)(n + 2), C_DLG);
    for (n = 0; lines[n]; ++n)
        box_line(n, lines[n]);
    o_flush();
    key_wait();
}
