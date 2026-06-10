"""ESPHome component for Pylontech RS485 battery racks (up to 10 units).

Supports:
  - Up to 10 batteries × 15 cells
  - Dual update rate: fast (pwrsys + pwr) / slow (getpwr + stat)
  - Optional integrated relay (works without Home Assistant)
  - WiFi or Ethernet (W5500) via packages
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_PIN,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome import pins

DEPENDENCIES = ["uart"]
AUTO_LOAD  = ["sensor"]

pylontech_monitor_ns = cg.esphome_ns.namespace("pylontech_monitor")
PylontechMonitorComponent = pylontech_monitor_ns.class_(
    "PylontechMonitorComponent",
    cg.PollingComponent,
    uart.UARTDevice,
)

# ---------------------------------------------------------------------------
# Clés de configuration
# ---------------------------------------------------------------------------
CONF_BATTERY_COUNT          = "battery_count"
CONF_SLOW_INTERVAL          = "slow_interval"

# Capteurs système
CONF_SYSTEM_VOLTAGE         = "system_voltage"
CONF_SYSTEM_CURRENT         = "system_current"
CONF_SYSTEM_SOC             = "system_soc"
CONF_SYSTEM_SOH             = "system_soh"
CONF_SYSTEM_CAPACITY        = "system_capacity"
CONF_SYSTEM_CAPACITY_TOTAL  = "system_capacity_total"
CONF_INSTALLED_BATTERIES    = "installed_batteries"
CONF_PRESENT_BATTERIES      = "present_batteries"
CONF_CHARGE_CURRENT         = "charge_current"
CONF_DISCHARGE_CURRENT      = "discharge_current"
CONF_CHARGE_POWER           = "charge_power"
CONF_DISCHARGE_POWER        = "discharge_power"
CONF_AVAILABLE_ENERGY       = "available_energy"

# Capteurs par batterie
CONF_BATTERIES  = "batteries"
CONF_VOLTAGE    = "voltage"
CONF_CURRENT    = "current"
CONF_TEMPERATURE= "temperature"
CONF_TEMP_HIGH  = "temp_high"
CONF_TEMP_LOW   = "temp_low"
CONF_CELL_HIGH  = "cell_voltage_high"
CONF_CELL_LOW   = "cell_voltage_low"
CONF_BAT_SOC    = "soc"
CONF_CYCLE_COUNT= "cycle_count"
CONF_COULOMB    = "coulomb"
CONF_CAPACITY   = "capacity"
CONF_SOH        = "soh"
CONF_CELLS      = "cells"

# Relais intégré
CONF_RELAY              = "relay"
CONF_RELAY_SOC_THRESHOLD  = "soc_threshold"
CONF_RELAY_VOLT_THRESHOLD = "voltage_threshold"
CONF_RELAY_HYSTERESIS     = "hysteresis"

# ---------------------------------------------------------------------------
# Schéma relais intégré
# ---------------------------------------------------------------------------
RELAY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RELAY_SOC_THRESHOLD,  default=100.0): cv.float_range(min=0, max=100),
        cv.Optional(CONF_RELAY_VOLT_THRESHOLD, default=51.2):  cv.float_range(min=0, max=60),
        cv.Optional(CONF_RELAY_HYSTERESIS,     default=2.0):   cv.float_range(min=0, max=20),
    }
)

# ---------------------------------------------------------------------------
# Schéma d'une batterie individuelle
# ---------------------------------------------------------------------------
BATTERY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_TEMP_HIGH): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_TEMP_LOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_CELL_HIGH): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),
        cv.Optional(CONF_CELL_LOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),
        cv.Optional(CONF_BAT_SOC): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_CYCLE_COUNT): sensor.sensor_schema(
            unit_of_measurement="cycles",
            icon="mdi:counter",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_COULOMB): sensor.sensor_schema(
            unit_of_measurement="Ah",
            icon="mdi:battery-charging",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_CAPACITY): sensor.sensor_schema(
            unit_of_measurement="Ah",
            icon="mdi:battery",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_SOH): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:heart-battery",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_CELLS): cv.All(
            cv.ensure_list(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_VOLT,
                    device_class=DEVICE_CLASS_VOLTAGE,
                    state_class=STATE_CLASS_MEASUREMENT,
                    accuracy_decimals=3,
                )
            ),
            cv.Length(min=1, max=15),
        ),
    }
)

# ---------------------------------------------------------------------------
# Schéma principal
# ---------------------------------------------------------------------------
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PylontechMonitorComponent),

            cv.Optional(CONF_BATTERY_COUNT, default=1): cv.int_range(min=1, max=10),
            cv.Optional(CONF_SLOW_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,

            # Relais intégré (optionnel)
            cv.Optional(CONF_RELAY): RELAY_SCHEMA,

            # Capteurs système
            cv.Optional(CONF_SYSTEM_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=3,
            ),
            cv.Optional(CONF_SYSTEM_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=3,
            ),
            cv.Optional(CONF_SYSTEM_SOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_SYSTEM_SOH): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon="mdi:heart-battery",
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_SYSTEM_CAPACITY): sensor.sensor_schema(
                unit_of_measurement="Ah",
                icon="mdi:battery",
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_SYSTEM_CAPACITY_TOTAL): sensor.sensor_schema(
                unit_of_measurement="Ah",
                icon="mdi:battery-high",
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_INSTALLED_BATTERIES): sensor.sensor_schema(
                unit_of_measurement="",
                icon="mdi:battery-multiple",
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_PRESENT_BATTERIES): sensor.sensor_schema(
                unit_of_measurement="",
                icon="mdi:battery-multiple",
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_CHARGE_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=3,
            ),
            cv.Optional(CONF_DISCHARGE_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=3,
            ),
            cv.Optional(CONF_CHARGE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_DISCHARGE_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_AVAILABLE_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),

            cv.Optional(CONF_BATTERIES): cv.All(
                cv.ensure_list(BATTERY_SCHEMA),
                cv.Length(min=1, max=10),
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("2s"))
)

# ---------------------------------------------------------------------------
# Setters capteurs système
# ---------------------------------------------------------------------------
SYSTEM_SENSORS = [
    (CONF_SYSTEM_VOLTAGE,        "set_system_voltage_sensor"),
    (CONF_SYSTEM_CURRENT,        "set_system_current_sensor"),
    (CONF_SYSTEM_SOC,            "set_system_soc_sensor"),
    (CONF_SYSTEM_SOH,            "set_system_soh_sensor"),
    (CONF_SYSTEM_CAPACITY,       "set_system_capacity_sensor"),
    (CONF_SYSTEM_CAPACITY_TOTAL, "set_system_capacity_total_sensor"),
    (CONF_INSTALLED_BATTERIES,   "set_installed_batteries_sensor"),
    (CONF_PRESENT_BATTERIES,     "set_present_batteries_sensor"),
    (CONF_CHARGE_CURRENT,        "set_charge_current_sensor"),
    (CONF_DISCHARGE_CURRENT,     "set_discharge_current_sensor"),
    (CONF_CHARGE_POWER,          "set_charge_power_sensor"),
    (CONF_DISCHARGE_POWER,       "set_discharge_power_sensor"),
    (CONF_AVAILABLE_ENERGY,      "set_available_energy_sensor"),
]

BATTERY_SENSORS = [
    (CONF_VOLTAGE,      "set_voltage_sensor"),
    (CONF_CURRENT,      "set_current_sensor"),
    (CONF_TEMPERATURE,  "set_temperature_sensor"),
    (CONF_TEMP_HIGH,    "set_temp_high_sensor"),
    (CONF_TEMP_LOW,     "set_temp_low_sensor"),
    (CONF_CELL_HIGH,    "set_cell_voltage_high_sensor"),
    (CONF_CELL_LOW,     "set_cell_voltage_low_sensor"),
    (CONF_BAT_SOC,      "set_soc_sensor"),
    (CONF_CYCLE_COUNT,  "set_cycle_count_sensor"),
    (CONF_COULOMB,      "set_coulomb_sensor"),
    (CONF_CAPACITY,     "set_capacity_sensor"),
    (CONF_SOH,          "set_soh_sensor"),
]

# ---------------------------------------------------------------------------
# to_code()
# ---------------------------------------------------------------------------
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_battery_count(config[CONF_BATTERY_COUNT]))
    cg.add(var.set_slow_interval(config[CONF_SLOW_INTERVAL]))

    # Relais intégré
    if CONF_RELAY in config:
        relay_conf = config[CONF_RELAY]
        relay_pin  = await cg.gpio_pin_expression(relay_conf[CONF_PIN])
        cg.add(var.set_relay_pin(relay_pin))
        cg.add(var.set_relay_soc_threshold(relay_conf[CONF_RELAY_SOC_THRESHOLD]))
        cg.add(var.set_relay_voltage_threshold(relay_conf[CONF_RELAY_VOLT_THRESHOLD]))
        cg.add(var.set_relay_hysteresis(relay_conf[CONF_RELAY_HYSTERESIS]))

    # Capteurs système
    for conf_key, setter in SYSTEM_SENSORS:
        if conf_key in config:
            sens = await sensor.new_sensor(config[conf_key])
            cg.add(getattr(var, setter)(sens))

    # Capteurs par batterie
    if CONF_BATTERIES in config:
        for bat_index, bat_conf in enumerate(config[CONF_BATTERIES]):
            for conf_key, setter in BATTERY_SENSORS:
                if conf_key in bat_conf:
                    sens = await sensor.new_sensor(bat_conf[conf_key])
                    cg.add(cg.RawExpression(
                        f"{var}->get_battery({bat_index})->{setter}({sens})"
                    ))
            if CONF_CELLS in bat_conf:
                for cell_index, cell_conf in enumerate(bat_conf[CONF_CELLS]):
                    sens = await sensor.new_sensor(cell_conf)
                    cg.add(cg.RawExpression(
                        f"{var}->get_battery({bat_index})->set_cell_sensor({cell_index}, {sens})"
                    ))
