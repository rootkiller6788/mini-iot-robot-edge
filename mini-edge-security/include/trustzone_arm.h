#ifndef TRUSTZONE_ARM_H
#define TRUSTZONE_ARM_H

#include <stdbool.h>
#include <stdint.h>

#define SAU_REGION_COUNT    8
#define IDAU_REGION_COUNT   4
#define SECURE_FUNC_COUNT   16
#define TF_M_PARTITION_MAX  8
#define PSA_KEY_MAX_SIZE    32
#define SECURE_STORAGE_SIZE 4096
#define ENC_KEY_VALUE_SIZE  256
#define NV_COUNTER_SIZE     8

typedef enum {
    SEC_WORLD_NONSECURE,
    SEC_WORLD_SECURE
} SecureWorld;

typedef enum {
    SAU_NSEC,
    SAU_SECURE_NSC,
    SAU_SECURE
} SAUAttribute;

typedef enum {
    TF_M_IDLE,
    TF_M_INIT,
    TF_M_RUNNING,
    TF_M_ISOLATED,
    TF_M_ERROR
} TFMState;

typedef enum {
    PSA_LIFECYCLE_UNKNOWN,
    PSA_LIFECYCLE_ASSEMBLY,
    PSA_LIFECYCLE_PROVISIONING,
    PSA_LIFECYCLE_SECURED,
    PSA_LIFECYCLE_DECOMMISSIONED
} PSALifecycle;

typedef struct {
    uint32_t base_addr;
    uint32_t limit_addr;
    SAUAttribute attr;
    bool enabled;
} SAURegion;

typedef struct {
    SAURegion regions[SAU_REGION_COUNT];
    int region_count;
    bool sau_enabled;
} SAUContext;

typedef struct {
    uint32_t base_addr;
    uint32_t size;
    bool is_secure;
} IDAURegion;

typedef struct {
    IDAURegion regions[IDAU_REGION_COUNT];
    int region_count;
} IDAUContext;

typedef struct {
    uint32_t id;
    void (*handler)(void);
    bool is_privileged;
} SecureFunction;

typedef struct {
    SecureWorld current_world;
    SAUContext sau;
    IDAUContext idau;
    SecureFunction s_functions[SECURE_FUNC_COUNT];
    int s_func_count;
    TFMState tfm_state;
    PSALifecycle lifecycle;
    uint8_t rot_key[PSA_KEY_MAX_SIZE];
    uint8_t secure_kv_store[SECURE_STORAGE_SIZE];
    bool nsc_enabled;
    int partition_count;
} TrustZoneCore;

void trustzone_init(TrustZoneCore *tz);
void trustzone_switch_to_secure(TrustZoneCore *tz);
void trustzone_switch_to_nonsecure(TrustZoneCore *tz);
void trustzone_sau_configure(TrustZoneCore *tz, int region_idx,
                              uint32_t base, uint32_t limit, SAUAttribute attr);
void trustzone_sau_enable(TrustZoneCore *tz);
bool trustzone_is_secure_addr(TrustZoneCore *tz, uint32_t addr);
void trustzone_register_secure_func(TrustZoneCore *tz, uint32_t id,
                                     void (*handler)(void), bool privileged);
void trustzone_sg_call(TrustZoneCore *tz, uint32_t func_id);
void trustzone_tfm_init(TrustZoneCore *tz);
void trustzone_tfm_boot(TrustZoneCore *tz);
void trustzone_set_lifecycle(TrustZoneCore *tz, PSALifecycle lc);
void trustzone_secure_storage_write(TrustZoneCore *tz, const uint8_t *key,
                                     const uint8_t *value, int vlen);
int trustzone_secure_storage_read(TrustZoneCore *tz, const uint8_t *key,
                                   uint8_t *value, int max_len);
void trustzone_nv_counter_increment(TrustZoneCore *tz, int counter_idx);
uint64_t trustzone_nv_counter_read(TrustZoneCore *tz, int counter_idx);
void trustzone_derive_rot_key(TrustZoneCore *tz, const uint8_t *seed, int len);
void trustzone_isolate_partition(TrustZoneCore *tz, int partition_id);

#endif
