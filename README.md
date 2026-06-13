ESPHome Pylontech Monitor

ESPHome external component to monitor Pylontech battery racks (US2000, US3000, US5000) via the console port (RS232 TTL) of the master battery.

Supports up to 10 batteries × 15 cells, dual update rate, and an optional integrated relay (works without Home Assistant).


Features


Full rack monitoring: voltage, current, SOC, SOH, capacity, power
Per-battery: voltage, current, temperature, cell voltages (×15), cycles, coulomb, SOH
Dual update rate:

Fast (default 2s): pwrsys + pwr → SOC / voltage / current for automations
Slow (default 60s): getpwr + stat → cell voltages, cycle count, coulomb



Optional integrated relay — triggers directly on the ESP32 without Home Assistant
WiFi or Ethernet W5500 (SPI) via packages
Tested on ESP32-S3 with Pylontech US2000 and US5000



Hardware


⚠️ This component uses the console port of the Pylontech master battery, not the CAN or RS485 BMS port.



Connect the ESP32 UART to the console port of the Pylontech master battery (RJ45 connector).

The console port outputs RS232 TTL levels — you need a RS232-to-TTL adapter (MAX3232 or similar) between the Pylontech and the ESP32.

Pylontech console (RJ45)RS232-TTL adapterESP32-S3TX (pin 3)RX—RX (pin 6)TX—GND (pin 5)GNDGND—TXD (TTL out)RX (GPIO1)—RXD (TTL in)TX (GPIO2)


The console port uses the Pylontech CLI (commands: pwrsys, pwr, getpwr N, stat N).

Only the master battery (address 1) exposes this port — all modules in the rack respond.




⚠️ Important: Baud Rate Initialization (US2000B master)

The US5000 and recent US2000C start directly at 115200 bauds — no initialization needed.

However, older US2000B modules start at 1200 bauds after a power cycle. If your master battery is a US2000B, you must send a specific initialization sequence at 1200 bauds to switch it to 115200 before normal communication can start.

The initialization sequence (PYLON binary protocol at 1200 bauds):

7E 32 30 30 31 34 36 38 32 43 30 30 34 38 35 32 30 46 43 43 33 0D

Then switch to 115200 and send:

login debug

This initialization is not yet implemented in the current version of the component (not needed for US5000/US2000C). If you have a US2000B master battery, please open an issue — implementation is planned for a future release.

How to identify your situation:


If your master is a US5000 → no action needed, works out of the box
If your master is a US2000B and the component receives no data → your battery is likely stuck at 1200 bauds. You can manually initialize it once using a serial terminal at 1200 bauds, send the sequence above, and it will stay at 115200 until next power cycle.



Optional: Ethernet W5500 (SPI)

W5500ESP32-S3MOSIGPIO11MISOGPIO13SCLKGPIO12CSGPIO10INTGPIO9RSTGPIO143.3V3.3VGNDGND


Installation

1. Add to your ESPHome YAML

yamlexternal_components:
  - source:
      type: git
      url: https://github.com/kjm5759/pylontech-monitor
      ref: main
    components: [pylontech_monitor]

2. Configure the component

yamluart:
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

3. Network — WiFi or Ethernet

WiFi (default):

yamlwifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

Ethernet W5500 (recommended for stability):

yamlethernet:
  type: W5500
  clk_pin: GPIO12
  mosi_pin: GPIO11
  miso_pin: GPIO13
  cs_pin: GPIO10
  interrupt_pin: GPIO9
  reset_pin: GPIO14

Using packages: makes it easy to switch — see the example YAML files in this repository.


Optional: Integrated Relay

The relay triggers directly on the ESP32 — no Home Assistant required.

Useful for standalone installations (e.g. water heater on solar surplus).

yamlpylontech_monitor:
  # ...
  relay:
    pin: GPIO5
    soc_threshold: 100      # activate when SOC >= 100%
    voltage_threshold: 51.2 # and voltage >= 51.2V
    hysteresis: 2.0         # deactivate when SOC < 98%

Logic:


ON if SOC >= soc_threshold AND voltage >= voltage_threshold
OFF if SOC < (soc_threshold - hysteresis) OR voltage < voltage_threshold


Alternative: Home Assistant Automation

If you prefer to manage the relay in HA:

yaml# ESPHome
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


Full Configuration Parameters

ParameterDefaultDescriptionbattery_count1Number of batteries in rack (1–10)update_interval2sFast cycle interval (pwrsys + pwr)slow_interval60sSlow cycle interval (getpwr + stat)relay.pin—GPIO pin for integrated relay (optional)relay.soc_threshold100.0SOC % to activate relayrelay.voltage_threshold51.2Pack voltage (V) to activate relayrelay.hysteresis2.0SOC % hysteresis to deactivate relay

System sensors

KeyUnitDescriptionsystem_voltageVPack voltagesystem_currentAPack current (+ charge, - discharge)system_soc%State of Chargesystem_soh%State of Healthsystem_capacityAhRemaining capacity (RC)system_capacity_totalAhFull charge capacity (FCC)installed_batteriesTotal batteries configuredpresent_batteriesBatteries currently onlinecharge_currentACharge current (positive only)discharge_currentADischarge current (positive)charge_powerWCharge powerdischarge_powerWDischarge poweravailable_energyWhAvailable energy (V × RC)

Per-battery sensors

KeyUnitDescriptionvoltageVBattery voltagecurrentABattery currenttemperature°CAverage temperaturetemp_high°CHighest temperaturetemp_low°CLowest temperaturecell_voltage_highVHighest cell voltagecell_voltage_lowVLowest cell voltagesoc%State of Chargecycle_countcyclesCharge cycle countcoulombAhCumulative coulomb countcapacityAhRemaining capacitysoh%State of HealthcellsVList of up to 15 cell voltages


Tested Hardware

Battery modelCellsFirmwareStatusUS2000C15V2.8 / V3.x✅US500015V2.8 / V3.x✅


Contributing

Issues and pull requests are welcome.

If you test with other Pylontech models (US3000, Force H2...), please open an issue with your pwrsys output so we can verify compatibility.


License

MIT — free to use, modify and distribute.


Credits

Based on original work by @kjm5759.

Structured as an ESPHome official-style external component.
