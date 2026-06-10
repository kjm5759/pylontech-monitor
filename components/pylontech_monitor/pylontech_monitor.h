#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pylontech_monitor {

static const uint8_t  MAX_BATTERIES = 10;
static const uint8_t  MAX_CELLS     = 15;
static const uint16_t BUFFER_SIZE   = 4096;

#define PYLONTECH_SENSOR(name) \
  sensor::Sensor *name##_sensor_{nullptr}; \
  void set_##name##_sensor(sensor::Sensor *s) { name##_sensor_ = s; }

// ---------------------------------------------------------------------------
// Batterie individuelle
// ---------------------------------------------------------------------------
class PylontechBattery {
 public:
  PYLONTECH_SENSOR(voltage)
  PYLONTECH_SENSOR(current)
  PYLONTECH_SENSOR(temperature)
  PYLONTECH_SENSOR(temp_high)
  PYLONTECH_SENSOR(temp_low)
  PYLONTECH_SENSOR(cell_voltage_high)
  PYLONTECH_SENSOR(cell_voltage_low)
  PYLONTECH_SENSOR(soc)
  PYLONTECH_SENSOR(cycle_count)
  PYLONTECH_SENSOR(coulomb)
  PYLONTECH_SENSOR(capacity)
  PYLONTECH_SENSOR(soh)

  sensor::Sensor *cell_sensors[MAX_CELLS]{};
  void set_cell_sensor(uint8_t index, sensor::Sensor *s) {
    if (index < MAX_CELLS) cell_sensors[index] = s;
  }
};

// ---------------------------------------------------------------------------
// Composant principal
//
// Deux cadences :
//   Rapide (update_interval, défaut 2s)  : pwrsys + pwr
//   Lent   (slow_interval,   défaut 60s) : getpwr N + stat N
//
// Relais intégré optionnel (fonctionne sans Home Assistant) :
//   Activé  si  SOC >= soc_threshold  ET  tension >= voltage_threshold
//   Désactivé si SOC < (soc_threshold - hysteresis)
// ---------------------------------------------------------------------------
class PylontechMonitorComponent : public PollingComponent, public uart::UARTDevice {
 public:
  PylontechMonitorComponent() = default;

  // ── Capteurs système ─────────────────────────────────────
  SUB_SENSOR(system_voltage)
  SUB_SENSOR(system_current)
  SUB_SENSOR(system_soc)
  SUB_SENSOR(system_soh)
  SUB_SENSOR(system_capacity)
  SUB_SENSOR(system_capacity_total)
  SUB_SENSOR(installed_batteries)
  SUB_SENSOR(present_batteries)
  SUB_SENSOR(charge_current)
  SUB_SENSOR(discharge_current)
  SUB_SENSOR(charge_power)
  SUB_SENSOR(discharge_power)
  SUB_SENSOR(available_energy)

  // ── Accès batteries ──────────────────────────────────────
  PylontechBattery *get_battery(uint8_t index) {
    if (index < MAX_BATTERIES) return &batteries_[index];
    return nullptr;
  }

  // ── Configuration ────────────────────────────────────────
  void set_battery_count(uint8_t count)     { battery_count_        = count; }
  void set_slow_interval(uint32_t ms)        { slow_interval_ms_     = ms;   }
  void set_relay_pin(InternalGPIOPin *pin)   { relay_pin_            = pin;  }
  void set_relay_soc_threshold(float v)      { relay_soc_threshold_  = v;    }
  void set_relay_voltage_threshold(float v)  { relay_volt_threshold_ = v;    }
  void set_relay_hysteresis(float v)         { relay_hysteresis_     = v;    }

  // ── Cycle de vie ESPHome ─────────────────────────────────
  void setup() override;
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void dump_config() override;

 protected:
  enum class QueryPhase : uint8_t {
    IDLE = 0,
    SEND_PWRSYS, WAIT_PWRSYS,
    SEND_INFO,   WAIT_INFO,
    SEND_CELLS,  WAIT_CELLS,
    SEND_STAT,   WAIT_STAT,
  };

  // UART
  char     rx_buf_[BUFFER_SIZE];
  uint16_t rx_len_{0};
  uint32_t last_rx_ms_{0};
  static constexpr uint32_t RX_SILENCE_MS  = 150;
  static constexpr uint32_t CMD_TIMEOUT_MS = 2000;

  // Séquenceur
  QueryPhase phase_{QueryPhase::IDLE};
  uint8_t    current_bat_cmd_{0};
  uint32_t   phase_deadline_ms_{0};

  // Cycle lent
  uint32_t slow_interval_ms_{60000};
  uint32_t last_slow_ms_{0};
  bool     slow_pending_{false};

  // Relais intégré
  InternalGPIOPin *relay_pin_{nullptr};
  float relay_soc_threshold_{100.0f};
  float relay_volt_threshold_{51.2f};
  float relay_hysteresis_{2.0f};
  bool  relay_state_{false};
  float last_soc_{-1.0f};
  float last_sys_volt_{0.0f};

  void update_relay_();
  void send_command_(const char *cmd);
  void advance_phase_();
  void process_buffer_();
  void parse_pwr_table_();
  void parse_system_info_();
  void parse_cells_(int bat_idx);
  void parse_stat_(int bat_idx);
  void publish_pwr_row_(int bat_idx, float v, float c, float t,
                        float tl, float th, float vl, float vh, int soc);

  PylontechBattery batteries_[MAX_BATTERIES];
  uint8_t          battery_count_{1};
};

}  // namespace pylontech_monitor
}  // namespace esphome
