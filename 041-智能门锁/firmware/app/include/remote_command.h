#ifndef SMART_LOCK_REMOTE_COMMAND_H
#define SMART_LOCK_REMOTE_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sha1.h"

typedef enum {
    REMOTE_ACTION_UNLOCK = 1,
    REMOTE_ACTION_LOCK = 2,
    REMOTE_ACTION_CLEAR_LOCKOUT = 3,
    REMOTE_ACTION_ISSUE_VISITOR = 4
} remote_action_t;

typedef struct {
    uint64_t request_id;
    uint64_t issued_at;
    uint64_t expires_at;
    remote_action_t action;
    char visitor_code[12];
    uint8_t visitor_code_length;
    uint8_t visitor_uses;
    uint8_t authentication_tag[SHA1_DIGEST_SIZE];
} remote_command_t;

typedef enum {
    REMOTE_COMMAND_ACCEPTED = 0,
    REMOTE_COMMAND_BAD_TAG,
    REMOTE_COMMAND_EXPIRED,
    REMOTE_COMMAND_FROM_FUTURE,
    REMOTE_COMMAND_REPLAYED,
    REMOTE_COMMAND_INVALID
} remote_command_result_t;

bool remote_command_sign(remote_command_t *command,
                         const uint8_t *device_secret,
                         size_t device_secret_length);

remote_command_result_t remote_command_verify(
    const remote_command_t *command,
    const uint8_t *device_secret,
    size_t device_secret_length,
    uint64_t now,
    uint32_t maximum_future_skew,
    uint64_t *last_accepted_request_id);

#endif
