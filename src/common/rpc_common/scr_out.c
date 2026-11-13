/*
 * scr_out.c -- output: one buffer sent with rpmt_puts_atomic, so the
 * clock's and the heart's drawings, which come between any two sends, never
 * land inside an escape sequence. o_seq() and o_at() keep a sequence in one
 * send; text may be cut anywhere. Also the frames.
 *
 * Every byte counts: the RIA sends one character per UART character time,
 * 115200 baud, about 11 500 a second, with the VGA terminal fed at the same
 * pace (com_uart_drain_tx in the RIA's sys/com.c) -- whatever PHI2 is. So:
 *
 * - An SGR (a sequence ending in 'm', all the C_ colours) that is the one
 *   sent last is not sent again. Other tasks draw between ESC 7 and ESC 8,
 *   which give the attributes back, so the memory holds -- until another
 *   program has the screen: scr_enter() and scr_leave() forget it, as does
 *   o_sgr_reset().
 * - A run of spaces long enough goes out as ECH and CUF, erase and step on,
 *   unless the attributes are reverse video: a terminal on the UART erases
 *   with the background alone, which would cut a cursor bar short.
 * - A vertical line is placed once and goes down piece by piece: the piece,
 *   BS, CUD (o_vline).
 */
#include <string.h>
#include "rpmt.h"
#include "scr.h"

#define OB_SIZE 128

unsigned char scr_rows = 28;
unsigned char scr_cols = 80;
unsigned char scr_ascii;

const char C_NORMAL[] = "\x1b[0m";
const char C_SEL[] = "\x1b[0;1m";
const char C_CUR[] = "\x1b[0;7m";
const char C_CURSEL[] = "\x1b[0;1;7m";
const char C_DIM[] = "\x1b[0;90m";
const char C_INFO[] = "\x1b[0;38;5;214m";
const char C_INFO_REV[] = "\x1b[0;7;38;5;214m";
const char C_FIELD[] = "\x1b[0;100m";
const char C_HEADER[] = RPMT_UI_SGR_RPC;
const char CUR_OFF[] = "\x1b[?25l";
const char CUR_ON[] = "\x1b[?25h";

#define SO 0x0E
#define SI 0x0F
#define BLANK_RUN 10 /* from here ECH and CUF, at most 10 bytes, beat the spaces */

static char ob[OB_SIZE + 2]; /* and an SI a send may need, and the NUL */
static unsigned char on;
unsigned char o_sent;
static unsigned char shifted; /* the buffer is in SO: o_vline's */

static const char *sgr;       /* the attributes last sent; 0 not known */
static unsigned char sgr_rev = 1; /* they are reverse video, or not known */

/* Into the buffer, which the caller has made room in. One call each rather
 * than ob[on++] written out: cc65 makes a dozen bytes of every one of
 * those. */
static void __fastcall__ put(char c)
{
    ob[on++] = c;
}

static void __fastcall__ put_s(const char *s)
{
    while (*s)
        put(*s++);
}

static void shift_in(void);

void o_flush(void)
{
    shift_in();
    if (on)
    {
        ob[on] = '\0';
        rpmt_puts_atomic(ob);
        on = 0;
        o_sent = 1;
    }
}

static void __fastcall__ room(unsigned char n)
{
    if (on + n > OB_SIZE)
        o_flush();
}

void o_ch(char c)
{
    room(1);
    put(c);
}

void o_str(const char *s)
{
    while (*s)
        o_ch(*s++);
}

void o_sgr_reset(void)
{
    sgr = 0;
    sgr_rev = 1;
}

/* The attributes are known by the C_ string that set them: every program
 * sends them by name, so the same pointer is the same colour. */
void o_seq(const char *s)
{
    unsigned char n = (unsigned char)strlen(s);
    if (s[n - 1] == 'm')
    {
        if (s == sgr)
            return;
        sgr = s;
        sgr_rev = s == C_CUR || s == C_CURSEL || s == C_INFO_REV;
    }
    room(n);
    put_s(s);
}

static void dec_raw(unsigned v)
{
    char d[5];
    unsigned char n = 0;
    do
    {
        d[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        put(d[--n]);
}

void o_dec(unsigned v)
{
    room(5);
    dec_raw(v);
}

void o_at(unsigned char row, unsigned char col)
{
    room(8);
    put_s("\x1b[");
    dec_raw(row);
    put(';');
    dec_raw(col);
    put('H');
}

/* CSI n f, n from 1. */
static void csi_n(unsigned char n, char f)
{
    put_s("\x1b[");
    dec_raw(n);
    put(f);
}

void o_rep(char c, unsigned char n)
{
    if (c == ' ' && n >= BLANK_RUN && !sgr_rev)
    {
        room(10);
        csi_n(n, 'X');
        csi_n(n, 'C');
        return;
    }
    while (n--)
        o_ch(c);
}

void o_nextrow(unsigned char col)
{
    room(8);
    put_s("\x1b[E");
    if (col > 1)
        csi_n((unsigned char)(col - 1), 'C');
}

void o_fit(const char *s, unsigned char w)
{
    unsigned char n = 0;
    if (!w)
        return;
    while (s[n] && n <= w)
        ++n;
    if (n > w)
    {
        n = (unsigned char)(w - 1);
        while (n--)
            o_ch(*s++);
        o_ch('~');
        return;
    }
    w = (unsigned char)(w - n);
    while (n--)
        o_ch(*s++);
    o_rep(' ', w);
}

/* The frames are drawn in DEC Special Graphics, the VT100 line-drawing set,
 * which the RIA's terminal has built in (and draws with its code page's
 * glyphs) and terminals on the UART know too, whatever their encoding --
 * unlike CP437's box bytes, which a UTF-8 terminal shows as garbage. It is
 * designated into G1 once (ESC ) 0, see rpc_screen), and a piece of frame
 * goes out as SO, its letters, SI, in one send: the clock and the other
 * tasks draw between rpc's sends, and always find G0, plain ASCII.
 *
 * ┌ ┐ └ ┘ ─ │ │ ├ ┤ ─ ┬ ┴ ┼ in DEC graphics, and in ASCII for /a. The set
 * has single lines only. */
static const char fr_dec[] = "lkmjqxxtuqwvn";
static const char fr_ascii[] = "++++-||++-+++";

/* A frame piece: SO first unless the buffer is in SO already; a flush in
 * between ends the send with SI (o_flush), so the next starts with SO. */
static void __fastcall__ piece_out(unsigned char piece)
{
    if (scr_ascii)
        put(fr_ascii[piece]);
    else
    {
        if (!shifted)
        {
            put(SO);
            shifted = 1;
        }
        put(fr_dec[piece]);
    }
}

static void shift_in(void)
{
    if (shifted)
    {
        put(SI);
        shifted = 0;
    }
}

void o_frame(unsigned char piece, unsigned char n)
{
    while (n--)
    {
        room(3); /* SO, the piece, and the SI a flush adds */
        piece_out(piece);
    }
    shift_in();
}

/* Down from each piece to under it: BS and CUD -- but in the last column the
 * cursor stays where the piece went, wrapping being off, and BS would take
 * it a column too far left; there CUD and CHA, the column itself. */
void o_vline(unsigned char row, unsigned char col, unsigned char top, unsigned char n,
             unsigned char bottom)
{
    unsigned char piece = top;
    o_at(row, col);
    for (;;)
    {
        room(10); /* SO, the piece, CUD and CHA, and the SI a flush adds */
        piece_out(piece);
        if (piece == bottom)
            break;
        if (col < scr_cols)
            put_s("\b\x1b[B");
        else
        {
            put_s("\x1b[B");
            csi_n(col, 'G');
        }
        piece = n-- ? FR_V : bottom;
    }
    shift_in();
}
