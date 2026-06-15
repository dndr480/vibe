#include "ap_request.h"

#define AP_REQUEST_ID_HIGH_DEFAULT 0x41502d50494e4721ULL

void reset_ap_request_slot(ap_request_slot_t *slot) {
    slot->state = AP_REQUEST_STATUS_EMPTY;
    slot->request.source_cpu = 0;
    slot->request.target_cpu = 0;
    slot->request.opcode = AP_REQUEST_OP_NONE;
    slot->request.sequence = 0;
    slot->request.service_id = 0;
    slot->request.interface_id = 0;
    slot->request.id_high = 0;
    slot->request.id_low = 0;
    slot->request.parent_id_high = 0;
    slot->request.parent_id_low = 0;
    slot->request.reply_service_id = 0;
    slot->request.reply_interface_id = 0;
    slot->request.payload_addr = 0;
    slot->request.payload_len = 0;
    slot->request.flags = 0;
    slot->reply.result_code = 0;
    slot->reply.fault_code = 0;
    slot->reply.request_id_high = 0;
    slot->reply.request_id_low = 0;
    slot->reply.result_cs = 0;
    slot->reply.result_tr = 0;
    slot->metrics.handled_count = 0;
    slot->metrics.wait_loops = 0;
}

void prepare_ap_request_slot_with_envelope(ap_request_slot_t *slot, UINT32 target_cpu,
                                           UINT32 opcode, UINT64 service_id,
                                           UINT64 interface_id, UINT32 sequence,
                                           const ap_request_envelope_t *envelope) {
    slot->metrics.handled_count = 0;
    slot->request.source_cpu = 0;
    slot->request.target_cpu = target_cpu;
    slot->request.opcode = opcode;
    slot->request.sequence = sequence;
    slot->request.service_id = service_id;
    slot->request.interface_id = interface_id;
    slot->request.id_high = AP_REQUEST_ID_HIGH_DEFAULT;
    slot->request.id_low = sequence;
    slot->request.parent_id_high = envelope ? envelope->parent_id_high : 0;
    slot->request.parent_id_low = envelope ? envelope->parent_id_low : 0;
    slot->request.reply_service_id = envelope ? envelope->reply_service_id : 0;
    slot->request.reply_interface_id = envelope ? envelope->reply_interface_id : 0;
    slot->request.payload_addr = envelope ? envelope->payload_addr : 0;
    slot->request.payload_len = envelope ? envelope->payload_len : 0;
    slot->request.flags = envelope ? envelope->flags : 0;
}

void prepare_ap_request_slot(ap_request_slot_t *slot, UINT32 target_cpu, UINT32 opcode,
                             UINT64 service_id, UINT64 interface_id, UINT32 sequence) {
    prepare_ap_request_slot_with_envelope(slot, target_cpu, opcode, service_id, interface_id,
                                          sequence, 0);
}

void prepare_ap_request_slot_from_plan(ap_request_slot_t *slot, UINT32 target_cpu,
                                       const ap_request_plan_t *plan) {
    prepare_ap_request_slot_with_envelope(slot, target_cpu, plan->opcode, plan->service_id,
                                          plan->interface_id, plan->sequence,
                                          &plan->envelope);
}

void copy_ap_request_slot(ap_request_slot_t *dst, const ap_request_slot_t *src) {
    dst->state = src->state;
    dst->request.source_cpu = src->request.source_cpu;
    dst->request.target_cpu = src->request.target_cpu;
    dst->request.opcode = src->request.opcode;
    dst->request.sequence = src->request.sequence;
    dst->request.service_id = src->request.service_id;
    dst->request.interface_id = src->request.interface_id;
    dst->request.id_high = src->request.id_high;
    dst->request.id_low = src->request.id_low;
    dst->request.parent_id_high = src->request.parent_id_high;
    dst->request.parent_id_low = src->request.parent_id_low;
    dst->request.reply_service_id = src->request.reply_service_id;
    dst->request.reply_interface_id = src->request.reply_interface_id;
    dst->request.payload_addr = src->request.payload_addr;
    dst->request.payload_len = src->request.payload_len;
    dst->request.flags = src->request.flags;
    dst->reply.result_code = src->reply.result_code;
    dst->reply.fault_code = src->reply.fault_code;
    dst->reply.request_id_high = src->reply.request_id_high;
    dst->reply.request_id_low = src->reply.request_id_low;
    dst->reply.result_cs = src->reply.result_cs;
    dst->reply.result_tr = src->reply.result_tr;
    dst->metrics.handled_count = src->metrics.handled_count;
    dst->metrics.wait_loops = src->metrics.wait_loops;
}

void reset_ap_request_outbox(ap_request_outbox_t *outbox) {
    if (!outbox) {
        return;
    }

    outbox->count = 0;
    for (UINTN i = 0; i < AP_REQUEST_OUTBOX_CAPACITY; i++) {
        ap_request_plan_t *entry = &outbox->entries[i];
        entry->opcode = 0;
        entry->service_id = 0;
        entry->interface_id = 0;
        entry->sequence = 0;
        entry->envelope.parent_id_high = 0;
        entry->envelope.parent_id_low = 0;
        entry->envelope.reply_service_id = 0;
        entry->envelope.reply_interface_id = 0;
        entry->envelope.payload_addr = 0;
        entry->envelope.payload_len = 0;
        entry->envelope.flags = 0;
    }
}

int append_ap_request_outbox(ap_request_outbox_t *outbox, const ap_request_plan_t *plan) {
    if (!outbox || !plan || outbox->count >= AP_REQUEST_OUTBOX_CAPACITY) {
        return 0;
    }

    outbox->entries[outbox->count] = *plan;
    outbox->count++;
    return 1;
}
