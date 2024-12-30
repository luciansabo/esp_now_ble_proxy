Bluetooth Low-Energy BTHome (ESPHome) proxy for ESP32 devices. Proxies multiple BTLE devices using BTHome for Home assistant and EspNow for your custom devices.

Tested with LYWSD03MMC thermometers - they're super-cheap.

Last update 2024-12-30. 
Author: Lucian Sabo (luciansabo@gmail.com)
Based on John Mueller work (johnmu.com)

## Goals

BTLE thermometers are cheap, and last a long time on battery. However, they use BTLE to communicate, which doesn't go far (10-15m? YMMV). ESP32 devices are also cheap, they support both BTLE as well as Wifi. This project proxies BTLE to EspNow via Wifi, supporting multiple devices that don't need to be configured individually.

Add BTLE devices where you need them, add proxies nearby as needed. 

## Setup (super-rough)

1. Set up ESPhome 
2. Clone this repo
3. Flash the ATC firmware on your Xiaomi devices (removes bind keys). Use the BTHome protocol.
4. Set up BTHome in Home Assistant and listen for EspNow traffic in your ESP devices 
5. Compile & run this code on an ESP32
6. Repeat to place proxy devices in strategic locations

Note that when using the proxy, don't also read the BTLE values directly with Home Assistant (you'll just get duplicated sensors). Don't use the BTLE module there, only use MQTT.

## BTLE Device firmware

I used [pvvx's ATC firmware](https://github.com/pvvx/ATC_MiThermometer). This seems to work well on the Xiaomi Mi / LYWSD03MMC devices I have. 

The simplest way is to navigate to the [flasher page](https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html) on a smartphone, connect to the BTLE device, flash the firmware, and then configure the firmware to use "Mi" connections (this is compatible with all Xiaomi-like software).
Make sure you choose BTHome as protocol.

Device lifetime seems to be 12-18 months on a CR2032 battery. I used 5000 as broadcast interval and it works good.

## Proxy setup

I recommend using the default config file to start. Use unique device names for each ESP32 that you use, since these need to use the device name to communicate and for over-the-air updates. 

Before compiling, make a copy of "secrets-example.yaml" and call it "secrets.yaml". In this file, update the wifi SSID, password.

To proxy to EspNow, just set the broadcast address to the MAC of an ESP.

```
esphome -s name yourdevicename run esp_now_ble_proxy_default.yaml
```

This will compile ESPhome based on the YAML-config file specified, using the device name 'yourdevicename'. I like to number my devices, but call them whatever you want.

The initial setup must be done with the device connected with a USB cable, afterwards you can update with OTA. 

I use a variety of ESP32 dev-boards, they're cheap, use USB, and are pretty small. I hang them from a USB charger with a short cable in out-of-the-way places, or from USB ports of routers, Raspberry-Pi's, etc. 

## ESP-NOW

ESP-NOW is a wireless communication protocol defined by Espressif, which enables the direct, quick and low-power control of smart devices, without the need of a router. ESP-NOW can work with Wi-Fi and Bluetooth LE, and supports the ESP8266, ESP32, ESP32-S and ESP32-C series of SoCs.

### Receiver Code (ESP32):

#### Include Required Libraries:

```C++
#include <esp_now.h>
#include <WiFi.h>
#include <esp_crc.h> // Include the CRC library
```

#### Define the Secret Key and Structure:


```C++
const char *SECRET_KEY = "your_secret_key";
typedef struct struct_message {
  char device[17];
  float temperature;
  uint8_t batteryLevel;
  uint32_t crc32;  
} struct_message;
```

#### Function to Calculate CRC32 with Secret Key:

```C++
uint32_t calculate_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = esp_crc32_le(UINT32_MAX, (const uint8_t *)SECRET_KEY, strlen(SECRET_KEY)); // Start with the CRC of the secret key
    crc = esp_crc32_le(crc, data, len); // Continue with the data
    return crc;
}
```

#### Callback When Data is Received:

```C++
void on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  struct_message data = {};

  // Copy incoming data to encryptedData
  memcpy(&data, incomingData, sizeof(struct_message));

  uint32_t calculated_crc32 = calculate_crc32((uint8_t *)&data, sizeof(data) - sizeof(data.crc32));


  char message[100];    
  if (data.crc32 == calculated_crc32) {    
    sprintf(message, "Device: %s; Temp: %.2f; Bat: %d", data.device, data.temperature, data.batteryLevel);
    Logger::verbose(message);
  } else {        
    Logger::warning("CRC verification failed");    
  }
}
```

#### Setup and Receive Data:

```C++
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(on_data_recv);
}

void loop() {
    // put your main code here, to run repeatedly:
}
```

#### Explanation:
- **CRC32 Calculation with Secret Key:** The calculate_crc32 function is modified to include the secret key in the CRC32 calculation. This enhances the integrity check by incorporating a shared secret.
- **Sender:** The sender calculates the CRC32 checksum using the secret key and the data, then sends the data.
- **Receiver:** The receiver calculates the CRC32 checksum using the same secret key and the received data, then compares it with the received CRC32 to verify data integrity.
- This approach provides enhanced integrity verification by incorporating a secret key into the CRC32 calculation.

### Device measurements

Measurements are published as:

/ble_proxy/[MAC Addr]/[measurement]/state

For example:

```
/ble_proxy/A4:C1:38:AA:BB:CC/battery_level/state
/ble_proxy/A4:C1:38:AA:BB:CC/temperature/state
```

Read these values to get the current measurement for each device. 



## Advanced configuration


Required settings:

* hostname - name of this ESP32 device, used for MQTT connections
* security_key - a random string used for securing the CRC32. You can generate a 16 char password
* broadcast_address - the mac address of the ESP device where you want to listen to sensor updates

Optional settings:

* auto_reboot_interval
  Time to automatically reboot the ESP32. Since we track seen BTLE devices, this encourages us not to run out of memory over time. 

  Example:

  ```auto_reboot_interval: 6h```

* mac_addresses_renamed
  MAC addresses are great until you have to swap out a device. With this setting, you can rename devices to us a different name. You can also use this to give BTLE devices an understandable name.
  Uses YAML lists with strings.

  Examples:

  ```mac_addresses_renamed: "A4:C1:38:00:11:22=A4:C1:38:AA:BB:CC"```

  ```
  mac_addresses_renamed:
   - "A4:C1:38:00:11:22=A4:C1:38:AA:BB:CC"
   - "A4:C1:38:00:22:44=GUESTROOM"
  ```

* mac_addresses_allowed
  If you have a lot of BTLE devices and *only* want to proxy a portion of them, specify them like this. Allowed MACs are processed before renames.

  Examples:

  ```mac_addresses_allowed: ["A4:C1:38:00:11:22", "A4:C1:38:00:22:44"]```

  ```
  mac_addresses_allowed:
   - "A4:C1:38:00:11:22"
   - "A4:C1:38:00:22:44"
  ```

* mac_addresses_blocked
  If you have a lot of BTLE devices and *don't* want to proxy a portion of them, specify them like this. All other devices are proxied. Blocked MACs are processed before renames.

  Examples:

  ```mac_addresses_blocked: ["A4:C1:38:00:11:22", "A4:C1:38:00:22:44"]```

  ```
  mac_addresses_blocked:
   - "A4:C1:38:00:11:22"
   - "A4:C1:38:00:22:44"
  ```

* notify_interval
  Collects sensor values over this period of time and sends the average value over this time.

  Example:
  ```notify_interval: 15min```


## Supported BTLE devices

Theoretically this supports various Xiaomi BTLE devices. I only have the thermometers. Aliexpress or a local electronics shop is your friend.

This code proxies EspNow measurements for:

* temperature
* battery-level

I didn't need the humidity

