/*
 * scr.h -- the screen for rpc and the programs it starts: one output
 * buffer, colours, frames, the line editor, the message row and dialogs.
 *
 *   scr_out.c   the buffer, o_*(), frames
 *   scr_term.c  the terminal's size, the alternate screen and its cleanup
 *   scr_ed.c    the line editor
 *   scr_msg.c   the message row
 *   scr_dlg.c   dialogs
 *
 * Apart, so that a program links only what it uses.
 */
#ifndef RPC_SCR_H
#define RPC_SCR_H

/* The terminal's rows less two -- rpmt's counters and the shell's status
 * row are below -- and its columns. */
extern unsigned char scr_rows;
extern unsigned char scr_cols;
extern unsigned char scr_ascii; /* frames in ASCII rather than DEC line drawing */

/* One colour: the terminal's own white on black, with bold for what is
 * selected, reverse video for a cursor bar, dark grey for what only
 * explains and for a dialog's input field, amber for what belongs to the
 * active panel. Each string once. */
extern const char C_NORMAL[];
extern const char C_SEL[];      /* bold */
extern const char C_CUR[];      /* reverse */
extern const char C_CURSEL[];   /* both */
extern const char C_DIM[];      /* dark grey */
extern const char C_INFO[];     /* amber: xterm 256 colour 214, #FFAF00 */
extern const char C_INFO_REV[]; /* the same, reverse: black on amber */
/* White on dark grey, for a dialog's input field. Not reverse video: the
 * terminal draws its cursor in white, which a white field would hide. */
extern const char C_FIELD[];
/* White on amber: row 1 after the title's reverse " razemOS(mt) > ",
 * RPMT_UI_SGR_RPC. */
extern const char C_HEADER[];
/* The cursor is hidden while a program redraws, and shown again where it
 * is to stay. */
extern const char CUR_OFF[];
extern const char CUR_ON[];
#define C_MSG C_SEL
#define C_DLG C_NORMAL
#define C_DLGIN C_FIELD
#define C_ERR C_SEL

/* Frame pieces for o_frame(). */
#define FR_TL 0
#define FR_TR 1
#define FR_BL 2
#define FR_BR 3
#define FR_H 4
#define FR_V 5
#define FR_VT 6 /* thin vertical, between columns */
#define FR_ML 7 /* left end of the thin rule */
#define FR_MR 8
#define FR_HT 9  /* thin horizontal */
#define FR_TT 10 /* column tee in the top line */
#define FR_MT 11 /* column tee in the thin rule */
#define FR_X 12  /* cross */
/* n of a frame piece, in DEC line drawing (or ASCII with scr_ascii), as one
 * send. */
void o_frame(unsigned char piece, unsigned char n);
/* A vertical line down from row, col: top, n of FR_V, bottom. */
void o_vline(unsigned char row, unsigned char col, unsigned char top, unsigned char n,
             unsigned char bottom);

void o_flush(void);
/* Set by every send; whoever cares clears it (rpc, as an overlay). */
extern unsigned char o_sent;
void o_ch(char c);
void o_str(const char *s);
/* s whole in one flush: an escape sequence must not be split, another
 * task's drawing could land inside it. An SGR that is the one sent last is
 * left out (scr_out.c). */
void o_seq(const char *s);
/* The attributes on the screen are not known any more: the next SGR goes
 * out whatever it is. */
void o_sgr_reset(void);
void o_at(unsigned char row, unsigned char col);
/* The next row, from column col: where the output so far ended on the row
 * above, cheaper than o_at. */
void o_nextrow(unsigned char col);
/* n of c; a long run of spaces as ECH and CUF, outside reverse video. */
void o_rep(char c, unsigned char n);
void o_dec(unsigned v);
/* s in exactly w columns: cut, marked with '~', or padded with spaces. */
void o_fit(const char *s, unsigned char w);
/* A function key hint on row, from column KEYS_COL, dark grey, cut to the
 * screen's width (scr_keys.c): row 2's columns before it are the status
 * bar's (rpmt.h). */
#define KEYS_COL RPMT_UI_KEYS_COL
void o_keys(unsigned char row, const char *s);
/* Row 1, the system's frame (rpmt.h): RPMT_UI_PROMPT in reverse, then a
 * space and name -- "File Manager", the space is o_title's -- and, for a
 * program on a file, " > " and the file, its end after "..." if the row is
 * short and the last column left free as the shell leaves it; in color,
 * white on the program's colour, to the row's end (scr_keys.c). */
void o_title(const char *color, const char *name, const char *file);

/* --- scr_term.c --- */
/* The terminal's size into scr_rows and scr_cols, from its answer to
 * CSI 6 n with the cursor in the far corner; 0 if it did not answer. */
unsigned char scr_ask_size(void);
/* What puts the shell's screen back: attributes off, wrapping and the
 * cursor on, the shell's scroll region (rows 3 to scr_rows), the main
 * screen. Made by scr_make_cleanup(), for rpmt_set_cleanup() too. */
extern char scr_cleanup[32];
void scr_make_cleanup(void);
/* The alternate screen, the whole of it scrolling, no wrapping, the cursor
 * on, DEC line drawing in G1 for the frames, G0 active. */
void scr_enter(void);
/* The shell's screen again: scr_cleanup. */
void scr_leave(void);
/* After a program's own CSI 2 J: the status bar draws itself again (rpmt.h).
 * scr_enter and scr_leave say it themselves. */
#define scr_cleared() (++RPMT_SCR_GEN)

/* --- scr_ed.c: the line editor, one at a time --- */
typedef struct
{
    char *buf;
    unsigned char max; /* buffer size, NUL included */
    unsigned char pos;
    unsigned char off; /* first shown */
} ed_t;
extern ed_t ed; /* dlg_input saves and restores the one it interrupts */
void ed_set(char *buf, unsigned char max);
/* 1 if the key was the editor's. */
unsigned char ed_key(unsigned k);
void ed_draw(unsigned char row, unsigned char col, unsigned char w);

/* --- scr_msg.c: the message row --- */
#define MSG_COL 1
extern unsigned char scr_msg_row;
extern unsigned char scr_msg_w;
/* What the program draws after a message, to put the cursor back where it
 * belongs: rpc's command line, say. Each program defines it. */
void scr_msg_hook(void);
void msg(const char *s);        /* the message row, until the next key */
void msg_err(const char *what); /* what, and errno's text */
void msg_clear(void);
void msg_redraw(void); /* after the screen was cleared */

/* --- scr_dlg.c: a box in the middle, drawn over the screen, which the
 * caller draws again afterwards --- */
extern unsigned char dl_row;
extern unsigned char dl_col;
extern unsigned char dl_w;
void box(const char *title, unsigned char w, unsigned char h, const char *attr);
/* Line i inside the box, from its third column. */
void box_line(unsigned char i, const char *s);
unsigned char dlg_width(void);
unsigned char dlg_input(const char *title, const char *prompt, char *buf, unsigned char max);
unsigned char dlg_confirm(const char *title, const char *text);
/* A list to choose from: items NUL-separated, "" at the end; the chosen
 * one's number, or -1. */
int dlg_menu(const char *title, const char *items, unsigned char cur);
void dlg_text(const char *title, const char *const *lines);

#endif
