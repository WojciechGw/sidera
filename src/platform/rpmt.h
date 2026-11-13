/*
 * rpmt.h -- the rpmt API for C (cc65). include/rpmt.inc is the same for
 * ca65; keep the two in step.
 *
 * Every task has a private slot 0: its own zero page, stack, arguments at
 * $0200 and cc65 C stack below $0800. A program started with rpmt_spawn()
 * also has $0800-$DFFF to itself, 54 kB: as much of it as the program needs
 * in blocks of its own, the shell's identity blocks in the rest, which it
 * must leave alone. Everything else is shared.
 *
 * The RIA's OS calls (files, directories, attributes) share one XSTACK, so
 * make them between rpmt_os_lock() and rpmt_os_unlock(), or through the
 * rpmt_open()... wrappers below. read() and write() are replaced by rpmt's
 * own: descriptors 0-2 are the console, the rest take the lock themselves.
 * Programs have no heap: malloc() returns NULL.
 */
#ifndef RPMT_H
#define RPMT_H

#include <rp6502.h>

#define RPMT_TASK_COUNT 16
#define RPMT_TICK_HZ 125

/* rpmt_task_t.status */
#define RPMT_TS_FREE 0
#define RPMT_TS_LOADING 1
#define RPMT_TS_RUNNING 2
#define RPMT_TS_FAILED 3
#define RPMT_TS_EXITED 4
#define RPMT_TS_KILLED 5

/* rpmt_task_t.flags */
#define RPMT_TF_KILL_PENDING 0x02
#define RPMT_TF_STOP_PENDING 0x04
/* Set by the shell for a program started with /v among its arguments,
 * with interrupts off (the core sets the bits above from other tasks): only
 * then does the shell say how it ended, "[n] name exit code". A start writes
 * the flags anew, so it never comes from an earlier task with the same
 * number. */
#define RPMT_TF_VERBOSE 0x08
/* Ctrl+C for a task that catches it (basic.com): the task sets CATCH_INT in
 * its own entry, with interrupts off; the shell then neither kills it nor
 * passes the key by, but sets SIGINT, also with interrupts off, and pushes
 * Ctrl+C into the ring, to wake a task waiting for a key. The task clears
 * SIGINT once it has seen it. */
#define RPMT_TF_CATCH_INT 0x10
#define RPMT_TF_SIGINT 0x20
/* Changes not saved, Linux's SIGTERM done rpmt's way. A program with text
 * of its own (rpce, hasse, calcit) keeps DIRTY set while it has changes
 * not saved (rpmt_set_dirty); cart, carts and `exit` warn of them. The
 * shell's `save` -- and `exit`, and what cart and carts ask it for -- sets
 * SIGTERM in each such task, one at a time gives it the keyboard and its
 * XRAM, as fg does, wakes it with Ctrl+L, and waits for it to end: it saves
 * a copy, <file>.sav, the file itself untouched, and ends. A program that
 * sets DIRTY must do that. Both with interrupts off. */
#define RPMT_TF_DIRTY 0x40
#define RPMT_TF_SIGTERM 0x80

/* rpmt_spawn() errors, and the exit code of a task that failed to load */
#define RPMT_E_NOENT 1
#define RPMT_E_HDR 2
#define RPMT_E_NOMEM 3
#define RPMT_E_IO 4
#define RPMT_E_NOTASK 5
#define RPMT_E_MAP 6
#define RPMT_EXIT_KILLED 130

/* The scheduler's task states, TASK_STATE[] in sched6502.s */
#define RPMT_DEAD 0
#define RPMT_RUNNABLE 1
#define RPMT_RUNNING 2
#define RPMT_SLEEPING 3
#define RPMT_STOPPED 4 /* rpmt's own: stopped by rpmt_stop() */

/* spare and spare2 are the task's own: rpmt neither writes nor clears them,
 * so they hold what the task put there -- or whatever the last task with
 * this number did, until it does. Another task can read and write them:
 * status.com takes the changes another status passes on there. */
typedef struct
{
    char name[8];
    unsigned char status;
    unsigned char exit;
    unsigned char flags;
    unsigned char spare;
    unsigned char unused[2]; /* was the builtin entry */
    unsigned char spare2[2];
} rpmt_task_t;

typedef struct
{
    volatile unsigned char *task_state; /* the scheduler's, per task */
    volatile unsigned char *block_owner; /* task number, 0xFF free, 0xFE reserved */
    volatile rpmt_task_t *tasks;
    volatile unsigned *ticks;
    volatile unsigned char *current;
    unsigned char task_count;
    unsigned char block_count; /* 0 = 256 */
    unsigned char first_block;
    unsigned char prog_slot0;
    unsigned char prog_slots;
} rpmt_sysinfo_t;

void rpmt_yield(void);
void __fastcall__ rpmt_exit(unsigned char code);

/* Start a task. >= 0: its number. < 0: it did not start, and the low byte is
 * the RPMT_E_ code. A program that fails to load has started: its status
 * goes from RPMT_TS_LOADING to RPMT_TS_FAILED, with the code as exit code. */
int __fastcall__ rpmt_spawn(const char *cmdline);

volatile rpmt_task_t *__fastcall__ rpmt_task_info(unsigned char id);
/* The lowest live task other than the caller whose name, as the task table
 * has it (the file's name up to any '.', 7 characters at most), is `name`;
 * -1 for none. For a program that keeps to one copy of itself. Needs an
 * rmt.rp6502 as new as this header: the core's jump table entry 30. */
int __fastcall__ rpmt_find_task(const char *name);
unsigned char __fastcall__ rpmt_kill(unsigned char id);
/* Stop task id where it is, and let it go on. rpmt_stop: 0 stopped, 1 no
 * such task, 2 noted, to stop when it gives the OS lock back, 3 refused (the
 * shell, or the caller). rpmt_cont: 0 continued, 1 it was not stopped. */
unsigned char __fastcall__ rpmt_stop(unsigned char id);
unsigned char __fastcall__ rpmt_cont(unsigned char id);
/* Wait for task id to end; its exit code, and its task table entry freed,
 * as rpmt_reap does. -1 for a number out of range or an entry already freed. */
int __fastcall__ rpmt_wait(unsigned char id);
/* Collect an ended task: its exit code, and its task table entry freed, so
 * that it no longer shows in a listing of ended tasks. -1 if it is still
 * running, or there is nothing to collect. */
int __fastcall__ rpmt_reap(unsigned char id);
unsigned char rpmt_getpid(void);

void __fastcall__ rpmt_sleep_ticks(unsigned ticks);
void __fastcall__ rpmt_sleep_ms(unsigned ms);
void __fastcall__ rpmt_sleep_until(unsigned char tick);
unsigned rpmt_ticks(void);

void rpmt_os_lock(void);
void rpmt_os_unlock(void);

void __fastcall__ rpmt_putc(char c);
void __fastcall__ rpmt_puts(const char *s);
/* The console whatever stdout is: rpmt_putc and rpmt_puts go to a file while
 * the shell has redirected stdout (RPMT_REDIR, below), these never. */
void __fastcall__ rpmt_con_putc(char c);
void __fastcall__ rpmt_con_puts(const char *s);
void __fastcall__ rpmt_puts_atomic(const char *s);
int rpmt_getc(void);
int rpmt_getc_nb(void); /* -1: nothing there */
unsigned char __fastcall__ rpmt_con_push(char c);
void __fastcall__ rpmt_set_fg(unsigned char id);
/* The task that has the keyboard, 0 the shell: RP_FG_TASK in rpmt.inc.
 * RPMT_BARE (bare/rpmt_bare.c, no rpmt): the program alone, task 0. */
#ifdef RPMT_BARE
#define RPMT_FG_TASK 0
#else
#define RPMT_FG_TASK (*(volatile unsigned char *)0xEF81)
#endif
/* A byte from the UART waits, not yet read -- by the shell, which hands it
 * on: a key on its way to the foreground task, there before long. Taken
 * from nobody (RIA.ready's RX bit, RIA_RX_READY in sched.inc). */
#define rpmt_rx_waiting() (RIA.ready & 0x40)
/* The task that has the console raw, 0 none: RP_CON_RAW. */
#define RPMT_CON_RAW_TASK (*(volatile unsigned char *)0xEF8B)
/* The shell's `autorx`, 0 off: shared, so that cart can keep it over a ROM
 * in session.cfg (RP_AUTORX). */
#define RPMT_AUTORX (*(volatile unsigned char *)0xEF89)

/* An overlay over the shell's prompt, as rpc is, Midnight Commander's way:
 * it draws on the alternate screen and leaves the row above the counters'
 * to the shell's command line. The shell sends it the keys that are not the
 * line's, runs what is typed on its own screen, and gives the overlay
 * Ctrl+L after: draw everything again. rpmt.inc has the addresses.
 *
 *   RPMT_OVERLAY    the overlay's task, set at its start, 0 at its end
 *   RPMT_SCR_OWNER  who draws on that screen now, 0xFF none: rpmt_scr_lock
 *   RPMT_SCR_DIRTY  the overlay drew: the shell puts its line back after
 *   RPMT_OVL_MODAL  the overlay has a dialog up: the shell sends it every key
 *   RPMT_OVL_MB     the overlay asks the shell: [0] a type, 0 none, then a
 *                   line; the shell takes it and sets the type to 0. Any
 *                   program may leave RPMT_OVL_RUN there as it ends -- cart
 *                   and carts leave "save cart <rom>" --: the shell runs the
 *                   line at its next prompt. */
#define RPMT_OVERLAY (*(volatile unsigned char *)0xEF8C)
#define RPMT_SCR_OWNER (*(volatile unsigned char *)0xEF8D)
#define RPMT_SCR_DIRTY (*(volatile unsigned char *)0xEF8E)
#define RPMT_OVL_MODAL (*(volatile unsigned char *)0xEF8F)
#define RPMT_OVL_MB ((volatile char *)0xEC00)
#define RPMT_OVL_LINE 128
#define RPMT_OVL_RUN 1    /* the line, as if typed and Enter */
#define RPMT_OVL_INSERT 2 /* the line, typed where the cursor is */
/* The screen to oneself, waiting while the other has it -- or taken over
 * from a task that has ended; and given back. */
void rpmt_scr_lock(void);
void rpmt_scr_unlock(void);
/* on = 1: take the console raw -- rpmt_getc_nb() then reads the UART itself,
 * every byte as it came, Ctrl+C and Ctrl+Z included, and every other task's
 * output waits until it is given back. Only for the task that has the
 * keyboard -- which the shell gives only once the program has loaded, so
 * ask again for a while before taking no for an answer. The shell cannot
 * interrupt it meanwhile, so the task must give
 * the user a way out itself. on = 0 gives it back, as ending, being killed
 * or stopped does. Taking it sends CAN, which ends an escape sequence
 * another task left half sent, and shows nothing. 0 done, 1 refused. For
 * file transfers. Needs an
 * rmt.rp6502 as new as this header: the core's jump table entry 31. */
unsigned char __fastcall__ rpmt_con_raw(unsigned char on);

unsigned __fastcall__ rpmt_phi2_set(unsigned khz);
unsigned rpmt_phi2_get(void);

const rpmt_sysinfo_t *rpmt_sysinfo(void);

/* What rpmt sends to the terminal when this task is killed or stopped, to
 * take down what it drew -- at most 31 characters, copied; NULL or "" for
 * nothing. A program's own memory is private, so this is the only way the
 * one that kills it can clean up after it. */
void __fastcall__ rpmt_set_cleanup(const char *seq);
void __fastcall__ rpmt_shutdown(unsigned char code);
/* The RIA's EXEC from rpmt: end every task and the system, as
 * rpmt_shutdown does, and load the ROM that the argv names -- pushed on the
 * XSTACK by the caller, in the layout ria_execv() sends, with the OS lock
 * held. ria_execv() itself cannot be used: the ROM would be loaded through
 * the caller's map, and the caller's code, at $0800, is gone once the maps
 * go back to identity. `term` goes to the terminal first, with every other
 * task already dead, so nothing draws after it -- at most 255 characters;
 * NULL for nothing. Never returns: if the RIA refuses the argv, rpmt exits
 * with code 1. Needs an rmt.rp6502 as new as this header: the core's jump
 * table entry 32. */
void __fastcall__ rpmt_exec(const char *term);

/* Whether there is VGA graphics to use: 1 yes, 0 no -- a serial terminal,
 * or GFX=0 in sys.cfg. The shell sets it at its start from GFX (0, 1, or
 * auto: whether the RIA's VSYNC counts, which only a VGA makes it do, and
 * no serial terminal answers ESC [ c -- the VGA alone); a
 * program that draws on the VGA refuses at 0. RP_GFX in rpmt.inc. */
#define RPMT_GFX (*(volatile unsigned char *)0xEC81)

/* Switching between programs in the foreground: Alt+Tab, Ctrl+Z, fg.
 *
 * The shell hands the keyboard from one program to another, and the screen
 * and XRAM go with it, so a program that comes back finds them as the last
 * one left them. It learns that it is back from rpmt_fg_gen(), which goes up
 * by 1 each time, or from a Ctrl+L, sent with RPMT_SWF_CTRL_L set; it then
 * sends its xreg calls again, goes back to its screen (the alternate one,
 * CSI ?1049h, if that is where it draws) and draws everything.
 *
 * XRAM it declares with rpmt_set_xram() is saved to a file while it is away
 * and put back before it runs again: such a program is stopped, not left
 * running, when it loses the keyboard, and bg and cont refuse it -- it would
 * write over the XRAM of the one in the foreground. 48 kB take about 0.1 s.
 * The range is cut to $0000-$BFFF, the foreground program's part of XRAM
 * (common/rpc_common/rpc_xram.h).
 *
 * The Alt+Tab list shows a program's name and its first argument, its
 * directory left out -- crt0_prog.s puts that in as its title, with
 * RPMT_SWF_ARG --, or the title it sets itself, alone: what tells two copies
 * of one program apart. Up to 9 characters; set it again whenever it
 * changes.
 *
 * RPMT_SWF_ALT says it draws on the alternate screen alone, so the shell's
 * screen is as it left it: the shell's prompt comes back on its own line.
 * RPMT_SWF_KEEP says more: whenever it waits for a key the terminal is as
 * the shell has it -- SGR 0, G0, wrapping on, the cursor shown. Its screen
 * can then stay on the alternate one while the shell's is shown (the
 * terminal has two, CSI ? 47 swaps them unerased); if it is the next to get
 * the keyboard -- the Alt+Tab list is drawn on the shell's screen then --,
 * it gets its screen back as it was, its cursor where it was, and no Ctrl+L. Its
 * cleanup is not sent when the shell stops it to save its XRAM then; one
 * with no XRAM is not stopped, and must not draw while it is away.
 *
 * Each entry is the task's own and is cleared when a program starts
 * (the core's child_start, and crt0_prog.s); rpmt.inc has the layout,
 * RPMT_SW. */
#define RPMT_SWF_CTRL_L 0x01
#define RPMT_SWF_ALT 0x02
#define RPMT_SWF_KEEP 0x04 /* RPMT_SWF_ALT and more */
#define RPMT_SWF_GFX 0x08 /* VGA canvas: shell restores console when parked/ended;
                          * declare XRAM, rebuild modes on fg_gen/Ctrl+L. */
#define RPMT_SWF_ARG 0x80 /* rpmt's: the title is the first argument */
#define RPMT_SW_TITLE 10
typedef struct
{
    unsigned xr_start;
    unsigned xr_len;
    unsigned char gen;
    unsigned char flags;
    char title[RPMT_SW_TITLE];
} rpmt_sw_t;
#define RPMT_SW ((volatile rpmt_sw_t *)0xEC82)

/* For a screensaver -- status.com /ss -- which runs in the background and
 * sees no keys: the shell's view of the terminal, and the keyboard's bytes.
 * rpmt.inc has the addresses; the shell clears them at its start, with
 * RPMT_SW.
 *
 *   RPMT_TERM_ROWS   the terminal's rows and columns, as the shell asked
 *   RPMT_TERM_COLS   them; 0 not yet known
 *   RPMT_OVL_HIDDEN  Ctrl+O has the shell's screen up over the overlay's
 *   RPMT_ALT_OWN     the task whose screen the shell keeps on the alternate
 *                    one, 0xFF none; a screensaver that wipes it sets 0xFF,
 *                    and the task gets Ctrl+L when it is back instead
 *   RPMT_KEYS_IN     counts every byte the UART gives: one changed, a key
 *   RPMT_SAVER       non-zero: the UART is the screensaver's -- its task in
 *                    the low four bits, status.com's SV_INPLACE in bit 7;
 *                    rpmt_getc_nb() reads nothing from it, for anyone.
 *                    The screensaver reads the waking key itself, puts the
 *                    screen back, and sets it to 0. Needs an rmt.rp6502 as
 *                    new as this header. */
#define RPMT_TERM_ROWS (*(volatile unsigned char *)0xED82)
#define RPMT_TERM_COLS (*(volatile unsigned char *)0xED83)
#define RPMT_OVL_HIDDEN (*(volatile unsigned char *)0xED84)
#define RPMT_ALT_OWN (*(volatile unsigned char *)0xED85)
#define RPMT_KEYS_IN (*(volatile unsigned char *)0xED86)
#define RPMT_SAVER (*(volatile unsigned char *)0xED87)

/* For a debugger (dbg.com): where a task's memory is. RPMT_SLOT0[task] is
 * the block of its slot 0; in that slot 0, RPMT_MAP[slot - 1] the block of
 * a program's slot 1-27, valid from the program's base slot up to its end
 * (the header at RPMT_HDR). */
#define RPMT_SLOT0 ((volatile unsigned char *)0xED88)

/* The status bar, status.com: the terminal's last two rows and the first
 * 16 columns of row 2, which no program draws on but it. rpmt.inc has the
 * addresses.
 *
 *   RPMT_UI_COLOR  a task's colour, RPMT_UI_*, for a program with the
 *                  system's frame (row 1 its title in that colour); 0 none.
 *                  Set by the program, cleared at its start (crt0_prog.s).
 *                  Row 2 shows each such task in its column, task + 1, and
 *                  the one in the foreground with a '.'.
 *   RPMT_SCR_GEN   +1 whenever a screen is cleared or the terminal goes to
 *                  the other one: the bar, and the counters in the row above
 *                  the last, draw themselves again.
 *   RPMT_BAR_RIGHT the last row's columns at the right that are the bar's,
 *                  the clock, its alarm and the heart: a program's message
 *                  there goes from column 1 and stops short of them.
 *
 * The rows the bar leaves to others: the last one from column 1 to the
 * program in the foreground, "Ready" and its messages; the one above it,
 * from the left, to counter.com -- unless a note or the foreground
 * program's line (rpmt_set_line) has it. */
#define RPMT_UI_COLOR ((volatile unsigned char *)0xED98)
#define RPMT_UI_SHELL 1   /* dark grey */
#define RPMT_UI_RPC 2     /* amber: rpc, rpce, rpcv */
#define RPMT_UI_CARTS 3   /* magenta */
#define RPMT_UI_HASSE 4   /* dark cyan */
#define RPMT_UI_CALCIT 5  /* green */
#define RPMT_UI_WRITEIT 6 /* blue */
#define RPMT_UI_CARDIT 7  /* maroon */
#define RPMT_SCR_GEN (*(volatile unsigned char *)0xEDA8)
#define RPMT_BAR_RIGHT 32

/* The system's frame, the same in every program that has one: row 1
 * RPMT_UI_PROMPT in reverse video, then the program's name and, for one on
 * a file, " > " and the file, white on its colour to the row's end; row 2
 * from RPMT_UI_KEYS_COL its key hint, dark grey (RPMT_UI_SGR_KEYS). The
 * colours as SGR, white on each, by RPMT_UI_*; a program with none -- a
 * tool, keyboard or ctx -- has the shell's and no column in row 2. */
#define RPMT_UI_PROMPT " razemOS(mt) > "
#define RPMT_UI_KEYS_COL 17
#define RPMT_UI_SGR_KEYS "\x1b[0;90m"
#define RPMT_UI_SGR_SHELL "\x1b[0;97;100m"
#define RPMT_UI_SGR_RPC "\x1b[0;97;48;5;214m"
#define RPMT_UI_SGR_CARTS "\x1b[0;97;45m"
#define RPMT_UI_SGR_HASSE "\x1b[0;97;48;5;23m"
#define RPMT_UI_SGR_CALCIT "\x1b[0;97;48;5;28m"
#define RPMT_UI_SGR_WRITEIT "\x1b[0;97;44m"
#define RPMT_UI_SGR_CARDIT "\x1b[0;97;48;5;88m"

/* A note from any task, shown by status.com in the row above the last,
 * grey, for `seconds` (1-255), over the counters; a newer one takes
 * its place. Without status.com it goes to the console as a line. 0
 * shown, 1 printed. Only in rpmt.lib. */
unsigned char __fastcall__ rpmt_notify(const char *text, unsigned char seconds);
/* The note itself: RPMT_NOTE_STATE 0 none, 1 to be shown, 2 shown. */
#define RPMT_NOTE_STATE (*(volatile unsigned char *)0xEDA9)
#define RPMT_NOTE_SECONDS (*(volatile unsigned char *)0xEDAA)
#define RPMT_NOTE_FROM (*(volatile unsigned char *)0xEDAB)
#define RPMT_NOTE_TEXT ((volatile char *)0xEDAC)
#define RPMT_NOTE_MAX 83
/* The calling task's line: status.com shows text in the row above the last,
 * from column 1, grey on black to the temperature, while the task
 * has the keyboard and no note is
 * up, and the counters stay away meanwhile. One line, the last task's to
 * set it; a program's start takes its own number's back (crt0_prog.s).
 * Drawn at the bar's next step, 128 ms at most; without status.com not at
 * all. Only in rpmt.lib. rpce, hasse and rpcv put where in the text there. */
void __fastcall__ rpmt_set_line(const char *text);
/* RPMT_LINE_TASK whose it is, 0 none; RPMT_LINE_GEN +1 with every
 * rpmt_set_line; RPMT_LINE_SHOWN 1 while status.com has it drawn. */
#define RPMT_LINE_TASK (*(volatile unsigned char *)0xEFC0)
#define RPMT_LINE_GEN (*(volatile unsigned char *)0xEFC1)
#define RPMT_LINE_SHOWN (*(volatile unsigned char *)0xEFC2)
#define RPMT_LINE_TEXT ((volatile char *)0xEFC3)
#define RPMT_LINE_MAX 60
/* The bar's name as the task table has it: status.com's. */
#define RPMT_BAR_NAME "status"
#define RPMT_MAP 0x02ACu
#define RPMT_HDR 0x02A4u

/* Redirection: `a > file`, `a < file`, `a | b` (the shell runs b after a,
 * through a file). RPMT_REDIR is where the task's stdin (high four bits)
 * and stdout (low four) go, the RIA's descriptor of a file the shell has
 * open, 0 the console; crt0_prog.s sets it from the command line. Redirected
 * stdout takes write(1), rpmt_putc and rpmt_puts, each written at once,
 * byte for byte; stderr, rpmt_con_putc, rpmt_puts_atomic and the keyboard
 * (rpmt_getc) stay the console's. Redirected stdin is read(0) and stdio's:
 * 0 at the end of the file. RPMT_BARE: never redirected. */
#ifdef RPMT_BARE
#define RPMT_REDIR 0
#else
#define RPMT_REDIR (*(volatile unsigned char *)0x02C7)
#endif
#define RPMT_REDIR_MARK 0x01 /* rpmt.inc's REDIR_MARK */
#define rpmt_out_tty() (!(RPMT_REDIR & 0x0F))
#define rpmt_in_tty() (!(RPMT_REDIR & 0xF0))

void __fastcall__ rpmt_set_xram(unsigned start, unsigned len);
void __fastcall__ rpmt_set_swf(unsigned char flags);
void __fastcall__ rpmt_set_title(const char *title);
unsigned char rpmt_fg_gen(void);
/* RPMT_TF_DIRTY in the caller's entry, on or off; and whether the shell has
 * set RPMT_TF_SIGTERM there: save a copy and end. Only in rpmt.lib. */
void __fastcall__ rpmt_set_dirty(unsigned char on);
unsigned char rpmt_sigterm(void);

/* A task's own arguments: argv[0] is the program name. */
#define rpmt_argc (*(int *)0x0280)
#define rpmt_argv ((char **)0x0282)

/* OS calls under the lock. */
int __fastcall__ rpmt_open(const char *name, int flags);
int __fastcall__ rpmt_close(int fd);
int __fastcall__ rpmt_opendir(const char *name);
int __fastcall__ rpmt_readdir(f_stat_t *dirent, int dirdes);
int __fastcall__ rpmt_closedir(int dirdes);
int __fastcall__ rpmt_chdir(const char *name);
int __fastcall__ rpmt_getcwd(char *name, int size);

#endif
