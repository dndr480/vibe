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

ap_request_handler_t find_ap_request_handler(UINT64 service_id, UINT64 interface_id);
UINT32 ap_dispatch_miss_result_code(UINT64 service_id, UINT64 interface_id);

#endif
