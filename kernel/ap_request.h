#ifndef VIBE_KERNEL_AP_REQUEST_H
#define VIBE_KERNEL_AP_REQUEST_H

#include "efi.h"

#define AP_REQUEST_SERVICE_PING 0x41502d5356430001ULL
#define AP_REQUEST_INTERFACE_PING 0x41502d4946430001ULL
#define AP_REQUEST_SERVICE_COUNTER 0x41502d5356430002ULL
#define AP_REQUEST_INTERFACE_COUNTER_INCREMENT 0x41502d4946430002ULL
#define AP_REQUEST_OUTBOX_CAPACITY 4U

enum {
    AP_REQUEST_STATUS_EMPTY = 0,
    AP_REQUEST_STATUS_PENDING = 1,
    AP_REQUEST_STATUS_RUNNING = 2,
    AP_REQUEST_STATUS_DONE = 3,
    AP_REQUEST_STATUS_TIMEOUT = 4,
    AP_REQUEST_STATUS_BAD_OP = 5,
    AP_REQUEST_STATUS_FAULT = 6,
    AP_REQUEST_STATUS_SKIPPED = 7,
};

enum {
    AP_REQUEST_OP_NONE = 0,
    AP_REQUEST_OP_PING = 1,
    AP_REQUEST_OP_COUNTER = 2,
};

typedef struct {
    volatile UINT32 source_cpu;
    volatile UINT32 target_cpu;
    volatile UINT32 opcode;
    volatile UINT32 sequence;
    volatile UINT64 service_id;
    volatile UINT64 interface_id;
    volatile UINT64 id_high;
    volatile UINT64 id_low;
    volatile UINT64 parent_id_high;
    volatile UINT64 parent_id_low;
    volatile UINT64 reply_service_id;
    volatile UINT64 reply_interface_id;
    volatile UINT64 payload_addr;
    volatile UINT32 payload_len;
    volatile UINT32 flags;
} ap_request_header_t;

typedef struct {
    volatile UINT32 result_code;
    volatile UINT32 fault_code;
    volatile UINT64 request_id_high;
    volatile UINT64 request_id_low;
    volatile UINT64 result_cs;
    volatile UINT64 result_tr;
} ap_reply_header_t;

typedef struct {
    volatile UINT32 handled_count;
    volatile UINT32 wait_loops;
} ap_request_metrics_t;

typedef struct {
    UINT64 parent_id_high;
    UINT64 parent_id_low;
    UINT64 reply_service_id;
    UINT64 reply_interface_id;
    UINT64 payload_addr;
    UINT32 payload_len;
    UINT32 flags;
} ap_request_envelope_t;

typedef struct {
    UINT32 opcode;
    UINT64 service_id;
    UINT64 interface_id;
    UINT32 sequence;
    ap_request_envelope_t envelope;
} ap_request_plan_t;

typedef struct {
    UINT32 count;
    ap_request_plan_t entries[AP_REQUEST_OUTBOX_CAPACITY];
} ap_request_outbox_t;

typedef struct {
    volatile UINT32 state;
    ap_request_header_t request;
    ap_reply_header_t reply;
    ap_request_metrics_t metrics;
} ap_request_slot_t;

void reset_ap_request_slot(ap_request_slot_t *slot);
void prepare_ap_request_slot(ap_request_slot_t *slot, UINT32 target_cpu, UINT32 opcode,
                             UINT64 service_id, UINT64 interface_id, UINT32 sequence);
void prepare_ap_request_slot_with_envelope(ap_request_slot_t *slot, UINT32 target_cpu,
                                           UINT32 opcode, UINT64 service_id,
                                           UINT64 interface_id, UINT32 sequence,
                                           const ap_request_envelope_t *envelope);
void prepare_ap_request_slot_from_plan(ap_request_slot_t *slot, UINT32 target_cpu,
                                       const ap_request_plan_t *plan);
void copy_ap_request_slot(ap_request_slot_t *dst, const ap_request_slot_t *src);
void reset_ap_request_outbox(ap_request_outbox_t *outbox);
int append_ap_request_outbox(ap_request_outbox_t *outbox, const ap_request_plan_t *plan);
int complete_ap_request_done_with_outbox(ap_request_slot_t *slot,
                                         ap_request_outbox_t *committed,
                                         const ap_request_outbox_t *scratch);

#endif
