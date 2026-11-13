/*
 * scr_term.c -- the terminal: its size, the alternate screen and the
 * sequence that puts the shell's screen back (scr.h).
 */
#include <string.h>
#include "rpmt.h"
#include "key.h"
#include "util.h"
#include "scr.h"

#define ANSWER_TICKS 25 /* 200 ms */
#define DRAIN_TICKS 13  /* 100 ms */

char scr_cleanup[32];

static unsigned char read_num(int *c)
{
    unsigned char v = 0;
    for (;;)
    {
        *c = key_byte_within(ANSWER_TICKS);
        if (*c < '0' || *c > '9')
            return v;
        v = (unsigned char)(v * 10 + (*c - '0'));
    }
}

/* As the shell asks for it: the cursor to the far corner, where is it?
 * Without an answer, or with a strange one, the sizes stay as they were:
 * 30 rows (scr_rows 28) and 80 columns. */
unsigned char scr_ask_size(void)
{
    unsigned char rows = 0;
    unsigned char cols = 0;
    int c;
    rpmt_puts_atomic("\x1b" "7\x1b[999;999H\x1b[6n\x1b" "8");
    for (;;)
    {
        c = key_byte_within(ANSWER_TICKS);
        if (c < 0)
            break;
        if (c != 0x1b || key_byte_within(ANSWER_TICKS) != '[')
            continue;
        rows = read_num(&c);
        if (c == ';')
            cols = read_num(&c);
        break;
    }
    while (key_byte_within(DRAIN_TICKS) >= 0)
        ;
    if (rows >= 3)
        scr_rows = (unsigned char)(rows - 2);
    if (cols >= 40)
        scr_cols = cols;
    return rows;
}

void scr_make_cleanup(void)
{
    char *p = scr_cleanup;
    strcpy(p, "\x1b[0m\x1b[?7h\x1b[?25h\x1b[3;");
    p += strlen(p);
    p += fmt_ul(p, scr_rows);
    strcpy(p, "r\x1b[?1049l");
}

/* Either way another program has the screen next, or had it: what the
 * attributes are is not known (scr_out.c). */
void scr_leave(void)
{
    o_sgr_reset();
    o_seq(scr_cleanup);
    o_flush();
    scr_cleared();
}

void scr_enter(void)
{
    o_sgr_reset();
    o_seq("\x1b[?1049h\x1b[r\x1b[?7l\x1b[?25h\x1b)0\x0f");
    o_flush();
    scr_cleared();
}
