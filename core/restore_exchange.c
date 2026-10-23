#include "opentape/restore.h"
unsigned ot_restore_exchange(const struct ot_restore_exchange_ops *ops, const char *stage,
    const char *destination, const char *previous, unsigned replacing,
    unsigned old_attributes, unsigned new_attributes, unsigned *recovery_error)
{
    unsigned error;
    if (ops == 0 || ops->attributes == 0 || ops->rename == 0 || ops->remove == 0 ||
        stage == 0 || destination == 0 || previous == 0 || recovery_error == 0) return 87u;
    *recovery_error = 0u;
    if (replacing) {
        error = ops->attributes(ops->context, destination, old_attributes & ~1u);
        if (error != 0u) return error;
        error = ops->rename(ops->context, destination, previous);
        if (error != 0u) {
            *recovery_error = ops->attributes(ops->context, destination, old_attributes);
            return error;
        }
    }
    error = ops->rename(ops->context, stage, destination);
    if (error != 0u && replacing) {
        *recovery_error = ops->rename(ops->context, previous, destination);
        if (*recovery_error == 0u)
            *recovery_error = ops->attributes(ops->context, destination, old_attributes);
    }
    if (error == 0u) error = ops->attributes(ops->context, destination, new_attributes);
    if (error == 0u && replacing) {
        error = ops->attributes(ops->context, previous, 0u);
        if (error == 0u) error = ops->remove(ops->context, previous);
    }
    return error;
}
