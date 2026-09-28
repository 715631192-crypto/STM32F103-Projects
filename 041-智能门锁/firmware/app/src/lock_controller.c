#include "lock_controller.h"

#include <stddef.h>

void lock_controller_init(lock_controller_t *controller,
                          uint32_t auto_lock_seconds)
{
    if (controller == NULL) {
        return;
    }
    controller->state = LOCK_STATE_LOCKED;
    controller->door_closed = true;
    controller->unlock_deadline = 0U;
    controller->lockout_deadline = 0U;
    controller->auto_lock_seconds = auto_lock_seconds;
}

void lock_controller_set_lockout(lock_controller_t *controller,
                                 uint64_t until)
{
    if (controller == NULL) {
        return;
    }
    controller->state = LOCK_STATE_LOCKOUT;
    controller->lockout_deadline = until;
    controller->unlock_deadline = 0U;
}

lock_state_t lock_controller_handle(lock_controller_t *controller,
                                    lock_control_event_t event,
                                    uint64_t now)
{
    if (controller == NULL) {
        return LOCK_STATE_ALARM;
    }

    if (event == LOCK_CONTROL_TAMPER) {
        controller->state = LOCK_STATE_ALARM;
        controller->unlock_deadline = 0U;
        return controller->state;
    }
    if ((event == LOCK_CONTROL_CLEAR_ALARM) &&
        (controller->state == LOCK_STATE_ALARM)) {
        controller->state = LOCK_STATE_LOCKED;
        return controller->state;
    }

    if (controller->state == LOCK_STATE_ALARM) {
        return controller->state;
    }
    if (controller->state == LOCK_STATE_LOCKOUT) {
        if ((event == LOCK_CONTROL_CLEAR_LOCKOUT) ||
            ((event == LOCK_CONTROL_TICK) &&
             (now >= controller->lockout_deadline))) {
            controller->state = LOCK_STATE_LOCKED;
            controller->lockout_deadline = 0U;
        }
        return controller->state;
    }

    switch (event) {
    case LOCK_CONTROL_AUTH_GRANTED:
    case LOCK_CONTROL_REMOTE_UNLOCK:
        controller->state = LOCK_STATE_UNLOCKED;
        controller->unlock_deadline = now + controller->auto_lock_seconds;
        break;
    case LOCK_CONTROL_FORCE_LOCK:
        controller->state = LOCK_STATE_LOCKED;
        controller->unlock_deadline = 0U;
        break;
    case LOCK_CONTROL_DOOR_OPENED:
        controller->door_closed = false;
        break;
    case LOCK_CONTROL_DOOR_CLOSED:
        controller->door_closed = true;
        if (controller->state == LOCK_STATE_UNLOCKED) {
            controller->state = LOCK_STATE_LOCKED;
            controller->unlock_deadline = 0U;
        }
        break;
    case LOCK_CONTROL_TICK:
        if ((controller->state == LOCK_STATE_UNLOCKED) &&
            (controller->unlock_deadline != 0U) &&
            (now >= controller->unlock_deadline) && controller->door_closed) {
            controller->state = LOCK_STATE_LOCKED;
            controller->unlock_deadline = 0U;
        }
        break;
    case LOCK_CONTROL_AUTH_DENIED:
    case LOCK_CONTROL_TAMPER:
    case LOCK_CONTROL_CLEAR_ALARM:
    case LOCK_CONTROL_CLEAR_LOCKOUT:
    default:
        break;
    }
    return controller->state;
}
