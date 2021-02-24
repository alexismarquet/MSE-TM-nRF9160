#ifndef CUSTOM_SECURE_H__
#define CUSTOM_SECURE_H__

#include <stdint.h>

int spm_store_key_in_kmu(uint32_t slot_id, uint32_t addr, uint32_t permission, uint8_t *key);

int spm_encrypt_using_keyslot(uint8_t slot_id, uint32_t keybits, uint8_t *plaintext, uint8_t *encrypted);

int spm_decrypt_using_keyslot(uint8_t slot_id, uint32_t keybits, uint8_t *encrypted, uint8_t *plaintext);

#endif