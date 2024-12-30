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

#pragma once

#include <map>
#include <set>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

// #ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp_now_ble_proxy {

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  char device[17];
  float temperature;
  uint8_t batteryLevel;
  uint32_t crc32;
} struct_message;

class ESP_NOW_BLE_PROXY : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_hostname(const std::string &hostname) { hostname_ = hostname; };
  void set_broadcast_address(const std::string &broadcast_address) {
    sscanf(broadcast_address.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &broadcast_address_[0], &broadcast_address_[1], &broadcast_address_[2], &broadcast_address_[3], &broadcast_address_[4], &broadcast_address_[5]);
  };
  void set_security_key(const std::string &security_key) { security_key_ = security_key; };
  void add_macs_allowed(const std::string &item) { macs_allowed_.insert(item); };
  void add_macs_disallowed(const std::string &item) { macs_disallowed_.insert(item); };
  void add_macs_renamed(const std::string &item) { macs_rename_.insert(item); };
  void set_reboot_interval(uint32_t update_interval) { reboot_millis_ = millis() + update_interval; }
  void set_notify_interval(uint32_t notify_interval) { notify_interval_millis_ = notify_interval; }
  void update_ble_enabled(bool enabled_yes);

 protected:
  void init_esp_now();
  uint32_t calculate_crc32(const uint8_t *data, size_t len);
  void notify_data(const esp32_ble_tracker::ESPBTDevice &device, 
    std::string label, double value);
  bool send_data_http(const esp32_ble_tracker::ESPBTDevice &device, 
    std::string label, double value, bool new_device);
  std::string get_device_name(const esp32_ble_tracker::ESPBTDevice &device);
  bool can_track(const esp32_ble_tracker::ESPBTDevice &device);
  void seen_device(const esp32_ble_tracker::ESPBTDevice &device);
  void check_auto_reboot();
  void notify_seen_devices();
  std::string hostname_;
  std::string security_key_;
  uint8_t broadcast_address_[6];
  std::set<std::string> macs_allowed_;
  std::set<std::string> macs_disallowed_;
  std::set<std::string> macs_rename_;
  std::map<std::string, double> sensors_value_sum_;
  std::map<std::string, int> sensors_value_count_;
  std::map<std::string, long unsigned int> sensors_last_notified_;
  long unsigned int reboot_millis_ = 0;
  long unsigned int seen_devices_notify_millis_ = 0;
  long unsigned int notify_interval_millis_ = 0; 
  bool esp_now_initialized = false;
};


template<typename... Ts> class BleEnableAction : public Action<Ts...> {
  public:
    BleEnableAction(BLE_PROXY *ble_prox) : ble_prox_(ble_prox) {}
    void play(Ts... x) override { this->ble_prox_->update_ble_enabled(true); }

  protected:
    BLE_PROXY *ble_prox_;
};

template<typename... Ts> class BleDisableAction : public Action<Ts...> {
  public:
    BleDisableAction(BLE_PROXY *ble_prox) : ble_prox_(ble_prox) {}
    void play(Ts... x) override { this->ble_prox_->update_ble_enabled(false); }

  protected:
    BLE_PROXY *ble_prox_;
};


}  // namespace ble_proxy
}  // namespace esphome

// #endif
