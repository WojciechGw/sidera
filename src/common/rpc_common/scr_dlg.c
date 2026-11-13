/*
 * scr_dlg.c -- dialogs: a box in the middle, drawn over the screen, which
 * the caller draws again afterwards (scr.h). The box, a line of input and
 * a question; a menu and a text are scr_menu.c and scr_text.c.
 */
#include <string.h>
#include "rpmt.h"
#include "key.h"
#include "scr.h"

unsigned char dl_row;
unsigned char dl_col;
unsigned char dl_w;

void box(const char *title, unsigned char w, unsigned char h, const char *attr)
{
    unsigned char r;
    unsigned char tl = (unsigned char)strlen(title);
    unsigned char lead;

    if (w > scr_cols)
        w = scr_cols;
    dl_w = w;
    dl_row = (unsigned char)((scr_rows - h) / 2 + 1);
    dl_col = (unsigned char)((scr_cols - w) / 2 + 1);
    if (tl > w - 4)
        tl = (unsigned char)(w - 4);
    lead = (unsigned char)((w - 2 - tl) / 2);

    /* No cursor in a box, whatever showed one before -- an input field
     * shows it in its own (dlg_input). */
    o_seq(CUR_OFF);
    o_seq(attr);
    o_at(dl_row, dl_col);
    o_frame(FR_TL, 1);
    o_frame(FR_H, lead);
    o_fit(title, tl);
    o_frame(FR_H, (unsigned char)(w - 2 - lead - tl));
    o_frame(FR_TR, 1);
    for (r = 1; r < h - 1; ++r)
    {
        o_at((unsigned char)(dl_row + r), dl_col);
        o_frame(FR_V, 1);
        o_rep(' ', (unsigned char)(w - 2));
        o_frame(FR_V, 1);
    }
    o_at((unsigned char)(dl_row + h - 1), dl_col);
    o_frame(FR_BL, 1);
    o_frame(FR_H, (unsigned char)(w - 2));
    o_frame(FR_BR, 1);
}

void box_line(unsigned char i, const char *s)
{
    o_at((unsigned char)(dl_row + 1 + i), (unsigned char)(dl_col + 2));
    o_fit(s, (unsigned char)(dl_w - 4));
}

unsigned char dlg_width(void)
{
    return scr_cols > 68 ? 64 : (unsigned char)(scr_cols - 4);
}

unsigned char dlg_input(const char *title, const char *prompt, char *buf, unsigned char max)
{
    ed_t saved;
    unsigned k;
    unsigned char w = dlg_width();

    saved = ed;
    ed_set(buf, max);
    box(title, w, 6, C_DLG);
    box_line(0, prompt);
    box_line(3, "Enter OK   Esc cancel");
    for (;;)
    {
        o_seq(C_DLGIN);
        ed_draw((unsigned char)(dl_row + 2), (unsigned char)(dl_col + 2), (unsigned char)(w - 4));
        o_seq(CUR_ON);
        o_flush();
        k = key_wait();
        if (k == '\r' || k == K_ESC)
            break;
        ed_key(k);
    }
    ed = saved;
    /* The plain colour again, not the field's: whoever draws next -- rpce
     * its text rows -- may draw without setting one. And the cursor off
     * again, as it was before the field showed it: rpcv has none, and who
     * wants one shows it where it belongs (rpce after a redraw, the shell
     * on its line under rpc). */
    o_seq(C_NORMAL);
    o_seq(CUR_OFF);
    return k == '\r';
}

unsigned char dlg_confirm(const char *title, const char *text)
{
    unsigned k;
    box(title, dlg_width(), 5, C_ERR);
    box_line(0, text);
    box_line(2, "Enter/Y yes   Esc/N no");
    o_flush();
    for (;;)
    {
        k = key_wait();
        if (k == '\r' || k == 'y' || k == 'Y' || k == K_ESC || k == 'n' || k == 'N')
            break;
    }
    o_seq(C_NORMAL); /* as dlg_input */
    return k == '\r' || k == 'y' || k == 'Y';
}
