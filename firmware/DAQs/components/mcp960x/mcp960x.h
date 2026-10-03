#ifndef __MCP960X_H__
#define __MCP960X_H__

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCP960X_ADDR_BASE     0x60
#define MCP960X_ADDR_DEFAULT  0x67

#define MCP960X_FILTER_OFF 0
#define MCP960X_FILTER_MAX 7

typedef enum
{
    MCP960X_TYPE_K = 0,
    MCP960X_TYPE_J,
    MCP960X_TYPE_T,
    MCP960X_TYPE_N,
    MCP960X_TYPE_S,
    MCP960X_TYPE_E,
    MCP960X_TYPE_B,
    MCP960X_TYPE_R,
} mcp960x_thermocouple_t;

typedef enum
{
    MCP960X_ADC_RES_18 = 0,
    MCP960X_ADC_RES_16,    
    MCP960X_ADC_RES_14,    
    MCP960X_ADC_RES_12,    
} mcp960x_adc_resolution_t;

typedef enum
{
    MCP960X_SAMPLES_1 = 0,
    MCP960X_SAMPLES_2,
    MCP960X_SAMPLES_4,
    MCP960X_SAMPLES_8,
    MCP960X_SAMPLES_16,
    MCP960X_SAMPLES_32,
    MCP960X_SAMPLES_64,
    MCP960X_SAMPLES_128,
} mcp960x_burst_samples_t;

typedef enum
{
    MCP960X_MODE_NORMAL = 0,
    MCP960X_MODE_SHUTDOWN,
    MCP960X_MODE_BURST,
} mcp960x_mode_t;

typedef enum
{
    MCP960X_TC_RES_0_0625 = 0,
    MCP960X_TC_RES_0_25,      
} mcp960x_tc_resolution_t;

typedef enum
{
    MCP960X_OK = 0,       
    MCP960X_OPEN_CIRCUIT, 
    MCP960X_SHORT_CIRCUIT,
} mcp960x_status_t;

typedef enum
{
    MCP960X_ALERT_1 = 0,
    MCP960X_ALERT_2,
    MCP960X_ALERT_3,
    MCP960X_ALERT_4,
} mcp960x_alert_t;

typedef enum
{
    MCP960X_ALERT_DISABLED,
    MCP960X_ALERT_COMP,    
    MCP960X_ALERT_INT,     
} mcp960x_alert_mode_t;

typedef enum
{
    MCP960X_ACTIVE_LOW = 0,
    MCP960X_ACTIVE_HIGH    
} mcp960x_alert_level_t;

typedef enum
{
    MCP960X_FALLING = 0,
    MCP960X_RISING      
} mcp960x_alert_temp_dir_t;

typedef enum
{
    MCP960X_ALERT_SRC_TH = 0,
    MCP960X_ALERT_SRC_TC     
} mcp960x_alert_source_t;

/**
 * Device descriptor (Replaced i2c_dev with port & addr)
 */
typedef struct
{
    i2c_port_t port;   /**< I2C port number */
    uint8_t addr;      /**< Device I2C address */
    uint8_t id;        /**< Hardware ID */
    uint8_t revision;  /**< Hardware revision */
} mcp960x_t;

esp_err_t mcp960x_init_desc(mcp960x_t *dev, uint8_t addr, i2c_port_t port);
esp_err_t mcp960x_init(mcp960x_t *dev);
esp_err_t mcp960x_set_sensor_config(mcp960x_t *dev, mcp960x_thermocouple_t th, uint8_t filter);
esp_err_t mcp960x_get_sensor_config(mcp960x_t *dev, mcp960x_thermocouple_t *th, uint8_t *filter);
esp_err_t mcp960x_set_device_config(mcp960x_t *dev, mcp960x_mode_t mode, mcp960x_burst_samples_t bs,
                                    mcp960x_adc_resolution_t adc_res, mcp960x_tc_resolution_t tc_res);
esp_err_t mcp960x_get_device_config(mcp960x_t *dev, mcp960x_mode_t *mode, mcp960x_burst_samples_t *bs,
                                    mcp960x_adc_resolution_t *adc_res, mcp960x_tc_resolution_t *tc_res);
esp_err_t mcp960x_set_mode(mcp960x_t *dev, mcp960x_mode_t mode);
esp_err_t mcp960x_get_raw_adc_data(mcp960x_t *dev, int32_t *data);
esp_err_t mcp960x_get_thermocouple_temp(mcp960x_t *dev, float *t);
esp_err_t mcp960x_get_delta_temp(mcp960x_t *dev, float *t);
esp_err_t mcp960x_get_ambient_temp(mcp960x_t *dev, float *t);
esp_err_t mcp960x_get_status(mcp960x_t *dev, bool *temp_ready, bool *burst_ready, mcp960x_status_t *status,
                             bool *alert1, bool *alert2, bool *alert3, bool *alert4);
esp_err_t mcp960x_set_alert_config(mcp960x_t *dev, mcp960x_alert_t alert, mcp960x_alert_mode_t mode,
                                   mcp960x_alert_level_t active_lvl, mcp960x_alert_temp_dir_t temp_dir, mcp960x_alert_source_t src,
                                   float limit, uint8_t hyst);
esp_err_t mcp960x_get_alert_config(mcp960x_t *dev, mcp960x_alert_t alert, mcp960x_alert_mode_t *mode,
                                   mcp960x_alert_level_t *active_lvl, mcp960x_alert_temp_dir_t *temp_dir, mcp960x_alert_source_t *src,
                                   float *limit, uint8_t *hyst);
esp_err_t mcp960x_get_alert_status(mcp960x_t *dev, mcp960x_alert_t alert, bool *status);
esp_err_t mcp960x_clear_alert_int(mcp960x_t *dev, mcp960x_alert_t alert);

#ifdef __cplusplus
}
#endif

#endif /* __MCP960X_H__ */