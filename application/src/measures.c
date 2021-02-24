#include "measures.h"

LOG_MODULE_REGISTER(measures, LOG_LEVEL_DBG);

void swap(measure_t *a, measure_t *b)
{
    measure_t tmp;
    memcpy(&tmp, a, sizeof(measure_t));
    memcpy(a, b, sizeof(measure_t));
    memcpy(b, &tmp, sizeof(measure_t));
}

void shuffle(measure_t *m, uint32_t n)
{
    for (int i = 0; i < n; i++)
    {
        int j = randint(0, n);
        swap(&m[i], &m[j]);
    }
}

int encrypt(uint8_t *plainIn, uint8_t *encryptedOut, size_t len)
{
    int ret = 0;
    for (int i = 0; i < len / 16; i++)
    {
        ret += spm_encrypt_using_keyslot(KEYSLOT_SF, SF_BIT_LENGTH, &plainIn[i * 16], &encryptedOut[i * 16]);
    }
    return ret;
}

int decrypt(uint8_t *encryptedIn, uint8_t *plainOut, size_t len)
{
    int ret = 0;
    for (int i = 0; i < len / 16; i++)
    {
        ret += spm_decrypt_using_keyslot(KEYSLOT_SF, SF_BIT_LENGTH, &encryptedIn[i * 16], &plainOut[i * 16]);
    }
    return ret;
}

size_t pack(uint8_t *buf, measure_t *m)
{
    uint32_t index = 0;

    memcpy(buf + index, m->MUID, 32 * sizeof(m->MUID[0]));
    index += 32 * sizeof(m->MUID[0]);
    memcpy(buf + index, &m->timestamp, sizeof(m->timestamp));
    index += sizeof(m->timestamp);
    memcpy(buf + index, m->position, 2 * sizeof(m->position[0]));
    index += 2 * sizeof(m->position[0]);
    memcpy(buf + index, &m->data, sizeof(m->data));
    index += sizeof(m->data);
    memcpy(buf + index, &m->reliability, sizeof(m->reliability));
    index += sizeof(m->reliability);
    return index;
}
size_t unpack(uint8_t *buf, measure_t *m)
{
    uint32_t index = 0;

    memcpy(m->MUID, buf + index, 32 * sizeof(m->MUID[0]));
    index += 32 * sizeof(m->MUID[0]);
    memcpy(&m->timestamp, buf + index, sizeof(m->timestamp));
    index += sizeof(m->timestamp);
    memcpy(m->position, buf + index, 2 * sizeof(m->position[0]));
    index += 2 * sizeof(m->position[0]);
    memcpy(&m->data, buf + index, sizeof(m->data));
    index += sizeof(m->data);
    memcpy(&m->reliability, buf + index, sizeof(m->reliability));
    index += sizeof(m->reliability);
    return index;
}

int GenerateMUID(measure_t *m, uint64_t uid)
{

    // get random number from TRNG
    // should not be an array, only one, but currently the TRNG only accepts a 144-len request
    uint64_t rand[18];
    size_t oLen = 0;
    spm_request_random_number((uint8_t *)rand, sizeof(rand), &oLen);

    // concatenate data
    uint8_t _buf[512];
    sprintf(_buf, "%" PRId64 "%" PRId64 "%" PRIx64, uid, m->timestamp, rand[0]);

    //LOG_DBG("MUID unhashed: %s", _buf);
    // hash string to get MUID
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    const size_t payloadLength = strlen(_buf);
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char *)_buf, payloadLength);
    mbedtls_md_finish(&ctx, m->MUID);
    mbedtls_md_free(&ctx);

    // HEPIA coordinates
    m->position[0] = 0;
    m->position[1] = 0;

    // datetime

    int ret = date_time_now(&m->timestamp);
    if (ret)
    {
        LOG_ERR("failed to get UTC time: %d", ret);
    }

    // since temp is from modem
    m->reliability = 1.0;
    return 0;
}

void printMeasure(measure_t *m)
{
    LOG_INF("MUID: 0x");
    for (int i = 0; i < 32; i++)
    {
        printk("%02X", m->MUID[i]);
    }
    printk("");
    LOG_INF("%" PRIX64 "", m->timestamp);
    LOG_INF("%" PRIx64 ", %" PRIx64 "", m->position[0], m->position[1]);
    LOG_INF("%" PRId64 "", m->data);
    LOG_INF("%" PRIx64 "", m->reliability);
}

void printMUID(measure_t *m)
{
    char buf[64];
    for (int i = 0; i < 32; i++)
    {
        sprintf(&buf[i * 2], "%02X", m->MUID[i]);
        //printk("%02X", m->MUID[i]);
    }
    LOG_INF("MUID: 0x%s", buf);
}

char *measure_as_json(measure_t *measure)
{
    cJSON *clear = cJSON_CreateObject();
    if (clear == NULL)
    {
        LOG_ERR("failed to allocate memory for measure json");
    }
    char MUIDstr[65];
    for (int x = 0; x < 32; x++)
    {
        sprintf(&MUIDstr[x * 2], "%02X", measure->MUID[x]);
    }
    cJSON_AddStringToObject(clear, "MUID", MUIDstr);
    cJSON_AddNumberToObject(clear, "timestamp", measure->timestamp);
    cJSON *positions = cJSON_AddArrayToObject(clear, "position");
    cJSON_AddItemToArray(positions, cJSON_CreateNumber(measure->position[0]));
    cJSON_AddItemToArray(positions, cJSON_CreateNumber(measure->position[1]));
    cJSON_AddNumberToObject(clear, "data", measure->data);
    cJSON_AddNumberToObject(clear, "reliability", measure->reliability);
    char *message = cJSON_PrintUnformatted(clear);
    cJSON_Delete(clear);
    return message;
}