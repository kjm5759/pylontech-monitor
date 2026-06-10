#include "pylontech_monitor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylontech_monitor {

static const char *const TAG = "pylontech_monitor";

// ============================================================
// setup()
// ============================================================
void PylontechMonitorComponent::setup() {
  rx_len_       = 0;
  memset(rx_buf_, 0, BUFFER_SIZE);
  phase_        = QueryPhase::IDLE;
  last_slow_ms_ = millis();

  if (relay_pin_ != nullptr) {
    relay_pin_->setup();
    relay_pin_->digital_write(false);
    relay_state_ = false;
    ESP_LOGCONFIG(TAG, "Relay pin configured — initially OFF");
  }
  ESP_LOGCONFIG(TAG, "Pylontech Monitor initialized (%d batteries)", battery_count_);
}

// ============================================================
// dump_config()
// ============================================================
void PylontechMonitorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Pylontech Monitor:");
  ESP_LOGCONFIG(TAG, "  Battery count : %d", battery_count_);
  ESP_LOGCONFIG(TAG, "  Fast interval : %dms", this->get_update_interval());
  ESP_LOGCONFIG(TAG, "  Slow interval : %dms", slow_interval_ms_);
  if (relay_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Relay pin     : configured");
    ESP_LOGCONFIG(TAG, "  SOC threshold : %.0f%%", relay_soc_threshold_);
    ESP_LOGCONFIG(TAG, "  Volt threshold: %.2fV",  relay_volt_threshold_);
    ESP_LOGCONFIG(TAG, "  Hysteresis    : %.1f%%", relay_hysteresis_);
  } else {
    ESP_LOGCONFIG(TAG, "  Relay         : not configured");
  }
  LOG_SENSOR("  ", "System voltage",  this->system_voltage_sensor_);
  LOG_SENSOR("  ", "System current",  this->system_current_sensor_);
  LOG_SENSOR("  ", "System SOC",      this->system_soc_sensor_);
  LOG_SENSOR("  ", "System SOH",      this->system_soh_sensor_);
  LOG_SENSOR("  ", "Charge power",    this->charge_power_sensor_);
  LOG_SENSOR("  ", "Discharge power", this->discharge_power_sensor_);
}

// ============================================================
// update() — cycle RAPIDE
// ============================================================
void PylontechMonitorComponent::update() {
  if (phase_ == QueryPhase::IDLE) {
    phase_ = QueryPhase::SEND_PWRSYS;
    advance_phase_();
  }
}

// ============================================================
// loop()
// ============================================================
void PylontechMonitorComponent::loop() {
  // 1. Lecture UART
  while (this->available()) {
    char c = this->read();
    if (rx_len_ < BUFFER_SIZE - 1) {
      rx_buf_[rx_len_++] = c;
      rx_buf_[rx_len_]   = '\0';
      last_rx_ms_        = millis();
      if (c == '>' && rx_len_ >= 12 &&
          strstr(rx_buf_ + rx_len_ - 12, "pylon_debug>") != nullptr) {
        process_buffer_();
        advance_phase_();
        return;
      }
    }
  }

  // 2. Timeout silence UART
  if (rx_len_ > 0 && (millis() - last_rx_ms_) > RX_SILENCE_MS) {
    process_buffer_();
    advance_phase_();
    return;
  }

  // 3. Cycle lent
  if (millis() - last_slow_ms_ >= slow_interval_ms_) {
    last_slow_ms_ = millis();
    slow_pending_ = true;
  }
  if (slow_pending_ && phase_ == QueryPhase::IDLE) {
    slow_pending_    = false;
    current_bat_cmd_ = 1;
    phase_           = QueryPhase::SEND_CELLS;
    advance_phase_();
    return;
  }

  // 4. Timeout de phase
  if (phase_ != QueryPhase::IDLE        &&
      phase_ != QueryPhase::SEND_PWRSYS &&
      phase_ != QueryPhase::SEND_INFO   &&
      phase_ != QueryPhase::SEND_CELLS  &&
      phase_ != QueryPhase::SEND_STAT) {
    if (millis() > phase_deadline_ms_) {
      ESP_LOGW(TAG, "Timeout phase %d (bat %d)", (int)phase_, current_bat_cmd_);
      advance_phase_();
    }
  }
}

// ============================================================
// update_relay_() — logique relais avec hystérésis
//
//  ON  si SOC >= soc_threshold  ET  tension >= volt_threshold
//  OFF si SOC <  (soc_threshold - hysteresis)
//      ou tension < volt_threshold  (sécurité tension basse)
// ============================================================
void PylontechMonitorComponent::update_relay_() {
  if (relay_pin_ == nullptr || last_soc_ < 0.0f) return;

  bool should_on = (last_soc_      >= relay_soc_threshold_) &&
                   (last_sys_volt_ >= relay_volt_threshold_);

  bool should_off = (last_soc_      < (relay_soc_threshold_ - relay_hysteresis_)) ||
                    (last_sys_volt_ < relay_volt_threshold_);

  if (!relay_state_ && should_on) {
    relay_pin_->digital_write(true);
    relay_state_ = true;
    ESP_LOGI(TAG, "Relay ON  (SOC=%.0f%% V=%.2fV)", last_soc_, last_sys_volt_);
  } else if (relay_state_ && should_off) {
    relay_pin_->digital_write(false);
    relay_state_ = false;
    ESP_LOGI(TAG, "Relay OFF (SOC=%.0f%% V=%.2fV)", last_soc_, last_sys_volt_);
  }
}

// ============================================================
// send_command_()
// ============================================================
void PylontechMonitorComponent::send_command_(const char *cmd) {
  ESP_LOGD(TAG, "TX → %s", cmd);
  this->write_str(cmd);
  this->write_byte('\r');
  rx_len_ = 0;
  memset(rx_buf_, 0, BUFFER_SIZE);
  last_rx_ms_ = millis();
}

// ============================================================
// advance_phase_()
// ============================================================
void PylontechMonitorComponent::advance_phase_() {
  switch (phase_) {

    case QueryPhase::IDLE:
      break;

    // ── Cycle rapide ─────────────────────────────────────────
    case QueryPhase::SEND_PWRSYS:
      send_command_("pwrsys");
      phase_             = QueryPhase::WAIT_PWRSYS;
      phase_deadline_ms_ = millis() + CMD_TIMEOUT_MS;
      break;

    case QueryPhase::WAIT_PWRSYS:
      phase_ = QueryPhase::SEND_INFO;
      advance_phase_();
      break;

    case QueryPhase::SEND_INFO:
      send_command_("pwr");
      phase_             = QueryPhase::WAIT_INFO;
      phase_deadline_ms_ = millis() + CMD_TIMEOUT_MS;
      break;

    case QueryPhase::WAIT_INFO:
      ESP_LOGD(TAG, "Fast cycle complete");
      phase_ = QueryPhase::IDLE;
      break;

    // ── Cycle lent ───────────────────────────────────────────
    case QueryPhase::SEND_CELLS:
      if (current_bat_cmd_ <= battery_count_) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "getpwr %d", current_bat_cmd_);
        send_command_(cmd);
        phase_             = QueryPhase::WAIT_CELLS;
        phase_deadline_ms_ = millis() + CMD_TIMEOUT_MS;
      } else {
        current_bat_cmd_ = 1;
        phase_           = QueryPhase::SEND_STAT;
        advance_phase_();
      }
      break;

    case QueryPhase::WAIT_CELLS:
      current_bat_cmd_++;
      phase_ = QueryPhase::SEND_CELLS;
      advance_phase_();
      break;

    case QueryPhase::SEND_STAT:
      if (current_bat_cmd_ <= battery_count_) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "stat %d", current_bat_cmd_);
        send_command_(cmd);
        phase_             = QueryPhase::WAIT_STAT;
        phase_deadline_ms_ = millis() + CMD_TIMEOUT_MS;
      } else {
        ESP_LOGD(TAG, "Slow cycle complete");
        phase_ = QueryPhase::IDLE;
      }
      break;

    case QueryPhase::WAIT_STAT:
      current_bat_cmd_++;
      phase_ = QueryPhase::SEND_STAT;
      advance_phase_();
      break;
  }
}

// ============================================================
// process_buffer_()
// ============================================================
void PylontechMonitorComponent::process_buffer_() {
  if (rx_len_ == 0) return;
  ESP_LOGV(TAG, "RX %d bytes: %.80s", rx_len_, rx_buf_);

  if (strstr(rx_buf_, "Power System Inform") != nullptr) {
    parse_system_info_();
  } else if (strstr(rx_buf_, "Power Volt") != nullptr) {
    parse_pwr_table_();
  } else if (strstr(rx_buf_, "getpwr") != nullptr) {
    const char *p = strstr(rx_buf_, "getpwr ");
    if (p) parse_cells_(atoi(p + 7));
  } else if (strstr(rx_buf_, "CYCLE Times") != nullptr ||
             strstr(rx_buf_, "stat ") != nullptr) {
    const char *p = strstr(rx_buf_, "stat ");
    if (p) parse_stat_(atoi(p + 5));
  }

  rx_len_ = 0;
  memset(rx_buf_, 0, BUFFER_SIZE);
}

// ============================================================
// parse_pwr_table_() — réponse "pwr"
// ============================================================
void PylontechMonitorComponent::parse_pwr_table_() {
  const char *fin = rx_buf_ + rx_len_;
  const char *pos = strstr(rx_buf_, "Power Volt");
  if (!pos) return;

  while (pos < fin) {
    if (*pos == '\n' || *pos == '\r') {
      const char *line = pos + 1;
      while (line < fin && (*line == ' ' || *line == '\t')) line++;
      int bat_idx = atoi(line);
      if (bat_idx >= 1 && bat_idx <= (int)battery_count_) {
        int b_num = 0, bsoc = 0;
        float v = 0, c = 0, t = 0, tl = 0, th = 0, vl = 0, vh = 0;
        int r = sscanf(line, "%d %f %f %f %f %f %f %f %*s %*s %*s %*s %d",
                       &b_num, &v, &c, &t, &tl, &th, &vl, &vh, &bsoc);
        if (r >= 3)
          publish_pwr_row_(bat_idx,
                           v / 1000.0f, c / 1000.0f, t / 1000.0f,
                           tl / 1000.0f, th / 1000.0f,
                           vl / 1000.0f, vh / 1000.0f, bsoc);
      }
    }
    pos++;
  }
}

// ============================================================
// publish_pwr_row_()
// ============================================================
void PylontechMonitorComponent::publish_pwr_row_(int bat_idx,
                                                  float v, float c, float t,
                                                  float tl, float th,
                                                  float vl, float vh, int soc) {
  if (bat_idx < 1 || bat_idx > MAX_BATTERIES) return;
  PylontechBattery &bat = batteries_[bat_idx - 1];
  if (bat.voltage_sensor_)           bat.voltage_sensor_->publish_state(v);
  if (bat.current_sensor_)           bat.current_sensor_->publish_state(c);
  if (bat.temperature_sensor_)       bat.temperature_sensor_->publish_state(t);
  if (bat.temp_low_sensor_)          bat.temp_low_sensor_->publish_state(tl);
  if (bat.temp_high_sensor_)         bat.temp_high_sensor_->publish_state(th);
  if (bat.cell_voltage_low_sensor_)  bat.cell_voltage_low_sensor_->publish_state(vl);
  if (bat.cell_voltage_high_sensor_) bat.cell_voltage_high_sensor_->publish_state(vh);
  if (bat.soc_sensor_)               bat.soc_sensor_->publish_state((float)soc);
}

// ============================================================
// parse_system_info_() — réponse "pwrsys"
// ============================================================
void PylontechMonitorComponent::parse_system_info_() {
  const char *fin = rx_buf_ + rx_len_;
  const char *match;
  float sys_volt = 0.0f, sys_curr = 0.0f, sys_cap = 0.0f;
  bool has_volt = false, has_curr = false, has_cap = false;

  auto skip_num = [&](const char *p) -> const char * {
    while (p < fin && !isdigit((unsigned char)*p) && *p != '-') p++;
    return p;
  };

  if ((match = strstr(rx_buf_, "System Volt")) != nullptr) {
    match = skip_num(match + 11);
    sys_volt = atof(match) / 1000.0f;
    has_volt = true;
    last_sys_volt_ = sys_volt;
    if (system_voltage_sensor_) system_voltage_sensor_->publish_state(sys_volt);
  }
  if ((match = strstr(rx_buf_, "System Curr")) != nullptr) {
    match = skip_num(match + 11);
    sys_curr = atof(match) / 1000.0f;
    has_curr = true;
    if (system_current_sensor_)    system_current_sensor_->publish_state(sys_curr);
    if (charge_current_sensor_)    charge_current_sensor_->publish_state(sys_curr >= 0.0f ? sys_curr : 0.0f);
    if (discharge_current_sensor_) discharge_current_sensor_->publish_state(sys_curr < 0.0f ? -sys_curr : 0.0f);
  }
  if ((match = strstr(rx_buf_, "System SOC")) != nullptr) {
    match = skip_num(match + 10);
    last_soc_ = atof(match);
    if (system_soc_sensor_) system_soc_sensor_->publish_state(last_soc_);
  }
  if ((match = strstr(rx_buf_, "System SOH")) != nullptr) {
    match = skip_num(match + 10);
    if (system_soh_sensor_) system_soh_sensor_->publish_state(atof(match));
  }
  if ((match = strstr(rx_buf_, "System FCC")) != nullptr) {
    match = skip_num(match + 10);
    if (system_capacity_total_sensor_)
      system_capacity_total_sensor_->publish_state(atof(match) / 1000.0f);
  }
  if ((match = strstr(rx_buf_, "System RC")) != nullptr) {
    match = skip_num(match + 9);
    sys_cap = atof(match) / 1000.0f;
    has_cap = true;
    if (system_capacity_sensor_) system_capacity_sensor_->publish_state(sys_cap);
  }
  if ((match = strstr(rx_buf_, "Total Num")) != nullptr) {
    match = skip_num(match + 9);
    if (installed_batteries_sensor_) installed_batteries_sensor_->publish_state(atof(match));
  }
  if ((match = strstr(rx_buf_, "Present Num")) != nullptr) {
    match = skip_num(match + 11);
    if (present_batteries_sensor_) present_batteries_sensor_->publish_state(atof(match));
  }
  if (has_volt && has_curr) {
    float power = sys_volt * sys_curr;
    if (charge_power_sensor_)    charge_power_sensor_->publish_state(power >= 0.0f ? power : 0.0f);
    if (discharge_power_sensor_) discharge_power_sensor_->publish_state(power < 0.0f ? -power : 0.0f);
  }
  if (has_volt && has_cap) {
    if (available_energy_sensor_) available_energy_sensor_->publish_state(sys_volt * sys_cap);
  }

  // Mise à jour relais après chaque cycle rapide
  update_relay_();
}

// ============================================================
// parse_cells_() — réponse "getpwr N"
// ============================================================
void PylontechMonitorComponent::parse_cells_(int bat_idx) {
  if (bat_idx < 1 || bat_idx > (int)battery_count_) return;
  PylontechBattery &bat = batteries_[bat_idx - 1];

  char buf[1024];
  strncpy(buf, rx_buf_, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  int token_index = 0;
  char *token = strtok(buf, "#");
  while (token != nullptr) {
    while (*token == ' ' || *token == '\n' || *token == '\r') token++;
    if (strlen(token) > 0) {
      token_index++;
      if (token_index == 4) {
        if (bat.capacity_sensor_) bat.capacity_sensor_->publish_state(atof(token) / 1000.0f);
      }
      if (token_index >= 9 && (token_index - 9) % 4 == 0) {
        int cell_num = (token_index - 9) / 4;
        if (cell_num < MAX_CELLS) {
          float cell_v = atof(token);
          if (cell_v > 10.0f) cell_v /= 1000.0f;
          if (cell_v > 2.0f && cell_v < 4.5f && bat.cell_sensors[cell_num] != nullptr)
            bat.cell_sensors[cell_num]->publish_state(cell_v);
        }
      }
    }
    token = strtok(nullptr, "#");
  }
}

// ============================================================
// parse_stat_() — réponse "stat N"
// ============================================================
void PylontechMonitorComponent::parse_stat_(int bat_idx) {
  if (bat_idx < 1 || bat_idx > (int)battery_count_) return;
  PylontechBattery &bat = batteries_[bat_idx - 1];
  const char *fin = rx_buf_ + rx_len_;

  const char *match = strstr(rx_buf_, "CYCLE Times");
  if (match) {
    while (match < fin && *match != ':') match++;
    if (match < fin) {
      match++;
      while (match < fin && (*match == ' ' || *match == '\t')) match++;
      float cycles = atof(match);
      if (cycles >= 0.0f && cycles <= 25000.0f && bat.cycle_count_sensor_)
        bat.cycle_count_sensor_->publish_state(cycles);
    }
  }

  const char *match2 = strstr(rx_buf_, "Pwr Coulomb");
  if (!match2) match2 = strstr(rx_buf_, "Coulomb");
  if (match2) {
    while (match2 < fin && *match2 != ':') match2++;
    if (match2 < fin) {
      match2++;
      while (match2 < fin && (*match2 == ' ' || *match2 == '\t')) match2++;
      if (bat.coulomb_sensor_) bat.coulomb_sensor_->publish_state(atof(match2) / 1000.0f);
    }
  }
}

}  // namespace pylontech_monitor
}  // namespace esphome
