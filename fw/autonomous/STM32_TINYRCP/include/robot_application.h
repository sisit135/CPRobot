#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Create bring-up and application tasks after successful Arduino HAL
 * init.
 * @returns true if all required tasks were created; false on a startup error.
 * @note Called once by setup(), before vTaskStartScheduler().
 */
bool robot_application_start(void);
#ifdef __cplusplus
}
#endif
