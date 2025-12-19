#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

// Compute SHA-256 digest of input data.
void sha256(const uint8_t* data, size_t len, uint8_t out_hash[32]);

// Convenience: compute SHA-256 and output as lowercase hex (64 chars + '\0').
void sha256_hex(const uint8_t* data, size_t len, char out_hex[65]);

#endif // SHA256_H


