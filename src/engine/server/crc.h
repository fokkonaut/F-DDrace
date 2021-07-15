#ifndef ENGINE_SERVER_CRC_H
#define ENGINE_SERVER_CRC_H

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *modify_file_crc32(const char *path, uint64_t offset, uint32_t newcrc, bool printstatus);

uint32_t get_crc32_and_length(FILE *f, uint64_t *length);
void fseek64(FILE *f, uint64_t offset);
uint32_t reverse_bits(uint32_t x);

uint64_t multiply_mod(uint64_t x, uint64_t y);
uint64_t pow_mod(uint64_t x, uint64_t y);
void divide_and_remainder(uint64_t x, uint64_t y, uint64_t *q, uint64_t *r);
uint64_t reciprocal_mod(uint64_t x);
int get_degree(uint64_t x);

#endif
