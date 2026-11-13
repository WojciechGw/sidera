/*
 * util.h -- rpc's helpers without I/O: masks, paths, number formats. The
 * host test builds util.c natively, so nothing here may include rpmt.h.
 */
#ifndef RPC_UTIL_H
#define RPC_UTIL_H

#define RPC_PATH 256

/* Does name match any of the masks, "*.com;*.txt" -- separated by ';', ','
 * or ' ', '*' and '?' as wildcards, case ignored, "*.*" everything. An
 * empty list matches everything. */
unsigned char mask_match(const char *masks, const char *name);

/* Where the name part starts: after "MSC0:/" or "/". */
unsigned char path_root_len(const char *path);
unsigned char path_is_root(const char *path);

/* dir + "/" + name into dst; 0 if it would not fit in RPC_PATH. */
unsigned char path_join(char *dst, const char *dir, const char *name);

/* path loses its last component, which goes to child (RPC_PATH); the root
 * stays. 0 if path was the root already. */
unsigned char path_parent(char *path, char *child);

/* --- pathx.c --- */
/* path loses its last component; the root stays. */
void path_up(char *path);
/* a and b the same, case ignored. */
unsigned char path_same(const char *a, const char *b);
/* a is b, or inside it, case ignored. */
unsigned char path_is_inside(const char *a, const char *b);
/* path into dst, w bytes with its NUL: whole, or its end after a '~'. */
void path_tail(char *dst, const char *path, unsigned char w);

/* --- errtext.c --- */
/* An errno as a few words: "disk full". */
const char *err_text(int e);

/* --- textcut.c --- */
/* One row of text from buf[*pos..len) into dst, at most w columns and a
 * NUL: a tab to the next eighth column, CR left out, a byte the terminal
 * must not get as '.' -- below a space, DEL, and with ascii the upper half
 * -- the rest of a longer line skipped; *pos after the row's LF. */
void text_cut(char *dst, const unsigned char *buf, unsigned len, unsigned *pos,
              unsigned char w, unsigned char ascii);

/* --- ftype.c: rpc's file info (Alt+I) --- */
/* v bytes, and below 1 MB in whole KB, above in MB to a tenth, both cut:
 * "1300000 B, 1.2 MB", "1024 B, 1 KB", "1023 B". 24 bytes at most. */
void fmt_bytes(char *dst, unsigned long v);
/* What the file is, from its name and its first n bytes h, into dst (40):
 * "rpmt program, $0800-$5FFF" ("(old)" at $8000), ".com, not rpmt",
 * "RP6502 ROM", "BMP 640x480, 8 bit", "PNG 320x16, 32 bit", "text" --
 * no control byte but TAB, CR, LF and ^Z --, "binary" or "empty". */
void file_type(char *dst, const char *name, const unsigned char *h, unsigned n);

/* Right-aligned in width columns, v in bytes: digits while they fit, then
 * kilobytes with a K. */
void fmt_size(char *dst, unsigned long v, unsigned char width);
/* Decimal, no padding; returns the length. */
unsigned char fmt_ul(char *dst, unsigned long v);
/* DOS date and time: "yy-mm-dd", with the century "yyyy-mm-dd"; "hh:mm". */
void fmt_date(char *dst, unsigned fdate, unsigned char century);
void fmt_time(char *dst, unsigned ftime);

#endif
