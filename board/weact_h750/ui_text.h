/**
 * @file ui_text.h
 * @brief One-line text rendering of the machine status (UART log), hardware independent
 */

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <stddef.h>
#include "ui_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format e.g. "STATE=RUNNING PHASE=WASH TIME=29:45 COIN=000C RLED=0 BLED=1 MOTOR=AGIT PUMP=0 LOCK=1 FAULT=-"
 * @return number of characters written (excluding the terminating NUL); output is always terminated
 */
size_t ui_status_line(char *buf, size_t cap, const ui_view_t *v);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEXT_H */
