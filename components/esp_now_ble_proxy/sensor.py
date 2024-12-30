import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_BATTERY_LEVEL, CONF_HUMIDITY, CONF_MAC_ADDRESS, \
    CONF_TEMPERATURE, UNIT_CELSIUS, ICON_THERMOMETER, UNIT_PERCENT, ICON_WATER_PERCENT, \
    ICON_BATTERY, CONF_ID

from esphome import automation
from esphome.automation import maybe_simple_id

CODEOWNERS = ['@johnmu', '@ahpohl']

DEPENDENCIES = ['esp32_ble_tracker']
AUTO_LOAD = ['xiaomi_ble']

CONF_HOSTNAME = 'hostname'
CONF_BROADCAST_ADDRESS = 'broadcast_address'
CONF_SECURITY_KEY = 'security_key'
CONF_MACS_ALLOWED = 'mac_addresses_allowed'
CONF_MACS_DISALLOWED = 'mac_addresses_blocked'
CONF_MACS_RENAME = 'mac_addresses_renamed'
CONF_AUTO_REBOOT = 'auto_reboot_interval'
CONF_NOTIFY_INTERVAL = 'notify_interval'

ble_proxy_ns = cg.esphome_ns.namespace('esp_now_ble_proxy')
ESP_NOW_BLE_PROXY = ble_proxy_ns.class_('ESP_NOW_BLE_PROXY',
                        esp32_ble_tracker.ESPBTDeviceListener,
                        cg.Component)
BleEnableAction = ble_proxy_ns.class_("BleEnableAction", automation.Action)
BleDisableAction = ble_proxy_ns.class_("BleDisableAction", automation.Action)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP_NOW_BLE_PROXY),
    cv.Required(CONF_HOSTNAME): cv.hostname,
    cv.Required(CONF_BROADCAST_ADDRESS): cv.string,
    cv.Required(CONF_SECURITY_KEY): cv.string,
    cv.Optional(CONF_MACS_ALLOWED): cv.ensure_list(cv.string),
    cv.Optional(CONF_MACS_DISALLOWED): cv.ensure_list(cv.string),
    cv.Optional(CONF_MACS_RENAME): cv.ensure_list(cv.string),
    cv.Optional(CONF_AUTO_REBOOT): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_NOTIFY_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_hostname(config[CONF_HOSTNAME]))
    cg.add(var.set_broadcast_address(config[CONF_BROADCAST_ADDRESS]))
    cg.add(var.set_security_key(config[CONF_SECURITY_KEY]))
    
    if CONF_MACS_ALLOWED in config:
        for item in config[CONF_MACS_ALLOWED]:
            cg.add(var.add_macs_allowed(item))
    if CONF_MACS_DISALLOWED in config:
        for item in config[CONF_MACS_DISALLOWED]:
            cg.add(var.add_macs_disallowed(item))
    if CONF_MACS_RENAME in config:
        for item in config[CONF_MACS_RENAME]:
            cg.add(var.add_macs_renamed(item))
    if CONF_AUTO_REBOOT in config:
        cg.add(var.set_reboot_interval(config[CONF_AUTO_REBOOT]))
    if CONF_NOTIFY_INTERVAL in config:
        cg.add(var.set_notify_interval(config[CONF_NOTIFY_INTERVAL]))

CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(ESP_NOW_BLE_PROXY),
    }
)

@automation.register_action(
    "esp_now_ble_proxy.ble_enable", BleEnableAction, CALIBRATION_ACTION_SCHEMA
)
@automation.register_action(
    "esp_now_ble_proxy.ble_disable", BleDisableAction, CALIBRATION_ACTION_SCHEMA
)
async def ble_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
