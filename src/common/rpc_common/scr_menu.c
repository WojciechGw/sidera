/*
 * scr_menu.c -- a dialog with a list to choose from (scr.h).
 */
#include <string.h>
#include "rpmt.h"
#include "key.h"
#include "scr.h"

int dlg_menu(const char *title, const char *items, unsigned char cur)
{
    const char *p;
    unsigned char n = 0;
    unsigned char w = 12;
    unsigned char i;
    unsigned char l;
    unsigned k;

    /* Wide enough for the title too: box() cuts a longer one, with '~'. */
    l = (unsigned char)strlen(title);
    if (l + 6 > w)
        w = (unsigned char)(l + 6);
    for (p = items; *p; p += l + 1)
    {
        l = (unsigned char)strlen(p);
        if (l + 6 > w)
            w = (unsigned char)(l + 6);
        ++n;
    }
    if (!n)
        return -1;
    if (cur >= n)
        cur = 0;
    box(title, w, (unsigned char)(n + 2), C_DLG);
    for (;;)
    {
        for (i = 0, p = items; i < n; ++i, p += strlen(p) + 1)
        {
            o_seq(i == cur ? C_CUR : C_DLG);
            o_at((unsigned char)(dl_row + 1 + i), (unsigned char)(dl_col + 1));
            o_ch(' ');
            o_fit(p, (unsigned char)(w - 3));
        }
        o_flush();
        k = key_wait();
        switch (k)
        {
        case K_UP:
            cur = cur ? (unsigned char)(cur - 1) : (unsigned char)(n - 1);
            break;
        case K_DOWN:
            cur = (unsigned char)(cur + 1 < n ? cur + 1 : 0);
            break;
        case K_HOME:
            cur = 0;
            break;
        case K_END:
            cur = (unsigned char)(n - 1);
            break;
        case '\r':
            o_seq(C_NORMAL); /* as dlg_input: not the cursor bar's */
            return cur;
        case K_ESC:
            o_seq(C_NORMAL);
            return -1;
        }
    }
}
