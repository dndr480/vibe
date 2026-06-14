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
    reset_ap_request_slot(slot);
    slot->request.target_cpu = 1;
    slot->request.id_high = 0x1111222233334444ULL;
    slot->request.id_low = 0x5555666677778888ULL;
    slot->reply.result_code = 0xffffffffU;
    slot->reply.fault_code = 0xffffffffU;
    slot->reply.result_cs = 0xffffffffffffffffULL;
    slot->reply.result_tr = 0xffffffffffffffffULL;
}

static void fill_slot(ap_request_slot_t *slot) {
    slot->state = AP_REQUEST_STATUS_RUNNING;
    slot->request.source_cpu = 9;
    slot->request.target_cpu = 7;
    slot->request.opcode = AP_REQUEST_OP_COUNTER;
    slot->request.sequence = 1234;
    slot->request.service_id = AP_REQUEST_SERVICE_COUNTER;
    slot->request.interface_id = AP_REQUEST_INTERFACE_COUNTER_INCREMENT;
    slot->request.id_high = 0x1111222233334444ULL;
    slot->request.id_low = 0x5555666677778888ULL;
    slot->request.parent_id_high = 0x9999aaaabbbbccccULL;
    slot->request.parent_id_low = 0xddddeeeeffff0001ULL;
    slot->request.reply_service_id = 0x2222333344445555ULL;
    slot->request.reply_interface_id = 0x6666777788889999ULL;
    slot->request.payload_addr = 0xaaaabbbbccccddddULL;
    slot->request.payload_len = 4096;
    slot->request.flags = 0x13579bdfU;
    slot->reply.result_code = 77;
    slot->reply.fault_code = 88;
    slot->reply.request_id_high = 0x1011121314151617ULL;
    slot->reply.request_id_low = 0x18191a1b1c1d1e1fULL;
    slot->reply.result_cs = 0x20;
    slot->reply.result_tr = 0x28;
    slot->metrics.handled_count = 99;
    slot->metrics.wait_loops = 100;
}

static void test_request_lifecycle(void) {
    const ap_request_envelope_t envelope = {
        .parent_id_high = 0x0123456789abcdefULL,
        .parent_id_low = 0xfedcba9876543210ULL,
        .reply_service_id = 0x1111222233334444ULL,
        .reply_interface_id = 0x5555666677778888ULL,
        .payload_addr = 0x9999aaaabbbbccccULL,
        .payload_len = 8192,
        .flags = 0x2468ace0U,
    };
    ap_request_slot_t slot;
    ap_request_slot_t src;
    ap_request_slot_t dst;

    fill_slot(&slot);
    reset_ap_request_slot(&slot);
    check_int(slot.state == AP_REQUEST_STATUS_EMPTY, "reset clears state");
    check_int(slot.request.source_cpu == 0, "reset clears source CPU");
    check_int(slot.request.target_cpu == 0, "reset clears target CPU");
    check_int(slot.request.opcode == AP_REQUEST_OP_NONE, "reset clears opcode");
    check_int(slot.request.sequence == 0, "reset clears sequence");
    check_int(slot.request.service_id == 0, "reset clears service");
    check_int(slot.request.interface_id == 0, "reset clears interface");
    check_int(slot.request.id_high == 0, "reset clears request id high");
    check_int(slot.request.id_low == 0, "reset clears request id low");
    check_int(slot.request.parent_id_high == 0, "reset clears parent id high");
    check_int(slot.request.parent_id_low == 0, "reset clears parent id low");
    check_int(slot.request.reply_service_id == 0, "reset clears reply service");
    check_int(slot.request.reply_interface_id == 0, "reset clears reply interface");
    check_int(slot.request.payload_addr == 0, "reset clears payload address");
    check_int(slot.request.payload_len == 0, "reset clears payload length");
    check_int(slot.request.flags == 0, "reset clears flags");
    check_int(slot.reply.result_code == 0, "reset clears result code");
    check_int(slot.reply.fault_code == 0, "reset clears fault code");
    check_int(slot.reply.request_id_high == 0, "reset clears reply request id high");
    check_int(slot.reply.request_id_low == 0, "reset clears reply request id low");
    check_int(slot.reply.result_cs == 0, "reset clears result CS");
    check_int(slot.reply.result_tr == 0, "reset clears result TR");
    check_int(slot.metrics.handled_count == 0, "reset clears handled metric");
    check_int(slot.metrics.wait_loops == 0, "reset clears wait metric");

    fill_slot(&slot);
    prepare_ap_request_slot(&slot, 3, AP_REQUEST_OP_PING, AP_REQUEST_SERVICE_PING,
                            AP_REQUEST_INTERFACE_PING, 42);
    check_int(slot.state == AP_REQUEST_STATUS_RUNNING, "prepare preserves state");
    check_int(slot.reply.result_code == 77, "prepare preserves reply result");
    check_int(slot.reply.fault_code == 88, "prepare preserves reply fault");
    check_int(slot.metrics.wait_loops == 100, "prepare preserves wait metric");
    check_int(slot.metrics.handled_count == 0, "prepare clears handled metric");
    check_int(slot.request.source_cpu == 0, "prepare uses BSP source CPU");
    check_int(slot.request.target_cpu == 3, "prepare sets target CPU");
    check_int(slot.request.opcode == AP_REQUEST_OP_PING, "prepare sets opcode");
    check_int(slot.request.sequence == 42, "prepare sets sequence");
    check_int(slot.request.service_id == AP_REQUEST_SERVICE_PING, "prepare sets service");
    check_int(slot.request.interface_id == AP_REQUEST_INTERFACE_PING,
              "prepare sets interface");
    check_int(slot.request.id_high == 0x41502d50494e4721ULL,
              "prepare sets request id high");
    check_int(slot.request.id_low == 42, "prepare sets request id low from sequence");
    check_int(slot.request.parent_id_high == 0, "prepare defaults parent id high");
    check_int(slot.request.parent_id_low == 0, "prepare defaults parent id low");
    check_int(slot.request.reply_service_id == 0, "prepare defaults reply service");
    check_int(slot.request.reply_interface_id == 0, "prepare defaults reply interface");
    check_int(slot.request.payload_addr == 0, "prepare defaults payload address");
    check_int(slot.request.payload_len == 0, "prepare defaults payload length");
    check_int(slot.request.flags == 0, "prepare defaults flags");

    prepare_ap_request_slot_with_envelope(&slot, 4, AP_REQUEST_OP_COUNTER,
                                          AP_REQUEST_SERVICE_COUNTER,
                                          AP_REQUEST_INTERFACE_COUNTER_INCREMENT, 43,
                                          &envelope);
    check_int(slot.request.target_cpu == 4, "prepare envelope sets target CPU");
    check_int(slot.request.opcode == AP_REQUEST_OP_COUNTER, "prepare envelope sets opcode");
    check_int(slot.request.sequence == 43, "prepare envelope sets sequence");
    check_int(slot.request.service_id == AP_REQUEST_SERVICE_COUNTER,
              "prepare envelope sets service");
    check_int(slot.request.interface_id == AP_REQUEST_INTERFACE_COUNTER_INCREMENT,
              "prepare envelope sets interface");
    check_int(slot.request.id_high == 0x41502d50494e4721ULL,
              "prepare envelope sets request id high");
    check_int(slot.request.id_low == 43, "prepare envelope sets request id low");
    check_int(slot.request.parent_id_high == envelope.parent_id_high,
              "prepare envelope sets parent id high");
    check_int(slot.request.parent_id_low == envelope.parent_id_low,
              "prepare envelope sets parent id low");
    check_int(slot.request.reply_service_id == envelope.reply_service_id,
              "prepare envelope sets reply service");
    check_int(slot.request.reply_interface_id == envelope.reply_interface_id,
              "prepare envelope sets reply interface");
    check_int(slot.request.payload_addr == envelope.payload_addr,
              "prepare envelope sets payload address");
    check_int(slot.request.payload_len == envelope.payload_len,
              "prepare envelope sets payload length");
    check_int(slot.request.flags == envelope.flags, "prepare envelope sets flags");

    prepare_ap_request_slot_with_envelope(&slot, 5, AP_REQUEST_OP_PING,
                                          AP_REQUEST_SERVICE_PING,
                                          AP_REQUEST_INTERFACE_PING, 44, 0);
    check_int(slot.request.parent_id_high == 0,
              "prepare envelope NULL defaults parent id high");
    check_int(slot.request.parent_id_low == 0,
              "prepare envelope NULL defaults parent id low");
    check_int(slot.request.reply_service_id == 0,
              "prepare envelope NULL defaults reply service");
    check_int(slot.request.reply_interface_id == 0,
              "prepare envelope NULL defaults reply interface");
    check_int(slot.request.payload_addr == 0,
              "prepare envelope NULL defaults payload address");
    check_int(slot.request.payload_len == 0,
              "prepare envelope NULL defaults payload length");
    check_int(slot.request.flags == 0, "prepare envelope NULL defaults flags");

    const ap_request_plan_t plan = {
        .opcode = AP_REQUEST_OP_COUNTER,
        .service_id = AP_REQUEST_SERVICE_COUNTER,
        .interface_id = AP_REQUEST_INTERFACE_COUNTER_INCREMENT,
        .sequence = 45,
        .envelope = envelope,
    };
    prepare_ap_request_slot_from_plan(&slot, 6, &plan);
    check_int(slot.request.target_cpu == 6, "prepare plan sets target CPU");
    check_int(slot.request.opcode == AP_REQUEST_OP_COUNTER, "prepare plan sets opcode");
    check_int(slot.request.sequence == 45, "prepare plan sets sequence");
    check_int(slot.request.service_id == AP_REQUEST_SERVICE_COUNTER,
              "prepare plan sets service");
    check_int(slot.request.interface_id == AP_REQUEST_INTERFACE_COUNTER_INCREMENT,
              "prepare plan sets interface");
    check_int(slot.request.parent_id_high == envelope.parent_id_high,
              "prepare plan sets parent id high");
    check_int(slot.request.parent_id_low == envelope.parent_id_low,
              "prepare plan sets parent id low");
    check_int(slot.request.reply_service_id == envelope.reply_service_id,
              "prepare plan sets reply service");
    check_int(slot.request.reply_interface_id == envelope.reply_interface_id,
              "prepare plan sets reply interface");
    check_int(slot.request.payload_addr == envelope.payload_addr,
              "prepare plan sets payload address");
    check_int(slot.request.payload_len == envelope.payload_len,
              "prepare plan sets payload length");
    check_int(slot.request.flags == envelope.flags, "prepare plan sets flags");

    fill_slot(&src);
    reset_ap_request_slot(&dst);
    copy_ap_request_slot(&dst, &src);
    check_int(dst.state == src.state, "copy preserves state");
    check_int(dst.request.source_cpu == src.request.source_cpu, "copy preserves source CPU");
    check_int(dst.request.target_cpu == src.request.target_cpu, "copy preserves target CPU");
    check_int(dst.request.opcode == src.request.opcode, "copy preserves opcode");
    check_int(dst.request.sequence == src.request.sequence, "copy preserves sequence");
    check_int(dst.request.service_id == src.request.service_id, "copy preserves service");
    check_int(dst.request.interface_id == src.request.interface_id,
              "copy preserves interface");
    check_int(dst.request.id_high == src.request.id_high, "copy preserves request id high");
    check_int(dst.request.id_low == src.request.id_low, "copy preserves request id low");
    check_int(dst.request.parent_id_high == src.request.parent_id_high,
              "copy preserves parent id high");
    check_int(dst.request.parent_id_low == src.request.parent_id_low,
              "copy preserves parent id low");
    check_int(dst.request.reply_service_id == src.request.reply_service_id,
              "copy preserves reply service");
    check_int(dst.request.reply_interface_id == src.request.reply_interface_id,
              "copy preserves reply interface");
    check_int(dst.request.payload_addr == src.request.payload_addr,
              "copy preserves payload address");
    check_int(dst.request.payload_len == src.request.payload_len,
              "copy preserves payload length");
    check_int(dst.request.flags == src.request.flags, "copy preserves flags");
    check_int(dst.reply.result_code == src.reply.result_code, "copy preserves result");
    check_int(dst.reply.fault_code == src.reply.fault_code, "copy preserves fault");
    check_int(dst.reply.request_id_high == src.reply.request_id_high,
              "copy preserves reply request id high");
    check_int(dst.reply.request_id_low == src.reply.request_id_low,
              "copy preserves reply request id low");
    check_int(dst.reply.result_cs == src.reply.result_cs, "copy preserves result CS");
    check_int(dst.reply.result_tr == src.reply.result_tr, "copy preserves result TR");
    check_int(dst.metrics.handled_count == src.metrics.handled_count,
              "copy preserves handled metric");
    check_int(dst.metrics.wait_loops == src.metrics.wait_loops,
              "copy preserves wait metric");
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

static void test_service_owner_lookup(void) {
    const UINT64 unknown_service = 0x41502d53564300ffULL;
    const UINT64 unknown_interface = 0x41502d49464300ffULL;
    UINT32 owner = 0xffffffffU;

    check_int(ap_service_owner_context_index(AP_REQUEST_SERVICE_PING,
                                             AP_REQUEST_INTERFACE_PING,
                                             &owner) == AP_SERVICE_LOOKUP_OK,
              "PING owner lookup status OK");
    check_int(owner == 0, "PING owner lookup returns AP0 context");

    owner = 0xffffffffU;
    check_int(ap_service_owner_context_index(AP_REQUEST_SERVICE_COUNTER,
                                             AP_REQUEST_INTERFACE_COUNTER_INCREMENT,
                                             &owner) == AP_SERVICE_LOOKUP_OK,
              "COUNTER owner lookup status OK");
    check_int(owner == 0, "COUNTER owner lookup returns AP0 context");

    owner = 0x12345678U;
    check_int(ap_service_owner_context_index(AP_REQUEST_SERVICE_PING, unknown_interface,
                                             &owner) ==
                  AP_SERVICE_LOOKUP_UNKNOWN_INTERFACE,
              "known service unknown interface owner status");
    check_int(owner == 0x12345678U,
              "known service unknown interface leaves owner unchanged");

    owner = 0x87654321U;
    check_int(ap_service_owner_context_index(unknown_service, AP_REQUEST_INTERFACE_PING,
                                             &owner) ==
                  AP_SERVICE_LOOKUP_UNKNOWN_SERVICE,
              "unknown service owner status");
    check_int(owner == 0x87654321U, "unknown service leaves owner unchanged");
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
    test_request_lifecycle();
    test_lookup();
    test_registry_lookup();
    test_service_owner_lookup();
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
