/* Class of Zigbee Shade endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

typedef enum {
  OPENING = 0x00,  /*!< Up/Open */
  CLOSING = 0x01, /*!< Down/Close */
  IDLE = 0x02,  /*!< Stop */
} shade_state;

/**
 * @brief Zigbee HA standard dimmable light device clusters.
 *        Added here as not supported by ESP Zigbee library.
 *
 *
 */
typedef struct zigbee_shade_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;            /*!<  Basic cluster configuration, @ref esp_zb_basic_cluster_cfg_s */
  esp_zb_identify_cluster_cfg_t identify_cfg;      /*!<  Identify cluster configuration, @ref esp_zb_identify_cluster_cfg_s */
  esp_zb_groups_cluster_cfg_t groups_cfg;          /*!<  Groups cluster configuration, @ref esp_zb_groups_cluster_cfg_s */
  esp_zb_shade_config_cluster_cfg_t shade_cfg;     /*!<  Shade config cluster configuration, @ref esp_zb_shade_config_cluster_cfg_s */
  esp_zb_window_covering_cluster_cfg_t window_cfg; /*!<  Window covering cluster configuration, @ref esp_zb_window_covering_cluster_cfg_s */
} zigbee_shade_cfg_t;

/**
 * @brief Zigbee HA standard dimmable light device default config value.
 *        Added here as not supported by ESP Zigbee library.
 *
 */
// clang-format off
#define ZIGBEE_DEFAULT_SHADE_CONFIG()                                  \
  {                                                                             \
    .basic_cfg =                                                                \
      {                                                                         \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,              \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,            \
      },                                                                        \
    .identify_cfg =                                                             \
      {                                                                         \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,       \
      },                                                                        \
    .groups_cfg =                                                               \
      {                                                                         \
        .groups_name_support_id = ESP_ZB_ZCL_GROUPS_NAME_SUPPORT_DEFAULT_VALUE, \
      },                                                                        \
    .shade_cfg =                                                               \
      {                                                                         \
        .status = ESP_ZB_ZCL_SHADE_CONFIG_STATUS_DEFAULT_VALUE,            \
        .closed_limit = ESP_ZB_ZCL_SHADE_CONFIG_CLOSED_LIMIT_DEFAULT_VALUE,         \
        .mode = ESP_ZB_ZCL_SHADE_CONFIG_MODE_DEFAULT_VALUE,         \
      },                                                                        \
    .window_cfg =                                                               \
      {                                                                         \
        .covering_type = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_TILT_BLIND_TILT_ONLY,                       \
        .covering_status = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_TILT_CONTROL_IS_CLOSED_LOOP, \
        .covering_mode = ESP_ZB_ZCL_WINDOW_COVERING_MODE_DEFAULT_VALUE, \
      },                                                                                \
  }
// clang-format on

class ZigbeeShade : public ZigbeeEP {
public:
  ZigbeeShade(uint8_t endpoint);
  ~ZigbeeShade() {}

  int msPerTiltPerc;

  void on_connected();

  // Use to set a cb function to be called on shade open
  void on_shade_open(void (*callback)()) {
    _on_shade_open = callback;
  }

  // Use to set a cb function to be called on shade close
  void on_shade_close(void (*callback)()) {
    _on_shade_close = callback;
  }

  // Use to set a cb function to be called on shade stop
  void on_shade_stop(void (*callback)()) {
    _on_shade_stop = callback;
  }

  // void on_tilt_change(void (*callback) (int16_t tilt_perc)) {
  //   _on_tilt_change = callback;
  // }

  void motor_forward (void (*callback)()) {
    _motor_forward = callback;
  }

  void motor_backward (void (*callback)()) {
    _motor_backward = callback;
  }

  void motor_stop (void (*callback)()) {
    _motor_stop = callback;
  }

  void set_state(shade_state state);
  void set_tilt_perc(uint16_t tilt_perc);
  void calibrate_tilt_perc(uint16_t tilt_perc);

  uint16_t get_tilt_perc() {
    return _current_tilt_perc;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  void (*_on_shade_open)();
  void (*_on_shade_close)();
  void (*_on_shade_stop)();
  void (*_motor_forward)();
  void (*_motor_backward)();
  void (*_motor_stop)();

  /**
   * @brief  Create a standard HA shade cluster list.
   *        Added here as not supported by ESP Zigbee library.
   *
   * @note This contains basic, identify, groups, shade, window covering as server side.
   * @param[in] shade_cfg  Configuration parameters for this cluster lists defined by @ref zigbee_shade_cfg_t
   *
   * @return Pointer to cluster list  @ref esp_zb_cluster_list_s
   *
   */
  esp_zb_cluster_list_t *zigbee_shade_clusters_create(zigbee_shade_cfg_t *shade_cfg);


  void load_tilt_perc();
  void save_tilt_perc();
  void state_changed();
  void tilt_changed(uint16_t new_tilt_perc);
  static void tilt_changed_task(void *pvParameters);
  static void clear_task();
  void report_tilt_perc();

  shade_state _current_state;
  uint16_t _current_tilt_perc;
  bool _task_cancel;
};

#endif  //SOC_IEEE802154_SUPPORTED
