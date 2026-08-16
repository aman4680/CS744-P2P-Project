#include "sha1.hpp"
#include <cstring>
#include <openssl/sha.h>

void hash(const char *str, uint8_t hash[HASH_SIZE]) {
    SHA1(reinterpret_cast<const unsigned char *>(str), strlen(str), hash);
}
