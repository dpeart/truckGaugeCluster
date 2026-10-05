#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_lock(void);
void lvgl_unlock(void);
void lvgl_lock_init(void);

#ifdef __cplusplus
}
#endif