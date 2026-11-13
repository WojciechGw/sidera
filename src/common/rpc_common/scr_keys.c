/*
 * scr_keys.c -- the two rows at the top, the system's frame (rpmt.h): the
 * title and the function key hint (scr.h).
 */
#include <string.h>
#include "rpmt.h"
#include "scr.h"

void o_title(const char *color, const char *name, const char *file)
{
    unsigned char n;
    unsigned char room;

    o_at(1, 1);
    o_seq(C_CUR);
    o_str(RPMT_UI_PROMPT);
    o_seq(color);
    o_ch(' ');
    o_str(name);
    if (file && *file)
    {
        /* The prompt, the space and the name, " > " and the free last
         * column. */
        n = (unsigned char)(sizeof(RPMT_UI_PROMPT) - 1 + 1 + strlen(name) + 3 + 1);
        room = scr_cols > n ? (unsigned char)(scr_cols - n) : 0;
        if (room >= 4)
        {
            o_str(" > ");
            n = (unsigned char)strlen(file);
            if (n > room)
            {
                o_str("...");
                file += n - (room - 3);
            }
            o_str(file);
        }
    }
    /* The RIA's terminal erases in the current background: the colour to
     * the end. */
    o_seq("\x1b[K");
    o_seq(C_NORMAL);
}

void o_keys(unsigned char row, const char *s)
{
    o_at(row, KEYS_COL);
    o_seq(C_DIM);
    o_fit(s, (unsigned char)(scr_cols - (KEYS_COL - 1)));
    o_seq(C_NORMAL);
}
