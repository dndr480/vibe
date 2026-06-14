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

static void test_registry_lookup(void) {
    const UINT64 unknown_service = 0x41502d53564300ffULL;
    const UINT64 unknown_interface = 0x41502d49464300ffULL;

    const ap_service_registry_entry_t *ping =
        find_ap_service(AP_REQUEST_SERVICE_PING, AP_REQUEST_INTERFACE_PING);
    const ap_service_registry_entry_t *counter =
        find_ap_service(AP_REQUEST_SERVICE_COUNTER,
                        AP_REQUEST_INTERFACE_COUNTER_INCREMENT);

    check_int(ping != 0, "PING registry lookup");
    check_int(counter != 0, "COUNTER registry lookup");
    if (ping) {
        check_int(ping->service_id == AP_REQUEST_SERVICE_PING,
                  "PING registry service id");
        check_int(ping->interface_id == AP_REQUEST_INTERFACE_PING,
                  "PING registry interface id");
        check_int(ping->owner_context_index == 0,
                  "PING owner is AP0 context");
        check_int(ping->handler == find_ap_request_handler(AP_REQUEST_SERVICE_PING,
                                                           AP_REQUEST_INTERFACE_PING),
                  "PING registry handler matches compatibility lookup");
    }
    if (counter) {
        check_int(counter->service_id == AP_REQUEST_SERVICE_COUNTER,
                  "COUNTER registry service id");
        check_int(counter->interface_id == AP_REQUEST_INTERFACE_COUNTER_INCREMENT,
                  "COUNTER registry interface id");
        check_int(counter->owner_context_index == 0,
                  "COUNTER owner is AP0 context");
        check_int(counter->handler ==
                      find_ap_request_handler(AP_REQUEST_SERVICE_COUNTER,
                                              AP_REQUEST_INTERFACE_COUNTER_INCREMENT),
                  "COUNTER registry handler matches compatibility lookup");
    }

    check_int(find_ap_service(AP_REQUEST_SERVICE_PING, unknown_interface) == 0,
              "known service unknown interface has no registry entry");
    check_int(find_ap_service(unknown_service, AP_REQUEST_INTERFACE_PING) == 0,
              "unknown service has no registry entry");
    check_int(classify_ap_service_lookup(AP_REQUEST_SERVICE_PING,
                                         AP_REQUEST_INTERFACE_PING) ==
                  AP_SERVICE_LOOKUP_OK,
              "PING registry status OK");
    check_int(classify_ap_service_lookup(AP_REQUEST_SERVICE_COUNTER,
                                         AP_REQUEST_INTERFACE_COUNTER_INCREMENT) ==
                  AP_SERVICE_LOOKUP_OK,
              "COUNTER registry status OK");
    check_int(classify_ap_service_lookup(AP_REQUEST_SERVICE_PING, unknown_interface) ==
                  AP_SERVICE_LOOKUP_UNKNOWN_INTERFACE,
              "known service unknown interface status");
    check_int(classify_ap_service_lookup(unknown_service, AP_REQUEST_INTERFACE_PING) ==
                  AP_SERVICE_LOOKUP_UNKNOWN_SERVICE,
              "unknown service status");
}

static void test_miss_result_code(void) {
    const UINT64 unknown_service = 0x41502d53564300ffULL;

    check_int(ap_dispatch_miss_result_code(AP_REQUEST_SERVICE_PING, 0x12345678ULL) ==
                  0x12345678U,
              "known service miss returns interface id");
    check_int(ap_dispatch_miss_result_code(AP_REQUEST_SERVICE_COUNTER, 0x87654321ULL) ==
                  0x87654321U,
              "known counter service miss returns interface id");
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

static void test_handlers_use_explicit_context(void) {
    volatile UINT32 ping_handled_count_a = 5;
    volatile UINT32 ping_counter_value_a = 50;
    volatile UINT32 ping_handled_count_b = 7;
    volatile UINT32 ping_counter_value_b = 70;
    volatile UINT32 handled_count_a = 10;
    volatile UINT32 counter_value_a = 100;
    volatile UINT32 handled_count_b = 20;
    volatile UINT32 counter_value_b = 200;
    ap_service_context_t ping_ctx_a = {&ping_handled_count_a, &ping_counter_value_a};
    ap_service_context_t ping_ctx_b = {&ping_handled_count_b, &ping_counter_value_b};
    ap_service_context_t ctx_a = {&handled_count_a, &counter_value_a};
    ap_service_context_t ctx_b = {&handled_count_b, &counter_value_b};
    ap_request_slot_t ping_slot_a1;
    ap_request_slot_t ping_slot_b;
    ap_request_slot_t ping_slot_a2;
    ap_request_slot_t slot_a1;
    ap_request_slot_t slot_b;
    ap_request_slot_t slot_a2;

    ap_request_handler_t ping_handler = find_ap_request_handler(AP_REQUEST_SERVICE_PING,
                                                                AP_REQUEST_INTERFACE_PING);
    ap_request_handler_t counter_handler =
        find_ap_request_handler(AP_REQUEST_SERVICE_COUNTER,
                                AP_REQUEST_INTERFACE_COUNTER_INCREMENT);
    check_int(ping_handler != 0, "PING handler available for context isolation");
    check_int(counter_handler != 0, "COUNTER handler available for context isolation");
    if (!ping_handler || !counter_handler) {
        return;
    }

    reset_slot(&ping_slot_a1);
    reset_slot(&ping_slot_b);
    reset_slot(&ping_slot_a2);
    reset_slot(&slot_a1);
    reset_slot(&slot_b);
    reset_slot(&slot_a2);

    ping_handler(&ping_ctx_a, &ping_slot_a1);
    ping_handler(&ping_ctx_b, &ping_slot_b);
    ping_handler(&ping_ctx_a, &ping_slot_a2);
    counter_handler(&ctx_a, &slot_a1);
    counter_handler(&ctx_b, &slot_b);
    counter_handler(&ctx_a, &slot_a2);

    check_int(ping_handled_count_a == 7, "PING context A handled count is independent");
    check_int(ping_counter_value_a == 50, "PING context A counter is unchanged");
    check_int(ping_handled_count_b == 8, "PING context B handled count is independent");
    check_int(ping_counter_value_b == 70, "PING context B counter is unchanged");
    check_int(ping_slot_a1.metrics.handled_count == 6,
              "first PING context A metric uses context A");
    check_int(ping_slot_b.metrics.handled_count == 8, "PING context B metric uses context B");
    check_int(ping_slot_a2.metrics.handled_count == 7,
              "second PING context A metric uses context A");

    check_int(handled_count_a == 12, "context A handled count is independent");
    check_int(counter_value_a == 102, "context A counter is independent");
    check_int(handled_count_b == 21, "context B handled count is independent");
    check_int(counter_value_b == 201, "context B counter is independent");
    check_int(slot_a1.metrics.handled_count == 11, "first context A metric uses context A");
    check_int(slot_a1.reply.result_code == 101, "first context A result uses context A");
    check_int(slot_b.metrics.handled_count == 21, "context B metric uses context B");
    check_int(slot_b.reply.result_code == 201, "context B result uses context B");
    check_int(slot_a2.metrics.handled_count == 12, "second context A metric uses context A");
    check_int(slot_a2.reply.result_code == 102, "second context A result uses context A");
}

int main(void) {
    test_lookup();
    test_registry_lookup();
    test_miss_result_code();
    test_ping_handler();
    test_counter_handler();
    test_handlers_use_explicit_context();

    if (failures) {
        printf("ap_services: %d failure(s)\n", failures);
        return 1;
    }

    printf("ap_services: ok\n");
    return 0;
}
