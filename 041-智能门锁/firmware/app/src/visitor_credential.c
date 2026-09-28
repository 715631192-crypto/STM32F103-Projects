#include "visitor_credential.h"

#include <string.h>

#include "lock_auth.h"

#define VISITOR_CODE_MIN_LENGTH 6U
#define VISITOR_CODE_MAX_LENGTH 12U

static void make_tag(const uint8_t *secret, size_t secret_length,
                     const char *code, size_t code_length,
                     uint8_t tag[SHA1_DIGEST_SIZE])
{
    hmac_sha1(secret, secret_length, (const uint8_t *)code, code_length, tag);
}

bool visitor_credential_issue(visitor_credential_t *credential,
                              const uint8_t *device_secret,
                              size_t device_secret_length,
                              const char *code, size_t code_length,
                              uint64_t valid_from, uint64_t valid_until,
                              uint8_t permitted_uses)
{
    if ((credential == NULL) || (device_secret == NULL) || (code == NULL) ||
        (device_secret_length < 16U) ||
        (code_length < VISITOR_CODE_MIN_LENGTH) ||
        (code_length > VISITOR_CODE_MAX_LENGTH) ||
        (valid_until <= valid_from) || (permitted_uses == 0U)) {
        return false;
    }

    memset(credential, 0, sizeof(*credential));
    make_tag(device_secret, device_secret_length, code, code_length,
             credential->code_tag);
    credential->valid_from = valid_from;
    credential->valid_until = valid_until;
    credential->uses_remaining = permitted_uses;
    credential->active = true;
    return true;
}

bool visitor_credential_verify_and_consume(visitor_credential_t *credential,
                                           const uint8_t *device_secret,
                                           size_t device_secret_length,
                                           const char *candidate,
                                           size_t candidate_length,
                                           uint64_t now)
{
    uint8_t candidate_tag[SHA1_DIGEST_SIZE];
    bool matched;

    if ((credential == NULL) || (device_secret == NULL) || (candidate == NULL) ||
        (!credential->active) || (candidate_length < VISITOR_CODE_MIN_LENGTH) ||
        (candidate_length > VISITOR_CODE_MAX_LENGTH) ||
        (now < credential->valid_from) || (now > credential->valid_until) ||
        (credential->uses_remaining == 0U)) {
        return false;
    }

    make_tag(device_secret, device_secret_length, candidate, candidate_length,
             candidate_tag);
    matched = lock_constant_time_equal(candidate_tag, credential->code_tag,
                                       sizeof(candidate_tag));
    memset(candidate_tag, 0, sizeof(candidate_tag));
    if (!matched) {
        return false;
    }

    --credential->uses_remaining;
    if (credential->uses_remaining == 0U) {
        credential->active = false;
        memset(credential->code_tag, 0, sizeof(credential->code_tag));
    }
    return true;
}
