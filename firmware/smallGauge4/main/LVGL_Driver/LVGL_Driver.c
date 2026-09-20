#include "LVGL_Driver.h"
#include "../src/lvgl_lock.h"
// === ADD THESE INCLUDES TO FIX THE ERRORS ===
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "ST77916.h" // Brings in EXAMPLE_LCD_WIDTH, panel_handle, etc.

// If 'tp' is declared in another touch header (like CST816.h), include it as well:
#include "CST816.h"

extern void wait_for_vsync(void); // Forward declaration for the TE wait function
extern esp_lcd_panel_handle_t panel_handle;

static const char *TAG_LVGL = "LVGL";

lv_disp_draw_buf_t disp_buf;                  // contains internal graphic buffer(s) called draw buffer(s)
lv_disp_drv_t disp_drv = {.user_data = NULL}; // contains callback functions
lv_indev_drv_t indev_drv;

// void example_increase_lvgl_tick(void *arg)
// {
//     /* Tell LVGL how many milliseconds has elapsed */
//     lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
// }

// void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
// {
//     esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
//     int offsetx1 = area->x1;
//     int offsetx2 = area->x2;
//     int offsety1 = area->y1;
//     int offsety2 = area->y2;
//     // copy a buffer's content to a specific area of the display
//     esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 +1, offsety2 + 1, color_map);
//     lv_disp_flush_ready(drv);
// }

void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// ASYNCHRONOUS CALLBACK: Called when the QSPI DMA finish transferring the pixels
// 1. Declare the global tracking flag at the top of LVGL_Driver.c
volatile bool lvgl_flush_in_progress = false;

// 2. Update your callback function
// bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
// {
//     // Check our flag: ONLY notify LVGL if the transfer was kicked off by LVGL's flush_cb
//     if (lvgl_flush_in_progress && disp != NULL && disp->driver != NULL)
//     {
//         lvgl_flush_in_progress = false; // Reset the flag for the next frame
//         lv_disp_flush_ready(disp->driver);
//     }

//     return false; // Return false as required by esp_lcd panel IO event callback signatures
// }

bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    // Notify LVGL that the last chunk is finished
    if (drv != NULL && lvgl_flush_in_progress)
    {
        lvgl_flush_in_progress = false;
        lv_disp_flush_ready(drv);
    }

    return false;
}

void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    // esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // Check if this is the start of a new frame
    if (area->y1 == 0 && area->x1 == 0)
    {
        // This is the first area of a new frame; set the flag
        wait_for_vsync(); // Wait for the TE signal to ensure we are in the vertical blanking period
    }

    // Set the flag BEFORE starting the transfer
    lvgl_flush_in_progress = true;

    // Push data via DMA. Because TE is configured on the driver,
    // esp_lcd will coordinate with GPIO18 to avoid cutting into the active refresh.
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);

    // REMOVED: lv_disp_flush_ready(drv);
    // This is now handled asynchronously by the DMA callback!
}

/*Read the touchpad*/
void example_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    // GUARD: If user_data was registered as NULL because LVGL started early,
    // try to grab the now-initialized global 'tp' handle.
    if (drv->user_data == NULL)
    {
        if (tp != NULL)
        {
            drv->user_data = tp;
        }
        else
        {
            // Touchpad driver is completely uninitialized; report released state and exit safely
            data->state = LV_INDEV_STATE_REL;
            return;
        }
    }

    uint16_t touchpad_x[5] = {0};
    uint16_t touchpad_y[5] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read touch controller data */
    esp_lcd_touch_read_data(drv->user_data);

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 5);

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
void example_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
lv_disp_t *disp;
// void LVGL_Init(void)
// {
//     ESP_LOGI(TAG_LVGL, "Initialize LVGL library");
//     lv_init();

//     // lvgl_lock_init();   // <-- add this

//     lv_color_t *buf1 = heap_caps_malloc(LVGL_BUF_LEN * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
//     assert(buf1);
//     lv_color_t *buf2 = heap_caps_malloc(LVGL_BUF_LEN * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
//     assert(buf2);
//     lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LVGL_BUF_LEN); // initialize LVGL draw buffers

//     ESP_LOGI(TAG_LVGL, "Register display driver to LVGL");
//     lv_disp_drv_init(&disp_drv); // Create a new screen object and initialize the associated device
//     disp_drv.hor_res = EXAMPLE_LCD_WIDTH;
//     disp_drv.ver_res = EXAMPLE_LCD_HEIGHT; // Horizontal pixel count
//     // disp_drv.rotated = LV_DISP_ROT_90; // 图像旋转                                                            // Vertical axis pixel count
//     disp_drv.flush_cb = example_lvgl_flush_cb;                  // Function : copy a buffer's content to a specific area of the display
//     disp_drv.drv_update_cb = example_lvgl_port_update_callback; // Function : Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated.
//     disp_drv.draw_buf = &disp_buf;                              // LVGL will use this buffer(s) to draw the screens contents
//     disp_drv.user_data = panel_handle;
//     ESP_LOGI(TAG_LVGL, "Register display indev to LVGL"); // Custom display driver user data
//     disp = lv_disp_drv_register(&disp_drv);

//     lv_indev_drv_init(&indev_drv);
//     indev_drv.type = LV_INDEV_TYPE_POINTER;
//     indev_drv.disp = disp;
//     indev_drv.read_cb = example_touchpad_read;
//     indev_drv.user_data = tp;
//     lv_indev_drv_register(&indev_drv);

//     /********************* LVGL *********************/
//     ESP_LOGI(TAG_LVGL, "Install LVGL tick timer");
//     // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
//     const esp_timer_create_args_t lvgl_tick_timer_args = {
//         .callback = &example_increase_lvgl_tick,
//         .name = "lvgl_tick"};

//     esp_timer_handle_t lvgl_tick_timer = NULL;
//     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
//     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
// }

void LVGL_Init(void)
{
    ESP_LOGI(TAG_LVGL, "Initialize LVGL library");
    lv_init();

// OPTIMIZATION: Instead of full frames in slow PSRAM, allocate 2 partial buffers in blazing-fast Internal DMA SRAM
// Height of 40 lines requires ~28.8 KB per buffer, which easily fits inside SRAM.
#define PARALLEL_BUF_LINES 40
#define PARALLEL_BUF_LINES 120
    uint32_t buf_pixel_cnt = EXAMPLE_LCD_WIDTH * PARALLEL_BUF_LINES;

    // lv_color_t *buf1 = heap_caps_malloc(buf_pixel_cnt * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    // lv_color_t *buf2 = heap_caps_malloc(buf_pixel_cnt * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    // Change MALLOC_CAP_INTERNAL to MALLOC_CAP_SPIRAM
    lv_color_t *buf1 = heap_caps_malloc(buf_pixel_cnt * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(buf_pixel_cnt * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);

    assert(buf1 && buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_pixel_cnt);

    ESP_LOGI(TAG_LVGL, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_WIDTH;
    disp_drv.ver_res = EXAMPLE_LCD_HEIGHT;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;

    disp = lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_touchpad_read;
    indev_drv.user_data = tp;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG_LVGL, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"};

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
}