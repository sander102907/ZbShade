#include "ZigbeeShade.h"
#include "nvs_flash.h"

#if SOC_IEEE802154_SUPPORTED

#define STORAGE_NAMESPACE "storage"
#define TILT_PERC_KEY "state"

TaskHandle_t _task_ref = NULL;

struct TiltChangeTaskParams {
  ZigbeeShade *instance;
  int16_t new_tilt_perc;
};


ZigbeeShade::ZigbeeShade(uint8_t endpoint)
  : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SHADE_DEVICE_ID;

  ESP_ERROR_CHECK(nvs_flash_init());

  zigbee_shade_cfg_t shade_cfg = ZIGBEE_DEFAULT_SHADE_CONFIG();
  _cluster_list = zigbee_shade_clusters_create(&shade_cfg);
  _ep_config = { .endpoint = endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SHADE_DEVICE_ID, .app_device_version = 0 };
  Serial.printf("Shade endpoint created %d\n", _endpoint);

  _current_state = IDLE;
  _current_tilt_perc = 0;
  _task_cancel = false;

  load_tilt_perc();
}

void ZigbeeShade::load_tilt_perc() {
  nvs_handle_t my_handle;
  esp_err_t err;
  int32_t tilt_perc_local = 0;

  err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle\n!", esp_err_to_name(err));
  }

  err = nvs_get_i32(my_handle, TILT_PERC_KEY, &tilt_perc_local);
  if (err == ESP_OK) {
    _current_tilt_perc = tilt_perc_local;
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    Serial.printf("Error (%s) reading!\n", esp_err_to_name(err));
  }

  nvs_close(my_handle);
}

void ZigbeeShade::save_tilt_perc() {
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    return;
  }

  err = nvs_set_i32(my_handle, TILT_PERC_KEY, _current_tilt_perc);
  if (err != ESP_OK) {
    Serial.printf("Failed to write to NVS (%s)!\n", esp_err_to_name(err));
  }

  err = nvs_commit(my_handle);
  if (err != ESP_OK) {
    Serial.printf("Failed to commit to NVS (%s)!\n", esp_err_to_name(err));
  }

  nvs_close(my_handle);
}

void ZigbeeShade::on_connected() {
  report_tilt_perc();
}

// set attribute method -> method overridden in child class
void ZigbeeShade::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  // check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
    if (message->attribute.id == ESP_ZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN) {
      _current_state = OPENING;
      state_changed();
    } else if (message->attribute.id == ESP_ZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE) {
      _current_state = CLOSING;
      state_changed();
    } else if (message->attribute.id == ESP_ZB_ZCL_CMD_WINDOW_COVERING_STOP) {
      _current_state = IDLE;
      state_changed();
    } else if (message->attribute.id == ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_TILT_PERCENTAGE) {
      uint16_t tilt_perc = message->attribute.data.type;

      if (tilt_perc > _current_tilt_perc) {
        _current_state = OPENING;
      } else if (tilt_perc < _current_tilt_perc) {
        _current_state = CLOSING;
      }
      tilt_changed(tilt_perc);
    } else {
      Serial.printf("Received message ignored. Attribute ID: %d not supported for window covering\n", message->attribute.id);
    }
  } else {
    Serial.printf("Received message ignored. Cluster ID: %d not supported for window covering\n", message->info.cluster);
  }
}

void ZigbeeShade::tilt_changed(uint16_t new_tilt_perc) {
  new_tilt_perc = min(new_tilt_perc, static_cast<uint16_t>(100));
  new_tilt_perc = max(new_tilt_perc, static_cast<uint16_t>(0));
  
  if (_motor_forward && _motor_backward && _motor_stop) {
    // Allocate memory for struct
    TiltChangeTaskParams *params = new TiltChangeTaskParams;
    params->instance = this;
    params->new_tilt_perc = new_tilt_perc;

    // Cancel existing task safely
    if (_task_ref != NULL) {
      _task_cancel = true;
      while (_task_ref != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Wait for task to stop
      }
    }

    _task_cancel = false;

    xTaskCreate(tilt_changed_task, "OnTiltChange", 4096, params, 5, &_task_ref);
  } else {
    Serial.println("No callback function set for tilt change");
  }
}

void ZigbeeShade::tilt_changed_task(void *pvParameters) {
  TiltChangeTaskParams *params = (TiltChangeTaskParams *)pvParameters;
  ZigbeeShade *shadeInstance = params->instance;
  int16_t new_tilt_perc = params->new_tilt_perc;

  int16_t tilt_change_perc = new_tilt_perc - shadeInstance->_current_tilt_perc;

  if (tilt_change_perc > 0) {
    shadeInstance->_motor_forward();
  } else {
    shadeInstance->_motor_backward();
  }

  uint16_t starting_tilt_perc = shadeInstance->_current_tilt_perc;

  int duration_ms = abs(tilt_change_perc) * shadeInstance->msPerTiltPerc;  // Calculate motor run time

  unsigned long startTime = millis();  // Get start time in milliseconds

  while (millis() - startTime < duration_ms) {
    if (shadeInstance->_task_cancel) {
      shadeInstance->_task_cancel = false;
      shadeInstance->_motor_stop();
      _task_ref = NULL;
      vTaskDelete(NULL);
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    shadeInstance->_current_tilt_perc = starting_tilt_perc + int((millis() - startTime) / shadeInstance->msPerTiltPerc) * (tilt_change_perc > 0 ? 1 : -1);
  }

  shadeInstance->_motor_stop();
  shadeInstance->_current_tilt_perc = new_tilt_perc;
  shadeInstance->report_tilt_perc();
  shadeInstance->save_tilt_perc();

  // Free memory allocated for the struct
  delete params;

  _task_ref = NULL;
  vTaskDelete(NULL);
}

void ZigbeeShade::clear_task() {
  if (_task_ref != NULL) {
    vTaskDelete(_task_ref);
    _task_ref = NULL;
  }
}

void ZigbeeShade::set_tilt_perc(uint16_t tilt_perc) {
  tilt_changed(tilt_perc);
}

void ZigbeeShade::report_tilt_perc() {
  Serial.printf("Reporting tilt percentage: %u\n", _current_tilt_perc);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID, &_current_tilt_perc, false);
  esp_zb_lock_release();
}

void ZigbeeShade::state_changed() {
  if (_current_state == OPENING) {
    if (_on_shade_open) {
      _on_shade_open();
    } else {
      Serial.println("No callback function set for on shade open");
    }
  }

  if (_current_state == CLOSING) {
    if (_on_shade_close) {
      _on_shade_close();
    } else {
      Serial.println("No callback function set for on shade close");
    }
  }

  if (_current_state == IDLE) {
    if (_on_shade_stop) {
      _on_shade_stop();
    } else {
      Serial.println("No callback function set for on shade stop");
    }
  }
}

void ZigbeeShade::set_state(shade_state state) {
  _current_state = state;
  state_changed();
}

esp_zb_cluster_list_t *ZigbeeShade::zigbee_shade_clusters_create(zigbee_shade_cfg_t *zb_shade_cfg) {
  esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&zb_shade_cfg->basic_cfg);
  esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&zb_shade_cfg->identify_cfg);
  esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_groups_cluster_create(&zb_shade_cfg->groups_cfg);
  esp_zb_attribute_list_t *esp_zb_shade_cfg_cluster = esp_zb_shade_config_cluster_create(&zb_shade_cfg->shade_cfg);
  esp_zb_attribute_list_t *esp_zb_window_covering_cluster = esp_zb_window_covering_cluster_create(&zb_shade_cfg->window_cfg);

  uint8_t current_tilt_percentage = 0;
  uint16_t installed_open_limit_tilt = 0x0000;
  uint16_t installed_closed_limit_tilt = 0xffff;
  uint16_t current_position_tilt = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_DEFAULT_VALUE;

  uint8_t current_lift_percentage = 0;
  uint16_t current_position_lift = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_DEFAULT_VALUE;

  esp_zb_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_U8, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &current_tilt_percentage);
  esp_zb_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_16BIT, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &installed_open_limit_tilt);
  esp_zb_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_16BIT, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &installed_closed_limit_tilt);
  esp_zb_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_16BIT, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &current_position_tilt);

  esp_zb_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_U8, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &current_lift_percentage);
  esp_zb_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID,
                          ESP_ZB_ZCL_ATTR_TYPE_16BIT, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &current_position_lift);

  // ------------------------------ Create cluster list ------------------------------
  esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_shade_config_cluster(esp_zb_cluster_list, esp_zb_shade_cfg_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_window_covering_cluster(esp_zb_cluster_list, esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  return esp_zb_cluster_list;
}

#endif  // SOC_IEEE802154_SUPPORTED
