#include "ap_services.h"

#define AP_SERVICE_OWNER_AP0_CONTEXT_INDEX 0U

static UINT16 read_cs(void) {
    UINT16 cs;
    __asm__ __volatile__("movw %%cs, %0" : "=r"(cs));
    return cs;
}

static UINT16 read_tr(void) {
    UINT16 tr;
    __asm__ __volatile__("str %0" : "=r"(tr));
    return tr;
}

static void run_ap_request_test_hook(int hang_enabled) {
#if VIBE_AP_REQUEST_FAULT_TEST == VIBE_AP_REQUEST_FAULT_TEST_UD2
    (void)hang_enabled;
    __asm__ __volatile__("ud2" : : : "memory");
#elif VIBE_AP_REQUEST_FAULT_TEST == VIBE_AP_REQUEST_FAULT_TEST_HANG
    if (!hang_enabled) {
        return;
    }

    __asm__ __volatile__("cli" : : : "memory");
    for (;;) {
        __asm__ __volatile__("pause" : : : "memory");
    }
#else
    (void)hang_enabled;
#endif
}

static UINT32 note_ap_request_handled(ap_service_context_t *ctx) {
    *ctx->request_handled_count = *ctx->request_handled_count + 1U;
    return *ctx->request_handled_count;
}

static void ap_handle_ping_request(ap_service_context_t *ctx, ap_request_slot_t *slot) {
    run_ap_request_test_hook(1);
    slot->reply.result_cs = read_cs();
    slot->reply.result_tr = read_tr();
    slot->reply.result_code = 0;
    slot->reply.fault_code = 0;
    slot->metrics.handled_count = note_ap_request_handled(ctx);
}

static void ap_handle_counter_increment(ap_service_context_t *ctx, ap_request_slot_t *slot) {
    run_ap_request_test_hook(0);
    *ctx->counter_value = *ctx->counter_value + 1U;
    slot->reply.result_cs = read_cs();
    slot->reply.result_tr = read_tr();
    slot->reply.result_code = *ctx->counter_value;
    slot->reply.fault_code = 0;
    slot->metrics.handled_count = note_ap_request_handled(ctx);
}

static const ap_service_registry_entry_t ap_service_registry[] = {
    {AP_REQUEST_SERVICE_PING, AP_REQUEST_INTERFACE_PING,
     AP_SERVICE_OWNER_AP0_CONTEXT_INDEX, ap_handle_ping_request},
    {AP_REQUEST_SERVICE_COUNTER, AP_REQUEST_INTERFACE_COUNTER_INCREMENT,
     AP_SERVICE_OWNER_AP0_CONTEXT_INDEX, ap_handle_counter_increment},
};

const ap_service_registry_entry_t *find_ap_service(UINT64 service_id, UINT64 interface_id) {
    for (UINTN i = 0; i < sizeof(ap_service_registry) / sizeof(ap_service_registry[0]); i++) {
        if (ap_service_registry[i].service_id == service_id &&
            ap_service_registry[i].interface_id == interface_id) {
            return &ap_service_registry[i];
        }
    }
    return 0;
}

ap_service_lookup_status_t classify_ap_service_lookup(UINT64 service_id, UINT64 interface_id) {
    int service_seen = 0;

    for (UINTN i = 0; i < sizeof(ap_service_registry) / sizeof(ap_service_registry[0]); i++) {
        if (ap_service_registry[i].service_id != service_id) {
            continue;
        }
        service_seen = 1;
        if (ap_service_registry[i].interface_id == interface_id) {
            return AP_SERVICE_LOOKUP_OK;
        }
    }

    return service_seen ? AP_SERVICE_LOOKUP_UNKNOWN_INTERFACE :
                          AP_SERVICE_LOOKUP_UNKNOWN_SERVICE;
}

ap_service_lookup_status_t ap_service_owner_context_index(UINT64 service_id, UINT64 interface_id,
                                                          UINT32 *owner_context_index) {
    const ap_service_registry_entry_t *entry = find_ap_service(service_id, interface_id);
    if (entry) {
        if (owner_context_index) {
            *owner_context_index = entry->owner_context_index;
        }
        return AP_SERVICE_LOOKUP_OK;
    }
    return classify_ap_service_lookup(service_id, interface_id);
}

ap_request_handler_t find_ap_request_handler(UINT64 service_id, UINT64 interface_id) {
    const ap_service_registry_entry_t *entry = find_ap_service(service_id, interface_id);
    return entry ? entry->handler : 0;
}

UINT32 ap_dispatch_miss_result_code(UINT64 service_id, UINT64 interface_id) {
    if (classify_ap_service_lookup(service_id, interface_id) ==
        AP_SERVICE_LOOKUP_UNKNOWN_SERVICE) {
        return (UINT32)service_id;
    }
    return (UINT32)interface_id;
}
