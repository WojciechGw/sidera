/*
 * hand.h -- the handoff, as a program rpc starts reads it (hand.c).
 */
#ifndef RPC_HAND_H
#define RPC_HAND_H

#include "rpc_xram.h"

extern rpc_hand_t hand;

/* The handoff into hand, its magic cleared so that it is used once, and
 * the screen's size and frames as rpc has them. 0 if there is none. */
unsigned char hand_open(void);

#endif
