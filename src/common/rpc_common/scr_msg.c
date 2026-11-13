/*
 * scr_msg.c -- the message row (scr.h).
 */
#include <errno.h>
#include <string.h>
#include "rpmt.h"
#include "key.h"
#include "util.h"
#include "scr.h"

/* The message row, from MSG_COL, scr_msg_w long: in rpc the terminal's
 * last, where the shell says "Ready", short of the status bar's
 * RPMT_BAR_RIGHT columns (rpmt.h). */
unsigned char scr_msg_row;
unsigned char scr_msg_w;

/* The message, a copy: the caller's buffer may change before it is drawn
 * again. */
static char m[80];
static unsigned char shown; /* until the next key */

static void msg_put(const char *s, const char *attr)
{
    o_at(scr_msg_row, MSG_COL);
    o_seq(attr);
    o_fit(s, scr_msg_w);
    o_seq(C_NORMAL);
}

void msg(const char *s)
{
    if (s != m)
        strncpy(m, s, sizeof(m) - 1);
    shown = 1;
    msg_put(m, C_MSG);
    scr_msg_hook();
}

/* The message, or "Ready" if there is none. */
void msg_redraw(void)
{
    if (shown)
        msg(m);
    else
        msg_put("Ready", C_NORMAL);
}

void msg_clear(void)
{
    if (!shown)
        return;
    shown = 0;
    msg_redraw();
}

void msg_err(const char *what)
{
    unsigned char n = (unsigned char)strlen(what);
    if (n > 50)
        n = 50;
    memcpy(m, what, n);
    m[n++] = ':';
    m[n++] = ' ';
    strcpy(m + n, err_text(errno));
    msg(m);
}
