#ifndef VIBE_KERNEL_AP_SERVICES_H
#define VIBE_KERNEL_AP_SERVICES_H

#include "ap_request.h"

#ifndef VIBE_AP_REQUEST_FAULT_TEST
#define VIBE_AP_REQUEST_FAULT_TEST 0
#endif

#define VIBE_AP_REQUEST_FAULT_TEST_NONE 0
#define VIBE_AP_REQUEST_FAULT_TEST_UD2 1
#define VIBE_AP_REQUEST_FAULT_TEST_HANG 2

typedef struct {
    volatile UINT32 *request_handled_count;
    volatile UINT32 *counter_value;
} ap_service_context_t;

typedef void (*ap_request_handler_t)(ap_service_context_t *ctx, ap_request_slot_t *slot);

typedef enum {
    AP_SERVICE_LOOKUP_OK = 0,
    AP_SERVICE_LOOKUP_UNKNOWN_SERVICE = 1,
    AP_SERVICE_LOOKUP_UNKNOWN_INTERFACE = 2,
} ap_service_lookup_status_t;

typedef struct {
    UINT64 service_id;
    UINT64 interface_id;
    UINT32 owner_context_index;
    ap_request_handler_t handler;
} ap_service_registry_entry_t;

const ap_service_registry_entry_t *find_ap_service(UINT64 service_id, UINT64 interface_id);
ap_service_lookup_status_t classify_ap_service_lookup(UINT64 service_id, UINT64 interface_id);
ap_request_handler_t find_ap_request_handler(UINT64 service_id, UINT64 interface_id);
UINT32 ap_dispatch_miss_result_code(UINT64 service_id, UINT64 interface_id);

#endif
