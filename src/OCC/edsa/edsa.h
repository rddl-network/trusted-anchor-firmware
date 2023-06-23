#ifndef EDSA_FUNCTIONS_H
#define EDSA_FUNCTIONS_H

#include "../utils/utils.h"

extern uint8_t pub_key_buffer[33];
extern uint8_t hash_buffer[32];

#define EC_FLAGS_TYPES (EC_FLAG_ECDSA | EC_FLAG_SCHNORR)

static bool is_valid_ec_type(uint32_t flags);

void routeEcdsaPubKey(OSCMessage &msg, int addressOffset);
void routeEcdsaSigFromBytes(OSCMessage &msg, int addressOffset);
void routeEcdsaSigVerifyPubkeyHash(OSCMessage &msg, int addressOffset);
void routeEcdsaSigVerify(OSCMessage &msg, int addressOffset);

#endif // EDSA_FUNCTIONS_H
