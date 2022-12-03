/*
 * MIT License
 * Copyright (c) 2022 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gba_emu.h"
#include "gba_internal.h"
#include <stdlib.h>

#include "lvgl/lvgl.h"

#if USE_EVDEV

#include "lv_drivers/indev/evdev.h"

static int get_kbd_event_number()
{
    const char* cmd = "grep -E 'Handlers|EV=' /proc/bus/input/devices | grep -B1 'EV=120013' | "
                      "grep -Eo 'event[0-9]+' | grep -Eo '[0-9]+' | tr -d '\n'";

    FILE* pipe = popen(cmd, "r");
    char buffer[8] = { 0 };
    int number = -1;
    while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            number = atoi(buffer);
        }
    }
    pclose(pipe);
    return number;
}

static void gba_input_update_cb(gba_context_t* ctx)
{
    lv_indev_data_t data;
    static lv_indev_drv_t indev_drv;

    if (indev_drv.type != LV_INDEV_TYPE_KEYPAD) {
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    }

    evdev_read(&indev_drv, &data);

    static const lv_key_t key_map[_GBA_JOYPAD_MAX] = {
        LV_KEY_BACKSPACE,
        0,
        LV_KEY_ESC,
        LV_KEY_HOME,
        LV_KEY_UP,
        LV_KEY_DOWN,
        LV_KEY_LEFT,
        LV_KEY_RIGHT,
        LV_KEY_ENTER,
        0,
        LV_KEY_PREV,
        LV_KEY_NEXT,
        0,
        0,
        0,
        0
    };

    for (int i = 0; i < GBA_ARRAY_SIZE(key_map); i++) {
        if (data.key == key_map[i]) {
            ctx->key_state[i] = (data.state == LV_INDEV_STATE_PRESSED);
        }
    }
}

void gba_evdev_init(gba_context_t* ctx)
{
    int number = get_kbd_event_number();
    if (number >= 0) {
        char kbd_event_name[32] = { 0 };
        lv_snprintf(kbd_event_name, sizeof(kbd_event_name), "/dev/input/event%d", number);
        LV_LOG_USER("kbd_name: %s", kbd_event_name);
        evdev_set_file(kbd_event_name);

        ctx->input_update_cb = gba_input_update_cb;
    } else {
        LV_LOG_WARN("can't get kbd event number");
    }
}

#endif

static void gba_context_init(gba_context_t* ctx)
{
    LV_ASSERT_NULL(ctx);
    lv_memzero(ctx, sizeof(gba_context_t));
}

static void gba_emu_timer_cb(lv_timer_t* timer)
{
    gba_context_t* gba_ctx = timer->user_data;
    gba_retro_run(gba_ctx);
}

lv_obj_t* lv_gba_emu_create(lv_obj_t* par, const char* rom_file_path)
{
    LV_ASSERT_NULL(rom_file_path);

    static gba_context_t gba_ctx;
    gba_context_init(&gba_ctx);

    gba_retro_init(&gba_ctx);

    if (!gba_view_init(&gba_ctx, par)) {
        goto failed;
    }

    char real_path[128];
    lv_snprintf(real_path, sizeof(real_path), "/%s", rom_file_path);

    if (!gba_retro_load_game(&gba_ctx, real_path)) {
        LV_LOG_ERROR("load ROM: %s failed", real_path);
        goto failed;
    }

#if USE_EVDEV
    gba_evdev_init(&gba_ctx);
#endif

    gba_ctx.timer = lv_timer_create(gba_emu_timer_cb, 1000 / gba_ctx.av_info.fps, &gba_ctx);

failed:
    return gba_view_get_root(&gba_ctx);
}
