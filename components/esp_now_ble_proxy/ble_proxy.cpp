/*
Copyright 2024 Lucian SABO
Adapted after John Mueller's work
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ble_proxy.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <esp_now.h>
#include <esp_crc.h> 
#include "esp_wifi.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

// #ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp_now_ble_proxy {

static const char *TAG = "ble_proxy";

uint32_t ESP_NOW_BLE_PROXY::calculate_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = esp_crc32_le(UINT32_MAX, (const uint8_t *)security_key_.c_str(), security_key_.size()); // Start with the CRC of the secret key
    crc = esp_crc32_le(crc, data, len); // Continue with the data
    return crc;
}

void ESP_NOW_BLE_PROXY::init_esp_now() {

  esp_now_peer_info_t peerInfo = {};
  esp_wifi_set_mode(WIFI_MODE_STA);

   // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing ESP-NOW");
    return;
  }

  ESP_LOGD(TAG, "initialized ESP-NOW");

  // Register peer
  memcpy(peerInfo.peer_addr, broadcast_address_, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    ESP_LOGE(TAG, "Failed to add peer");
  }

  esp_now_initialized = true;
}

/* Handle BLE device, check if we need to track & forward it
*/
bool ESP_NOW_BLE_PROXY::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // skip random BLE devices
  esp_ble_addr_type_t d_type = device.get_address_type();
  if ((d_type==BLE_ADDR_TYPE_RANDOM) || (d_type==BLE_ADDR_TYPE_RPA_RANDOM)) return false;

  for (auto &service_data : device.get_service_datas()) {
    esphome::esp32_ble_tracker::adv_data_t x = service_data.data;
    //ESP_LOGD("ble_adv", "ble data: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x ",  x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15],x[16]);

    if (x[5] != 0x02) {
       continue;
    }

    if (!esp_now_initialized) {
        init_esp_now();
    }

    std::string dev_addr(this->get_device_name(device));
    struct_message message;
    strcpy(message.device, dev_addr.c_str()); 

    int16_t temp_int = ((((x[6] & 0xff) | (x[7] & 0xff) << 8)));
    message.temperature = temp_int / 100.0;                    
    //float hum = ((((x[9] & 0xff) | (x[10] & 0xff) << 8))) / 100.0;
    message.batteryLevel = (x[4] & 0xff);

    std::string avg_temp(dev_addr + "/" + "temp");
    std::string avg_bat(dev_addr + "/" + "bat");
    sensors_value_sum_[avg_temp] += message.temperature ;
    sensors_value_count_[avg_temp]++;    
    sensors_value_sum_[avg_bat] += message.batteryLevel ;
    sensors_value_count_[avg_bat]++;

    if (millis() - sensors_last_notified_[avg_temp] > notify_interval_millis_) {
        message.crc32 = calculate_crc32((uint8_t *)&message, sizeof(message) - sizeof(message.crc32));

        // Send message via ESP-NOW
        esp_err_t result = esp_now_send(broadcast_address_, (uint8_t *) &message, sizeof(message));

        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Error sending message");
        }

        sensors_value_sum_[avg_temp] = sensors_value_sum_[avg_bat] = 0.0;
        sensors_value_count_[avg_temp] = sensors_value_count_[avg_bat] = 0;
        sensors_last_notified_[avg_temp] = millis();

        return result == ESP_OK;
    }

  }

  check_auto_reboot();

  return false; // unless we rebooted
}


/* Check if it's time to reboot this device
*/
void ESP_NOW_BLE_PROXY::check_auto_reboot() {
  if (this->reboot_millis_>0) {
    if (millis() > this->reboot_millis_) { 
      // no overflow, since we're counting from boot
      ESP_LOGI(TAG, "Rebooting now.");
      delay(500); // Let MQTT settle a bit
      App.safe_reboot();    
    }
  }
}

/* Get name of this device, taking into account mappings
*/
std::string ESP_NOW_BLE_PROXY::get_device_name(const esp32_ble_tracker::ESPBTDevice &device) {
  std::string mymac(device.address_str());  // MAC address
  for (auto elem : this->macs_rename_) {
    if (elem.find(mymac + "=") == 0) { // rename entry starts with MAC address
      mymac = elem.substr(mymac.length()+1); // use part after '='
    }
  }
  return(mymac);
}

/* Check if we can track this device
*/
bool ESP_NOW_BLE_PROXY::can_track(const esp32_ble_tracker::ESPBTDevice &device) {
  std::string mymac(device.address_str());  // MAC address
  if (!(this->macs_allowed_.empty())) { // allow-list exists, must follow it
    if (this->macs_allowed_.find(mymac) == this->macs_allowed_.end()) {
      ESP_LOGD(TAG, "Device not trackable: '%s' not in allow list", mymac.c_str());
      return(false);
    }
  }
  if (this->macs_disallowed_.find(mymac) != this->macs_disallowed_.end()) {
    ESP_LOGD(TAG, "Device not trackable: '%s' is in disallow list", mymac.c_str());
    return(false);
  }
  return(true);
}


void ESP_NOW_BLE_PROXY::update_ble_enabled(bool enabled_yes) {
  esp_err_t err;
  ESP_LOGD(TAG, "update_ble_enable to %i", (enabled_yes?1:0));
  if (enabled_yes) { // Enable BLE
    ESP_LOGD(TAG, "running esp32_ble_tracker::global_esp32_ble_tracker->setup()");
    esp32_ble_tracker::global_esp32_ble_tracker->setup();
    ESP_LOGD(TAG, "esp32_ble_tracker::global_esp32_ble_tracker->setup() complete");
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
      ESP_LOGD(TAG, "BLE already enabled, can't enable!");
    } else {
      ESP_LOGD(TAG, "BLE currently not enabled, trying to enable");
      // assuming we use standard esp32_ble_tracker init ...
      err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %s", esp_err_to_name(err));
      } else {
        ESP_LOGD(TAG, "BLE now enabled");
      }
    }
  } else { // Disable BLE
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
      ESP_LOGD(TAG, "BLE currently enabled, will disable");
      err = esp_bt_controller_disable();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE esp_bt_controller_disable failed: %s", esp_err_to_name(err));
      } else {
        ESP_LOGD(TAG, "BLE disabled.");
        ESP_LOGD(TAG, "BLE will now deinit() ...");
        err = esp_bt_controller_deinit(); // kill it all
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "BLE esp_bt_controller_deinit failed: %s", esp_err_to_name(err));
        } else {
          ESP_LOGD(TAG, "BLE esp_bt_controller_deinit successful.");
        }
      }
    } else {
      ESP_LOGD(TAG, "BLE NOT enabled, can't disable!");
    }
  }
}

}  // namespace esp_now_ble_proxy
}  // namespace esphome

// #endif
