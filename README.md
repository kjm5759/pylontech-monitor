# ESPHome Pylontech Monitor

ESPHome external component to monitor Pylontech battery racks (US2000, US3000, US5000) via console port.

Supports up to **10 batteries × 15 cells**, dual update rate, and an optional integrated relay (works without Home Assistant).

---

## Features

- Full rack monitoring: voltage, current, SOC, SOH, capacity, power
- Per-battery: voltage, current, temperature, cell voltages (×15), cycles, coulomb, SOH
- Dual update rate:
  - **Fast** (default 2s): `pwrsys` + `pwr` → SOC / voltage / current for automations
  - **Slow** (default 60s): `getpwr` + `stat` → cell voltages, cycle count, coulomb
- Optional **integrated relay** — triggers directly on the ESP32 without Home Assistant
- WiFi or **Ethernet W5500** (SPI) via packages
- Tested on ESP32-S3 with Pylontech US2000 and US5000

---

## Hardware

Connect the ESP32 UART to the Pylontech **RS485 console port** (RJ45 on master battery):

| Pylontech RJ45 | ESP32-S3    |
|----------------|-------------|
| TX (pin 3)     | RX (GPIO1)  |
| RX (pin 6)     | TX (GPIO2)  |
| GND (pin 5)    | GND         |

> Use a RS232-to-TTL adapter if your Pylontech outputs RS232 levels.

### Optional: Ethernet W5500 (SPI)

| W5500 | ESP32-S3   |
|-------|------------|
| MOSI  | GPIO11     |
| MISO  | GPIO13     |
| SCLK  | GPIO12     |
| CS    | GPIO10     |
| INT   | GPIO9      |
| RST   | GPIO14     |
| 3.3V  | 3.3V       |
| GND   | GND        |

---

## Installation

### 1. Add to your ESPHome YAML

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/kjm5759/pylontech-monitor
      ref: main
    components: [pylontech_monitor]
```

### 2. Configure the component

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO2
  rx_pin: GPIO1
  baud_rate: 115200
  rx_buffer_size: 8192

pylontech_monitor:
  uart_id: uart_bus
  battery_count: 3        # number of batteries in your rack (1-10)
  update_interval: 2s     # fast cycle: pwrsys + pwr
  slow_interval: 60s      # slow cycle: getpwr + stat

  system_soc:
    name: "Pylontech SOC"
  system_voltage:
    name: "Pylontech Voltage"
  system_current:
    name: "Pylontech Current"
  charge_power:
    name: "Pylontech Charge Power"
  discharge_power:
    name: "Pylontech Discharge Power"

  batteries:
    - voltage:     { name: "Bat 1 Voltage" }
      current:     { name: "Bat 1 Current" }
      temperature: { name: "Bat 1 Temperature" }
      soc:         { name: "Bat 1 SOC" }
      cycle_count: { name: "Bat 1 Cycles" }
      cells:
        - { name: "Bat 1 Cell 1" }
        - { name: "Bat 1 Cell 2" }
        # ... up to 15 cells
```

### 3. Network — WiFi or Ethernet

**WiFi** (default):
```yaml
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

**Ethernet W5500** (recommended for stability):
```yaml
ethernet:
  type: W5500
  clk_pin: GPIO12
  mosi_pin: GPIO11
  miso_pin: GPIO13
  cs_pin: GPIO10
  interrupt_pin: GPIO9
  reset_pin: GPIO14
```

Using `packages:` makes it easy to switch — see the example YAML files in this repository.

---

## Optional: Integrated Relay

The relay triggers directly on the ESP32 — no Home Assistant required.  
Useful for standalone installations (e.g. water heater on solar surplus).

```yaml
pylontech_monitor:
  # ...
  relay:
    pin: GPIO5
    soc_threshold: 100      # activate when SOC >= 100%
    voltage_threshold: 51.2 # and voltage >= 51.2V
    hysteresis: 2.0         # deactivate when SOC < 98%
```

**Logic:**
- **ON** if `SOC >= soc_threshold` AND `voltage >= voltage_threshold`
- **OFF** if `SOC < (soc_threshold - hysteresis)` OR `voltage < voltage_threshold`

### Alternative: Home Assistant Automation

If you prefer to manage the relay in HA:

```yaml
# ESPHome
switch:
  - platform: gpio
    pin: GPIO5
    name: "Water heater relay"
    id: relay_water_heater

# Home Assistant automation
automation:
  trigger:
    platform: numeric_state
    entity_id: sensor.pylontech_soc
    above: 99
  condition:
    - condition: numeric_state
      entity_id: sensor.pylontech_voltage
      above: 51.2
  action:
    - switch.turn_on: relay_water_heater
```

---

## Full Configuration Parameters

| Parameter          | Default | Description                              |
|--------------------|---------|------------------------------------------|
| `battery_count`    | 1       | Number of batteries in rack (1–10)       |
| `update_interval`  | 2s      | Fast cycle interval (pwrsys + pwr)       |
| `slow_interval`    | 60s     | Slow cycle interval (getpwr + stat)      |
| `relay.pin`        | —       | GPIO pin for integrated relay (optional) |
| `relay.soc_threshold`   | 100.0 | SOC % to activate relay             |
| `relay.voltage_threshold` | 51.2 | Pack voltage (V) to activate relay |
| `relay.hysteresis` | 2.0     | SOC % hysteresis to deactivate relay     |

### System sensors

| Key                    | Unit | Description                    |
|------------------------|------|--------------------------------|
| `system_voltage`       | V    | Pack voltage                   |
| `system_current`       | A    | Pack current (+ charge, - discharge) |
| `system_soc`           | %    | State of Charge                |
| `system_soh`           | %    | State of Health                |
| `system_capacity`      | Ah   | Remaining capacity (RC)        |
| `system_capacity_total`| Ah   | Full charge capacity (FCC)     |
| `installed_batteries`  |      | Total batteries configured     |
| `present_batteries`    |      | Batteries currently online     |
| `charge_current`       | A    | Charge current (positive only) |
| `discharge_current`    | A    | Discharge current (positive)   |
| `charge_power`         | W    | Charge power                   |
| `discharge_power`      | W    | Discharge power                |
| `available_energy`     | Wh   | Available energy (V × RC)      |

### Per-battery sensors

| Key                 | Unit | Description              |
|---------------------|------|--------------------------|
| `voltage`           | V    | Battery voltage          |
| `current`           | A    | Battery current          |
| `temperature`       | °C   | Average temperature      |
| `temp_high`         | °C   | Highest temperature      |
| `temp_low`          | °C   | Lowest temperature       |
| `cell_voltage_high` | V    | Highest cell voltage     |
| `cell_voltage_low`  | V    | Lowest cell voltage      |
| `soc`               | %    | State of Charge          |
| `cycle_count`       | cycles | Charge cycle count     |
| `coulomb`           | C    | Cumulative coulomb count |
| `capacity`          | Ah   | Remaining capacity       |
| `soh`               | %    | State of Health          |
| `cells`             | V    | List of up to 15 cell voltages |

---

## Tested Hardware

| Battery model | Cells | Firmware     | Status |
|---------------|-------|--------------|--------|
| US2000C       | 15    | V2.8 / V3.x  | ✅     |
| US5000        | 15    | V2.8 / V3.x  | ✅     |

---

## Contributing

Issues and pull requests are welcome.  
If you test with other Pylontech models (US3000, Force H2...), please open an issue with your `pwrsys` output so we can verify compatibility.

---

## License

MIT — free to use, modify and distribute.

---

## Credits

Based on original work by [@kjm5759](https://github.com/kjm5759).  
Structured as an ESPHome official-style external component.
