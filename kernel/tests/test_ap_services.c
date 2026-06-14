#include <stdio.h>

#include "../ap_services.h"

static int failures;

static void check_int(int condition, const char *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

static void reset_slot(ap_request_slot_t *slot) {
    slot->state = AP_REQUEST_STATUS_EMPTY;
    slot->request.source_cpu = 0;
    slot->request.target_cpu = 1;
    slot->request.opcode = AP_REQUEST_OP_NONE;
    slot->request.sequence = 0;
    slot->request.service_id = 0;
    slot->request.interface_id = 0;
    slot->request.id_high = 0x1111222233334444ULL;
    slot->request.id_low = 0x5555666677778888ULL;
    slot->reply.result_code = 0xffffffffU;
    slot->reply.fault_code = 0xffffffffU;
    slot->reply.request_id_high = 0;
    slot->reply.request_id_low = 0;
    slot->reply.result_cs = 0xffffffffffffffffULL;
    slot->reply.result_tr = 0xffffffffffffffffULL;
    slot->metrics.handled_count = 0;
    slot->metrics.wait_loops = 0;
}

static void test_lookup(void) {
    check_int(find_ap_request_handler(AP_REQUEST_SERVICE_PING,
                                      AP_REQUEST_INTERFACE_PING) != 0,
              "PING handler lookup");
    check_int(find_ap_request_handler(AP_REQUEST_SERVICE_COUNTER,
                                      AP_REQUEST_INTERFACE_COUNTER_INCREMENT) != 0,
              "COUNTER handler lookup");
    check_int(find_ap_request_handler(AP_REQUEST_SERVICE_PING,
                                      AP_REQUEST_INTERFACE_COUNTER_INCREMENT) == 0,
              "PING service rejects counter interface");
    check_int(find_ap_request_handler(0x41502d53564300ffULL,
                                      AP_REQUEST_INTERFACE_PING) == 0,
              "unknown service lookup fails");
}

static void test_miss_result_code(void) {
    const UINT64 unknown_service = 0x41502d53564300ffULL;

    check_int(ap_dispatch_miss_result_code(AP_REQUEST_SERVICE_PING, 0x12345678ULL) ==
                  0x12345678U,
              "known service miss returns interface id");
    check_int(ap_dispatch_miss_result_code(unknown_service, 0x12345678ULL) ==
                  (UINT32)unknown_service,
              "unknown service miss returns service id");
}

static void test_ping_handler(void) {
    volatile UINT32 handled_count = 0;
    volatile UINT32 counter_value = 41;
    ap_service_context_t ctx = {&handled_count, &counter_value};
    ap_request_slot_t slot;
    reset_slot(&slot);

    ap_request_handler_t handler = find_ap_request_handler(AP_REQUEST_SERVICE_PING,
                                                           AP_REQUEST_INTERFACE_PING);
    check_int(handler != 0, "PING handler available before call");
    if (!handler) {
        return;
    }

    handler(&ctx, &slot);

    check_int(handled_count == 1, "PING increments handled count");
    check_int(counter_value == 41, "PING does not change counter value");
    check_int(slot.reply.result_code == 0, "PING result code is zero");
    check_int(slot.reply.fault_code == 0, "PING fault code is zero");
    check_int(slot.metrics.handled_count == 1, "PING records handled count");
    check_int(slot.reply.result_cs != 0xffffffffffffffffULL, "PING writes CS result");
    check_int(slot.reply.result_tr != 0xffffffffffffffffULL, "PING writes TR result");
}

static void test_counter_handler(void) {
    volatile UINT32 handled_count = 3;
    volatile UINT32 counter_value = 41;
    ap_service_context_t ctx = {&handled_count, &counter_value};
    ap_request_slot_t slot;
    reset_slot(&slot);

    ap_request_handler_t handler =
        find_ap_request_handler(AP_REQUEST_SERVICE_COUNTER,
                                AP_REQUEST_INTERFACE_COUNTER_INCREMENT);
    check_int(handler != 0, "COUNTER handler available before call");
    if (!handler) {
        return;
    }

    handler(&ctx, &slot);

    check_int(handled_count == 4, "COUNTER increments handled count");
    check_int(counter_value == 42, "COUNTER increments counter value");
    check_int(slot.reply.result_code == 42, "COUNTER result code is counter value");
    check_int(slot.reply.fault_code == 0, "COUNTER fault code is zero");
    check_int(slot.metrics.handled_count == 4, "COUNTER records handled count");
    check_int(slot.reply.result_cs != 0xffffffffffffffffULL, "COUNTER writes CS result");
    check_int(slot.reply.result_tr != 0xffffffffffffffffULL, "COUNTER writes TR result");
}

int main(void) {
    test_lookup();
    test_miss_result_code();
    test_ping_handler();
    test_counter_handler();

    if (failures) {
        printf("ap_services: %d failure(s)\n", failures);
        return 1;
    }

    printf("ap_services: ok\n");
    return 0;
}
