#ifndef MEASURES_H__
#define MEASURES_H__

#include <stdio.h>
#include <stdlib.h>
#include <logging/log.h>
#include <secure_services.h>
#include <sha256.h>
#include <mbedtls/md.h>
#include "custom_secure.h"
#include <date_time.h>
#include <cJSON.h>
#include <cJSON_os.h>

#define KEYSLOT_SF 2
#define SF_BIT_LENGTH 128

typedef struct
{
    uint8_t MUID[32];
    uint64_t timestamp;
    double position[2];
    uint64_t data;
    float reliability;

} measure_t;

void swap(measure_t *a, measure_t *b);
void shuffle(measure_t *m, uint32_t n);
int encrypt(uint8_t *plainIn, uint8_t *encryptedOut, size_t len);
int decrypt(uint8_t *encryptedIn, uint8_t *plainOut, size_t len);
size_t pack(uint8_t *buf, measure_t *m);
size_t unpack(uint8_t *buf, measure_t *m);
int GenerateMUID(measure_t *m, uint64_t uid);
void printMeasure(measure_t *m);
void printMUID(measure_t *m);
char *measure_as_json(measure_t *measure);

#endif