#ifndef SPM_INTERNAL_H__
#define SPM_INTERNAL_H__

#include <zephyr.h>
#include <nrfx.h>
#include <misc/util.h>

/*
#define NRF_NSE(ret, name, ...)  \
    ret name##_nse(__VA_ARGS__); \
    TZ_THREAD_SAFE_NONSECURE_ENTRY_FUNC(name, ret, name##_nse, __VA_ARGS__)
*/
NRF_NSE(int, spm_store_key_in_kmu, uint32_t slot_id, uint32_t addr, uint32_t permission, uint8_t *key);

NRF_NSE(int, spm_encrypt_using_keyslot, uint8_t slot_id, uint32_t keybits, uint8_t *plaintext, uint8_t *encrypted);

NRF_NSE(int, spm_decrypt_using_keyslot, uint8_t slot_id, uint32_t keybits, uint8_t *encrypted, uint8_t *plaintext);
#endif /* SPM_INTERNAL_H__ */