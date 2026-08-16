#ifndef SHA1_HPP
#define SHA1_HPP

#include <cstdint>

#define HASH_SIZE 20 // size of the hash in bytes

void hash(const char *str, uint8_t hash[HASH_SIZE]);

#endif //SHA1_HPP
