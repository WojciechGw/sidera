/*
 * hand.c -- the handoff, as a program rpc starts reads it (hand.h).
 */
#include <string.h>
#include "rpmt.h"
#include "xr.h"
#include "scr.h"
#include "hand.h"

rpc_hand_t hand;

unsigned char hand_open(void)
{
    xr_read(RPC_XR_HAND, &hand, sizeof(hand));
    if (memcmp(hand.magic, RPH_MAGIC, 4))
        return 0;
    xr_write(RPC_XR_HAND, "", 1);
    scr_rows = hand.rows;
    scr_cols = hand.cols;
    scr_ascii = hand.ascii;
    return 1;
}
