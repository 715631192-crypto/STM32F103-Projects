#ifndef SMART_LOCK_CONTROLLER_H
#define SMART_LOCK_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LOCK_STATE_LOCKED = 0,
    LOCK_STATE_UNLOCKED,
    LOCK_STATE_LOCKOUT,
    LOCK_STATE_ALARM
} lock_state_t;

typedef enum {
    LOCK_CONTROL_AUTH_GRANTED = 0,
    LOCK_CONTROL_AUTH_DENIED,
    LOCK_CONTROL_REMOTE_UNLOCK,
    LOCK_CONTROL_FORCE_LOCK,
    LOCK_CONTROL_DOOR_OPENED,
    LOCK_CONTROL_DOOR_CLOSED,
    LOCK_CONTROL_TAMPER,
    LOCK_CONTROL_TICK,
    LOCK_CONTROL_CLEAR_ALARM,
    LOCK_CONTROL_CLEAR_LOCKOUT
} lock_control_event_t;

typedef struct {
    lock_state_t state;
    bool door_closed;
    uint64_t unlock_deadline;
    uint64_t lockout_deadline;
    uint32_t auto_lock_seconds;
} lock_controller_t;

void lock_controller_init(lock_controller_t *controller,
                          uint32_t auto_lock_seconds);
void lock_controller_set_lockout(lock_controller_t *controller,
                                 uint64_t until);
lock_state_t lock_controller_handle(lock_controller_t *controller,
                                    lock_control_event_t event,
                                    uint64_t now);

#endif
