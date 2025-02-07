#include "ZigbeeCore.h"
#include "Arduino.h"

#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED


// Custom Zigbee action handler override
esp_err_t custom_zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    Serial.printf("Test action handler (0x%x)\n", callback_id);
    esp_err_t ret = ESP_OK;
    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_set_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        ret = zb_cmd_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
        ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        ret = zb_cmd_default_resp_handler((esp_zb_zcl_cmd_default_resp_message_t *)message);
        break;
    // This is required for handling window covering/shade commands
    case ESP_ZB_CORE_WINDOW_COVERING_MOVEMENT_CB_ID:
        ret = zb_attribute_set_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        Serial.printf("Receive unhandled Zigbee action(0x%x) callback\n", callback_id);
        break;
    }
    return ret;
}

#endif // SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
