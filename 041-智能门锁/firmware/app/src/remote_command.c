#include "remote_command.h"

#include <string.h>

#include "lock_auth.h"

#define COMMAND_CANONICAL_SIZE 39U

static void write_be64(uint8_t *output, uint64_t value)
{
    for (size_t i = 0U; i < 8U; ++i) {
        output[7U - i] = (uint8_t)(value >> (i * 8U));
    }
}

static bool action_valid(remote_action_t action)
{
    return (action == REMOTE_ACTION_UNLOCK) ||
           (action == REMOTE_ACTION_LOCK) ||
           (action == REMOTE_ACTION_CLEAR_LOCKOUT) ||
           (action == REMOTE_ACTION_ISSUE_VISITOR);
}

static bool payload_valid(const remote_command_t *command)
{
    if (command->action != REMOTE_ACTION_ISSUE_VISITOR) {
        return (command->visitor_code_length == 0U) &&
               (command->visitor_uses == 0U);
    }
    if ((command->visitor_code_length < 6U) ||
        (command->visitor_code_length > sizeof(command->visitor_code)) ||
        (command->visitor_uses == 0U)) {
        return false;
    }
    for (uint8_t i = 0U; i < command->visitor_code_length; ++i) {
        if ((command->visitor_code[i] < '0') ||
            (command->visitor_code[i] > '9')) {
            return false;
        }
    }
    return true;
}

static void canonicalize(const remote_command_t *command,
                         uint8_t output[COMMAND_CANONICAL_SIZE])
{
    write_be64(&output[0], command->request_id);
    write_be64(&output[8], command->issued_at);
    write_be64(&output[16], command->expires_at);
    output[24] = (uint8_t)command->action;
    memcpy(&output[25], command->visitor_code, sizeof(command->visitor_code));
    output[37] = command->visitor_code_length;
    output[38] = command->visitor_uses;
}

bool remote_command_sign(remote_command_t *command,
                         const uint8_t *device_secret,
                         size_t device_secret_length)
{
    uint8_t canonical[COMMAND_CANONICAL_SIZE];
    if ((command == NULL) || (device_secret == NULL) ||
        (device_secret_length < 16U) || (!action_valid(command->action)) ||
        (!payload_valid(command)) ||
        (command->request_id == 0U) ||
        (command->expires_at <= command->issued_at)) {
        return false;
    }
    canonicalize(command, canonical);
    hmac_sha1(device_secret, device_secret_length, canonical, sizeof(canonical),
              command->authentication_tag);
    memset(canonical, 0, sizeof(canonical));
    return true;
}

remote_command_result_t remote_command_verify(
    const remote_command_t *command,
    const uint8_t *device_secret,
    size_t device_secret_length,
    uint64_t now,
    uint32_t maximum_future_skew,
    uint64_t *last_accepted_request_id)
{
    uint8_t canonical[COMMAND_CANONICAL_SIZE];
    uint8_t expected_tag[SHA1_DIGEST_SIZE];
    bool tag_matches;

    if ((command == NULL) || (device_secret == NULL) ||
        (device_secret_length < 16U) || (last_accepted_request_id == NULL) ||
        (!action_valid(command->action)) || (!payload_valid(command)) ||
        (command->request_id == 0U) ||
        (command->expires_at <= command->issued_at)) {
        return REMOTE_COMMAND_INVALID;
    }
    if (command->request_id <= *last_accepted_request_id) {
        return REMOTE_COMMAND_REPLAYED;
    }
    if (now > command->expires_at) {
        return REMOTE_COMMAND_EXPIRED;
    }
    if (command->issued_at > (now + maximum_future_skew)) {
        return REMOTE_COMMAND_FROM_FUTURE;
    }

    canonicalize(command, canonical);
    hmac_sha1(device_secret, device_secret_length, canonical, sizeof(canonical),
              expected_tag);
    tag_matches = lock_constant_time_equal(expected_tag,
                                           command->authentication_tag,
                                           sizeof(expected_tag));
    memset(canonical, 0, sizeof(canonical));
    memset(expected_tag, 0, sizeof(expected_tag));
    if (!tag_matches) {
        return REMOTE_COMMAND_BAD_TAG;
    }

    *last_accepted_request_id = command->request_id;
    return REMOTE_COMMAND_ACCEPTED;
}
