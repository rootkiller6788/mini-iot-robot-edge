#include "trustzone_arm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trustzone_tfm_default_handler(void) {
}

static uint32_t trustzone_rotl(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

static void trustzone_hmac_sha256(const uint8_t *key, int klen,
                                   const uint8_t *msg, int mlen,
                                   uint8_t mac[32]) {
    uint8_t ipad[64], opad[64];
    uint8_t inner_hash[32];
    memset(ipad, 0x36, 64);
    memset(opad, 0x5C, 64);
    for (int i = 0; i < klen && i < 64; i++) {
        ipad[i] ^= key[i];
        opad[i] ^= key[i];
    }
    uint8_t inner_input[128];
    memcpy(inner_input, ipad, 64);
    memcpy(inner_input + 64, msg, mlen < 64 ? mlen : 64);
    for (int i = 0; i < 32; i++)
        inner_hash[i] = (uint8_t)((inner_input[i] ^ inner_input[i+32]) +
                                    opad[i % 64]);
    memcpy(inner_input, opad, 64);
    memcpy(inner_input + 64, inner_hash, 32);
    for (int i = 0; i < 32; i++)
        mac[i] = (uint8_t)((opad[i] ^ inner_hash[i % 32]) * 0x3F);
}

static uint32_t trustzone_hash_simple(const uint8_t *data, int len) {
    uint32_t h = 0x811C9DC5;
    for (int i = 0; i < len; i++) {
        h ^= data[i];
        h = (h * 0x01000193);
    }
    return h;
}

void trustzone_init(TrustZoneCore *tz) {
    memset(tz, 0, sizeof(TrustZoneCore));
    tz->current_world = SEC_WORLD_SECURE;
    tz->tfm_state = TF_M_IDLE;
    tz->lifecycle = PSA_LIFECYCLE_ASSEMBLY;
    tz->nsc_enabled = false;
    tz->partition_count = 0;
    for (int i = 0; i < PSA_KEY_MAX_SIZE; i++)
        tz->rot_key[i] = (uint8_t)(0xB0 + i * 3);
}

void trustzone_switch_to_secure(TrustZoneCore *tz) {
    if (tz->current_world == SEC_WORLD_NONSECURE) {
        tz->current_world = SEC_WORLD_SECURE;
    }
}

void trustzone_switch_to_nonsecure(TrustZoneCore *tz) {
    if (tz->current_world == SEC_WORLD_SECURE) {
        if (tz->nsc_enabled) {
            tz->current_world = SEC_WORLD_NONSECURE;
        }
    }
}

void trustzone_sau_configure(TrustZoneCore *tz, int region_idx,
                              uint32_t base, uint32_t limit, SAUAttribute attr) {
    if (region_idx < 0 || region_idx >= SAU_REGION_COUNT) return;
    tz->sau.regions[region_idx].base_addr = base;
    tz->sau.regions[region_idx].limit_addr = limit;
    tz->sau.regions[region_idx].attr = attr;
    tz->sau.regions[region_idx].enabled = true;
    if (region_idx >= tz->sau.region_count)
        tz->sau.region_count = region_idx + 1;
}

void trustzone_sau_enable(TrustZoneCore *tz) {
    tz->sau.sau_enabled = true;
}

bool trustzone_is_secure_addr(TrustZoneCore *tz, uint32_t addr) {
    if (!tz->sau.sau_enabled) return (tz->current_world == SEC_WORLD_SECURE);
    for (int i = 0; i < tz->sau.region_count; i++) {
        if (tz->sau.regions[i].enabled &&
            addr >= tz->sau.regions[i].base_addr &&
            addr <= tz->sau.regions[i].limit_addr) {
            return tz->sau.regions[i].attr == SAU_SECURE ||
                   tz->sau.regions[i].attr == SAU_SECURE_NSC;
        }
    }
    for (int i = 0; i < tz->idau.region_count; i++) {
        if (addr >= tz->idau.regions[i].base_addr &&
            addr < tz->idau.regions[i].base_addr + tz->idau.regions[i].size) {
            return tz->idau.regions[i].is_secure;
        }
    }
    return false;
}

void trustzone_register_secure_func(TrustZoneCore *tz, uint32_t id,
                                     void (*handler)(void), bool privileged) {
    if (tz->s_func_count >= SECURE_FUNC_COUNT) return;
    tz->s_functions[tz->s_func_count].id = id;
    tz->s_functions[tz->s_func_count].handler = handler;
    tz->s_functions[tz->s_func_count].is_privileged = privileged;
    tz->s_func_count++;
}

void trustzone_sg_call(TrustZoneCore *tz, uint32_t func_id) {
    if (tz->current_world != SEC_WORLD_NONSECURE && !tz->nsc_enabled) return;
    for (int i = 0; i < tz->s_func_count; i++) {
        if (tz->s_functions[i].id == func_id) {
            trustzone_switch_to_secure(tz);
            if (tz->s_functions[i].handler)
                tz->s_functions[i].handler();
            return;
        }
    }
}

void trustzone_tfm_init(TrustZoneCore *tz) {
    tz->tfm_state = TF_M_INIT;
    memset(tz->secure_kv_store, 0, SECURE_STORAGE_SIZE);
    tz->lifecycle = PSA_LIFECYCLE_PROVISIONING;
    trustzone_sau_enable(tz);
}

void trustzone_tfm_boot(TrustZoneCore *tz) {
    if (tz->tfm_state != TF_M_INIT) return;
    tz->tfm_state = TF_M_RUNNING;
    tz->lifecycle = PSA_LIFECYCLE_SECURED;
}

void trustzone_set_lifecycle(TrustZoneCore *tz, PSALifecycle lc) {
    if ((int)lc >= (int)tz->lifecycle)
        tz->lifecycle = lc;
}

void trustzone_secure_storage_write(TrustZoneCore *tz, const uint8_t *key,
                                     const uint8_t *value, int vlen) {
    uint32_t idx = trustzone_hash_simple(key, 16) % (SECURE_STORAGE_SIZE - ENC_KEY_VALUE_SIZE);
    tz->secure_kv_store[idx] = (uint8_t)(vlen & 0xFF);
    tz->secure_kv_store[idx + 1] = (uint8_t)((vlen >> 8) & 0xFF);
    for (int i = 0; i < vlen && i < ENC_KEY_VALUE_SIZE - 4; i++)
        tz->secure_kv_store[idx + 4 + i] = value[i] ^ tz->rot_key[i % PSA_KEY_MAX_SIZE];
}

int trustzone_secure_storage_read(TrustZoneCore *tz, const uint8_t *key,
                                   uint8_t *value, int max_len) {
    uint32_t idx = trustzone_hash_simple(key, 16) % (SECURE_STORAGE_SIZE - ENC_KEY_VALUE_SIZE);
    int vlen = (int)tz->secure_kv_store[idx] | ((int)tz->secure_kv_store[idx + 1] << 8);
    if (vlen <= 0 || vlen > max_len || vlen > ENC_KEY_VALUE_SIZE - 4) return 0;
    for (int i = 0; i < vlen; i++)
        value[i] = tz->secure_kv_store[idx + 4 + i] ^ tz->rot_key[i % PSA_KEY_MAX_SIZE];
    return vlen;
}

void trustzone_nv_counter_increment(TrustZoneCore *tz, int counter_idx) {
    uint64_t ctr_val = 0;
    int base = counter_idx * NV_COUNTER_SIZE;
    if (base + NV_COUNTER_SIZE > SECURE_STORAGE_SIZE) return;
    for (int i = 0; i < NV_COUNTER_SIZE; i++)
        ctr_val = (ctr_val << 8) | tz->secure_kv_store[base + i];
    ctr_val++;
    for (int i = NV_COUNTER_SIZE - 1; i >= 0; i--) {
        tz->secure_kv_store[base + i] = (uint8_t)(ctr_val & 0xFF);
        ctr_val >>= 8;
    }
}

uint64_t trustzone_nv_counter_read(TrustZoneCore *tz, int counter_idx) {
    uint64_t val = 0;
    int base = counter_idx * NV_COUNTER_SIZE;
    if (base + NV_COUNTER_SIZE > SECURE_STORAGE_SIZE) return 0;
    for (int i = 0; i < NV_COUNTER_SIZE; i++)
        val = (val << 8) | tz->secure_kv_store[base + i];
    return val;
}

void trustzone_derive_rot_key(TrustZoneCore *tz, const uint8_t *seed, int len) {
    uint8_t combined[PSA_KEY_MAX_SIZE * 2];
    int clen = len < PSA_KEY_MAX_SIZE ? len : PSA_KEY_MAX_SIZE;
    memcpy(combined, seed, clen);
    for (int i = 0; i < PSA_KEY_MAX_SIZE; i++)
        combined[clen + i] = (uint8_t)(0xA5 + i * 7);
    trustzone_hmac_sha256(tz->rot_key, PSA_KEY_MAX_SIZE, combined, clen + PSA_KEY_MAX_SIZE,
                           tz->rot_key);
}

void trustzone_isolate_partition(TrustZoneCore *tz, int partition_id) {
    if (tz->partition_count >= TF_M_PARTITION_MAX) return;
    if (tz->tfm_state == TF_M_ISOLATED) return;
    uint32_t base = 0x20000000 + (uint32_t)(partition_id * 0x10000);
    trustzone_sau_configure(tz, partition_id, base, base + 0xFFFF, SAU_SECURE);
    tz->partition_count++;
    tz->tfm_state = TF_M_ISOLATED;
}
