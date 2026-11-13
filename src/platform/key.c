/*
 * key.c -- RIA character sequences as K_ codes (key.h).
 * Ctrl+horizontal arrows also consult the USB HID modifier bitmap, since
 * the character sequence alone does not distinguish left and right Ctrl.
 *
 * The RIA's own keyboard sends VT sequences: ESC [ A for Up, ESC O P for
 * F1, ESC [ 15 ~ for F5, ESC [ 1 ; m X with a modifier m = 1 + Shift +
 * 2 Alt + 4 Ctrl. Terminals on the UART send their own variants: ESC [ 11 ~
 * for F1, ESC [ 1 ~ and ESC [ 4 ~ for Home and End, ESC O A for Up, and
 * PuTTY ESC [ 29 ~, F16, for Shift+F6. All of them are taken.
 *
 * A sequence arrives as one burst, so a byte that does not follow ESC
 * within ESC_TICKS means the ESC was the key itself. ESC and a digit is
 * F1-F10, as in Midnight Commander, for terminals without function keys;
 * ESC and another character is Alt with it; ESC ESC is Esc.
 */
#include <platform.h>
#include "key.h"

#define ESC_TICKS 8 /* 64 ms */

static int key_byte_within(unsigned char ticks)
{
    unsigned start = platform_ticks();
    int c;
    for (;;)
    {
        c = platform_getc_nb();
        if (c >= 0)
            return c;
        if ((unsigned)(platform_ticks() - start) >= ticks)
            return -1;
    }
}

static unsigned tilde_key(unsigned char n)
{
    switch (n)
    {
    case 1:
    case 7:
        return K_HOME;
    case 2:
        return K_INS;
    case 3:
        return K_DEL;
    case 4:
    case 8:
        return K_END;
    case 5:
        return K_PGUP;
    case 6:
        return K_PGDN;
    case 23:
    case 24:
        return K_F1 + 10 + (n - 23);
    /* F13-F20, which PuTTY sends for Shift+F3..F10, as VT220s did: Shift+F1
     * and F2 are F11 and F12 there, the keys themselves. */
    case 25:
    case 26:
        return (K_F1 + 2 + (n - 25)) | K_SHIFT;
    case 28:
    case 29:
        return (K_F1 + 4 + (n - 28)) | K_SHIFT;
    case 31:
    case 32:
    case 33:
    case 34:
        return (K_F1 + 6 + (n - 31)) | K_SHIFT;
    }
    if (n >= 11 && n <= 15)
        return K_F1 + (n - 11);
    if (n >= 17 && n <= 21)
        return K_F1 + 5 + (n - 17);
    return K_NONE;
}

static unsigned letter_key(int c)
{
    switch (c)
    {
    case 'A':
        return K_UP;
    case 'B':
        return K_DOWN;
    case 'C':
        return K_RIGHT;
    case 'D':
        return K_LEFT;
    case 'H':
        return K_HOME;
    case 'F':
        return K_END;
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
        return K_F1 + (c - 'P');
    }
    return K_NONE;
}

static unsigned csi(void)
{
    unsigned char p0 = 0;
    unsigned char p1 = 0;
    unsigned char second = 0;
    unsigned char m;
    unsigned k;
    int c;

    for (;;)
    {
        c = key_byte_within(ESC_TICKS);
        if (c < 0)
            return K_NONE;
        if (c >= '0' && c <= '9')
        {
            if (second)
                p1 = (unsigned char)(p1 * 10 + (c - '0'));
            else
                p0 = (unsigned char)(p0 * 10 + (c - '0'));
        }
        else if (c == ';')
            second = 1;
        else
            break;
    }
    k = c == '~' ? tilde_key(p0) : letter_key(c);
    if (k == K_NONE)
        return K_NONE;
    m = p1 ? (unsigned char)(p1 - 1) : 0;
    if (m & 1)
        k |= K_SHIFT;
    if (m & 2)
        k |= K_ALT;
    if (m & 4)
        k |= K_CTRL;
    return k;
}

static unsigned decode(int c)
{
    if (c == 0x7F || c == 0x08)
        return K_BS;
    if (c == '\n')
        return '\r'; /* Enter, from a terminal that sends LF */
    if (c != 0x1B)
        return (unsigned)c;
    c = key_byte_within(ESC_TICKS);
    if (c < 0 || c == 0x1B)
        return K_ESC;
    if (c == '[')
        return csi();
    if (c == 'O')
        return letter_key(key_byte_within(ESC_TICKS));
    if (c >= '1' && c <= '9')
        return K_F1 + (c - '1');
    if (c == '0')
        return K_F1 + 9;
    return K_ALT | (unsigned)c;
}

unsigned key_poll(void)
{
    int c = platform_getc_nb();
    unsigned k = c < 0 ? K_NONE : decode(c);
    if((K_CODE(k)==K_LEFT||K_CODE(k)==K_RIGHT)&&(k&K_CTRL)&&
       (platform_keyboard_modifiers()&0x11)==0x01)
        k=(k&~K_CTRL)|K_LCTRL;
    return k;
}

