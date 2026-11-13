/* Terminal keys decoded by key.c. */
#ifndef SIDERA_KEY_H
#define SIDERA_KEY_H

#define K_NONE 0
#define K_BS 0x0100
#define K_ESC 0x0101
#define K_UP 0x0102
#define K_DOWN 0x0103
#define K_LEFT 0x0104
#define K_RIGHT 0x0105
#define K_HOME 0x0106
#define K_END 0x0107
#define K_PGUP 0x0108
#define K_PGDN 0x0109
#define K_INS 0x010A
#define K_DEL 0x010B
#define K_F1 0x0110 /* F1..F12: K_F1 + 0..11 */
#define K_SHIFT 0x1000
#define K_ALT 0x2000
#define K_CTRL 0x4000
#define K_CODE(k) ((k) & 0x0FFF)
#define CTRL(c) ((c) & 0x1F)

/* A key, K_NONE if nothing has come. */
unsigned key_poll(void);

#endif
