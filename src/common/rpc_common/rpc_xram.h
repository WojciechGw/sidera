/*
 * rpc_xram.h -- the XRAM that rpc.com and the programs it starts share.
 *
 * XRAM is the one memory every task sees and nothing in rpmt allocates, so
 * rpc takes $4000-$FFFF and leaves $0000-$3FFF to other programs:
 *
 *   $4000-$7FFF  left panel's directory listing, and the handoff
 *   $8000-$BFFF  right panel's
 *   $C000-$FFFF  two job slots of 8 KB for rpcw.com -- but $F700-$F7FF
 *                and $D700-$D7FF, the user's (RPC_XR_USERA, RPC_XR_USERB)
 *
 * A listing is a run of records, one per entry, in the order read, from
 * the start of its 16 KB up to RPC_XR_RECS:
 *
 *   +0 size (4)  +4 fdate (2)  +6 ftime (2)  +8 attr (1)  +9 len (1)
 *   +10 name, len bytes, no NUL
 *
 * and in its last 1 KB, from RPC_XR_IDX, where each record starts, two
 * bytes per entry in sorted order: 512 entries at most. Between the two,
 * in the left panel's area, is the handoff.
 *
 * rpce.com borrows $4000-$BFFF for its text: rpc reads both panels again
 * after every program it runs in the foreground.
 *
 * So may any other program: one run in the foreground -- from the line
 * under the panels or the shell's screen behind Ctrl+O, not with '&' --
 * has $0000-$BFFF while it runs. The shell holds the screen lock
 * meanwhile, so rpc draws nothing, and its Ctrl+L afterwards reads both
 * panels before anything is drawn from them; the cursor keeps its place by
 * number when its name is gone. A job of rpcw (F5, F6, F8) may go on: once
 * set up it works from its slot alone, $C000-$FFFF -- its names copied
 * there, the handoff no longer read. A program in the background has
 * $0000-$3FFF only, so a foreground program that takes part of that needs
 * none to be using it: boombox.com, which plays in the background, has
 * $1FF8-$3FFF for its record of the PCM: device and its ring while it plays. view.com takes $0000-$95FF for its bitmap while it
 * runs, viewstamp.com the same and its stamp from $9600, to $B5FF at most.
 * viewer.com, alone, goes further: its picture and palette from $0000 up to
 * $F6FF when no job runs -- B kept in its own memory and put back, the slot
 * headers it went over left FREE when it ends -- and to $BFFF while one
 * does, refusing a larger picture then.
 *
 * RPC_XR_USERA and RPC_XR_USERB, 256 bytes each, are the user's for good:
 * nothing of rpc, rpcw, rpcv or rpce touches them, nor anything that
 * borrows their XRAM. They are the ends of the job slots' lists of names,
 * which stop short of them (RPJ_NAMES_END): A, $F700-$F7FF, slot 2's; B,
 * $D700-$D7FF, slot 1's. RPC_XR_USER is A: view.com keeps its VGA mode 3
 * configuration at its start, $F700-$F70D; viewstamp.com the same, and its
 * mode 5 sprite and the sprite's palette after it, $F700-$F735; viewer.com
 * its picture's configuration and its I key's label, $F700-$F749.
 *
 * The portals are shared by every task, so each use of one goes under the
 * OS lock.
 */
#ifndef RPC_XRAM_H
#define RPC_XRAM_H

#define RPC_XR_PANEL0 0x4000u
#define RPC_XR_PANEL_SIZE 0x4000u
#define RPC_XR_RECS 0x3A00u /* in a panel's 16 KB: where the records end */
#define RPC_XR_IDX 0x3C00u  /* and where the index starts */

#define RPC_REC_HDR 10

/* --------------------------------------------------------------------------
 * The handoff: what rpc tells a program it starts over its screen -- rpcv,
 * rpce, rpcw, or viewer.com, which reads only the path -- and what that
 * program answers. Command lines are split at spaces and cannot quote, so
 * paths go this way.
 *
 * rpc gives the program the keyboard until it ends, or until it sets mode
 * to RPH_OVER: rpcw does, once a job is set up, and goes on in the
 * background.
 * -------------------------------------------------------------------------- */
#define RPC_XR_HAND 0x7A00u  /* the left panel's $3A00-$3BFF */
#define RPC_XR_HAND2 0xBA00u /* the right panel's: a job's target, 256 */

typedef struct
{
    char magic[4];       /* RPH_MAGIC */
    unsigned char mode;  /* RPH_ */
    unsigned char rows;  /* scr_rows */
    unsigned char cols;  /* scr_cols */
    unsigned char ascii; /* scr_ascii */
    /* view, edit: the file, absolute. A job: the source directory. Drive:
     * the panel's directory, and rpcw's answer, "" for none. */
    char path[256];
    /* A job: the active panel as rpc has it, for rpcw to take the names
     * from its listing in XRAM. */
    unsigned char op;    /* RPJ_O_ */
    unsigned char taken; /* rpcw's answer: the job's slot, 0xFF for none */
    unsigned xbase;      /* the panel's listing */
    unsigned n;
    unsigned cur;
    unsigned nsel;
    unsigned char sel[64]; /* a bit per entry, as panel_t has it */
} rpc_hand_t;

#define RPH_MAGIC "RPH1"
#define RPH_OVER 0    /* the program no longer needs the screen */
#define RPH_VIEW 'v'  /* rpcv */
#define RPH_EDIT 'e'  /* rpce */
#define RPH_TOUCH 't' /* touch: the path made, empty, if it is not there */
#define RPH_JOB 'j'   /* rpcw: ask about the job, take a slot, do it */
#define RPH_LIST 'l'  /* rpcw: the job list */
#define RPH_DRIVE 'd' /* rpcw: choose a drive */
#define RPH_HELP 'h'  /* rpcw: rpc's keys */
#define RPH_PICTURE 'p' /* viewer.com @: the BMP or PNG; why not, in path, if not */
#define RPH_SOUND 's' /* boombox.com @: the WAV; why not, in path, if not */
#define RPH_HDR 8     /* offset of path */
#define RPH_JOBPART (RPH_HDR + 256) /* offset of op */

/* rpce.com's text lives in $4000-$BFFF, with 1 KB of room to type in: a
 * larger file is only viewed. */
#define RPC_EDIT_MAX 31744UL

/* --------------------------------------------------------------------------
 * A job slot: one job for one rpcw.com, which copies, moves or deletes
 * entries while rpc goes on. Little-endian, 8 KB:
 *
 *   $0000  rpc_job_t
 *   $0040  the name rpcw is at, relative to the source, cut from the left
 *   $00C0  rpcw's report, once DONE: one line for rpc's message row
 *   $0100  the source directory, absolute, as rpc's panel has it
 *   $0200  the target, absolute, as rpcw's dialog made it; empty to delete
 *   $0300  the entries, each a name with its NUL, then one NUL more
 *   $1700  the user's 256 bytes: slot 1 RPC_XR_USERB, slot 2 RPC_XR_USERA
 *   $1800  rpcw's 2 KB for read_xram and write_xram
 *
 * The state: rpc starts `rpcw.com j` over its screen with the handoff.
 * rpcw asks where to and what about existing files; then it takes a slot
 * that is FREE or DONE, fills it, sets RUN, says which slot in the
 * handoff, gives the screen back (RPH_OVER) and works. Its last write is
 * DONE with a result and the report. rpc marks a RUN whose rpcw is gone
 * -- the task not named rpcw, or its spare byte not RPJ_SPARE(slot) -- as
 * DONE with RPJ_R_LOST.
 * -------------------------------------------------------------------------- */
#define RPC_XR_JOB0 0xC000u
#define RPC_XR_JOB_SIZE 0x2000u
#define RPC_JOBS 2
#define RPC_XR_JOB(slot) (RPC_XR_JOB0 + (unsigned)(slot) * RPC_XR_JOB_SIZE)

typedef struct
{
    char magic[2];          /* 'R' 'J' */
    unsigned char state;    /* RPJ_S_ */
    unsigned char op;       /* RPJ_O_ */
    unsigned char flags;    /* RPJ_F_ */
    unsigned char cancel;   /* rpc writes 1 to stop the job */
    unsigned char pid;      /* rpcw's task */
    unsigned char result;   /* RPJ_R_, once DONE */
    /* The progress, from here to the end: rpcw writes it all in one go, so
     * one read of it is always consistent. */
    unsigned char seq;      /* one more on every write of the progress */
    unsigned char err;      /* the last error's errno */
    unsigned nitems;        /* entries at $0300, counted by rpcw */
    unsigned item;          /* the one rpcw is at */
    unsigned done;          /* files and directories done */
    unsigned skipped;       /* already there, not overwritten */
    unsigned errors;
    unsigned long bytes;    /* copied so far */
    unsigned long cur_size; /* the file being copied */
    unsigned long cur_pos;
} rpc_job_t;

#define RPJ_PROGRESS 8 /* offset of seq */

#define RPJ_CUR 0x0040u
#define RPJ_CUR_SIZE 128
#define RPJ_REPORT 0x00C0u
#define RPJ_REPORT_SIZE 64 /* NUL included */
#define RPJ_SRC 0x0100u
#define RPJ_DST 0x0200u
#define RPJ_NAMES 0x0300u
#define RPJ_NAMES_END 0x1700u
#define RPJ_BUF 0x1800u
#define RPJ_BUF_SIZE 0x0800u

/* The user's two areas of 256 bytes, each between a slot's names and its
 * buffer; RPC_XR_USER is the first, where view.com keeps its configuration. */
#define RPC_XR_USERA (RPC_XR_JOB(1) + RPJ_NAMES_END) /* $F700 */
#define RPC_XR_USERB (RPC_XR_JOB(0) + RPJ_NAMES_END) /* $D700 */
#define RPC_XR_USER RPC_XR_USERA /* $F700 */
#define RPC_XR_USER_SIZE 0x100u

#define RPJ_S_FREE 0
#define RPJ_S_RUN 2
#define RPJ_S_DONE 3

#define RPJ_O_COPY 1
#define RPJ_O_MOVE 2
#define RPJ_O_DELETE 3

#define RPJ_F_OVERWRITE 0x01 /* else a file already there is skipped */

#define RPJ_R_OK 0
#define RPJ_R_CANCELLED 1 /* rpc asked it to stop */
#define RPJ_R_ABORTED 2   /* the target drive failed or filled */
#define RPJ_R_LOST 3      /* rpcw ended without finishing */

/* What rpcw leaves in its task entry's spare byte, so that rpc can tell it
 * from another task that took its number. */
#define RPJ_SPARE(slot) (0x80 | (slot))

#endif
