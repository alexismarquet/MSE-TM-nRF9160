#include <errno.h>
#include <cortex_m/tz.h>
#include <autoconf.h>
#include "secure_services.h"

#include <zephyr.h>
#include <sys/printk.h>
#include <string.h>
#include "nrf_cc3xx_platform.h"
#include "nrf_cc3xx_platform_kmu.h"
#include "mbedtls/cc3xx_kmu.h"
#include "mbedtls/aes.h"
#include "mbedtls/ctr_drbg.h"

__TZ_NONSECURE_ENTRY_FUNC
int spm_store_key_in_kmu_nse(uint32_t slot_id, uint32_t addr, uint32_t permission, uint8_t *key)
{
    int ret;

    ret = nrf_cc3xx_platform_kmu_write_key_slot(
        slot_id,
        addr,
        permission,
        key);
    return ret;
}

__TZ_NONSECURE_ENTRY_FUNC
int spm_encrypt_using_keyslot_nse(uint8_t slot_id, uint32_t keybits, uint8_t *plaintext, uint8_t *encrypted)
{

    mbedtls_aes_context ctx = {0};
    int ret;
    mbedtls_aes_init(&ctx);
    ret = mbedtls_aes_setkey_enc_shadow_key(&ctx, slot_id, keybits);
    if (ret != 0)
    {
        return -1;
    }
    mbedtls_aes_encrypt(&ctx, plaintext, encrypted);
    return 0;
}

__TZ_NONSECURE_ENTRY_FUNC
int spm_decrypt_using_keyslot_nse(uint8_t slot_id, uint32_t keybits, uint8_t *encrypted, uint8_t *plaintext)
{
    mbedtls_aes_context ctx = {0};
    int ret;

    // Reinitialize context.
    mbedtls_aes_init(&ctx);

    // Set to use direct shadow key for decryption.
    ret = mbedtls_aes_setkey_dec_shadow_key(&ctx, slot_id, keybits);
    if (ret != 0)
    {
        return -1;
    }

    mbedtls_aes_decrypt(&ctx, encrypted, plaintext);
    return 0;
}
/*
__TZ_NONSECURE_ENTRY_FUNC
uint64_t spm_request_UID()
{
    uint64_t uid = 0;
}*/
/*
__TZ_NONSECURE_ENTRY_FUNC
int spm_request_random_number_nse(uint8_t *output, size_t len, size_t *olen)
{
    int err;

    if (len != MBEDTLS_ENTROPY_MAX_GATHER)
    {
        return -EINVAL;
    }

    err = mbedtls_hardware_poll(NULL, output, len, olen);
    return err;
}
*/