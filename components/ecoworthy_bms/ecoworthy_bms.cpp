#include "ecoworthy_bms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

#ifdef USE_ESP32

namespace esphome {
namespace ecoworthy_bms {

static const char *const TAG = "ecoworthy_bms";

static constexpr uint16_t SERVICE_UUID = 0xFFF0;
static constexpr uint16_t NOTIFY_UUID = 0xFFF1;
static constexpr uint16_t WRITE_UUID = 0xFFF2;

static const uint8_t INIT_CMD_1[] = {
    0xFF, 0x08, 0x02, 0x00, 0x0B, 0x01, 0x00, 0x64, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x2D
};

static const uint8_t INIT_CMD_2[] = {
    0xFF, 0x08, 0x02, 0x00, 0x0B, 0x01, 0x00, 0x14, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x65, 0xEF
};

void EcoworthyBms::gattc_event_handler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t *param) {

  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT:
      this->node_state = ble_client::espbt::ClientState::IDLE;
      this->char_notify_handle_ = 0;
      this->char_write_handle_ = 0;
      this->request_phase_ = RequestPhase::IDLE;
      this->initialized_ = false;  // NEU: Bei neuer Verbindung erneut initialisieren
      this->retry_count_ = 0;
      this->received_a1_ = false;
      this->received_a2_ = false;
      this->frame_a1_.clear();
      this->frame_a2_.clear();
      ESP_LOGW(TAG, "BLE-Verbindung getrennt");
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *notify_char = this->parent_->get_characteristic(
          ble_client::espbt::ESPBTUUID::from_uint16(SERVICE_UUID),
          ble_client::espbt::ESPBTUUID::from_uint16(NOTIFY_UUID));

      if (notify_char == nullptr) {
        ESP_LOGE(TAG, "Notify-Characteristic FFF1 nicht gefunden");
        break;
      }

      this->char_notify_handle_ = notify_char->handle;

      auto status = esp_ble_gattc_register_for_notify(
          this->parent()->get_gattc_if(),
          this->parent()->get_remote_bda(),
          notify_char->handle);

      if (status != ESP_OK) {
        ESP_LOGE(TAG, "Registrierung der Notifications fehlgeschlagen: %d", status);
      }

      auto *write_char = this->parent_->get_characteristic(
          ble_client::espbt::ESPBTUUID::from_uint16(SERVICE_UUID),
          ble_client::espbt::ESPBTUUID::from_uint16(WRITE_UUID));

      if (write_char == nullptr) {
        ESP_LOGE(TAG, "Write-Characteristic FFF2 nicht gefunden");
        break;
      }

      this->char_write_handle_ = write_char->handle;
      ESP_LOGI(TAG, "ECO-WORTHY BLE-Characteristics gefunden");
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Notify-Registrierung fehlgeschlagen: %d",
                 param->reg_for_notify.status);
        break;
      }

      this->node_state = ble_client::espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "BLE-Notifications aktiviert");
      this->update();
      break;

    case ESP_GATTC_NOTIFY_EVT:{
      if (param->notify.handle != this->char_notify_handle_) {
        break;
      }

      if (param->notify.value_len < 9) {
       ESP_LOGW(TAG, "Paket zu kurz: %u Bytes",
                param->notify.value_len);
       break;
     }

     const uint8_t *data = param->notify.value;
     const size_t length = param->notify.value_len;
     uint8_t packet_type = 0;

     // Pakettyp direkt am Anfang
     if (data[0] == 0xA1 || data[0] == 0xA2) {
       packet_type = data[0];

     // MAC-Adresse an Position 0, Pakettyp an Position 6
     } else if (std::memcmp(
                    data, this->parent_->get_remote_bda(), 6) == 0 &&
                (data[6] == 0xA1 || data[6] == 0xA2)) {
       packet_type = data[6];
     }

     if (packet_type == 0) {
       ESP_LOGV(TAG, "Unbekanntes Paketformat oder fremde MAC-Adresse");
       break;
     }

      {
        // Modbus CRC16, Initialwert 0xFFFF.
        uint16_t crc = 0xFFFF;

        for (size_t i = 0; i < length - 2; i++) {
          crc ^= data[i];

          for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1) {
              crc = (crc >> 1) ^ 0xA001;
            } else {
              crc >>= 1;
            }
          }
        }

        const uint16_t received_crc =
            static_cast<uint16_t>(data[length - 2]) |
            (static_cast<uint16_t>(data[length - 1]) << 8);

        if (crc != received_crc) {
          ESP_LOGW(TAG,
                   "CRC ungültig: berechnet=0x%04X empfangen=0x%04X",
                   crc, received_crc);
          break;
        }

        std::vector<uint8_t> packet(data, data + length);

        if (packet_type == 0xA1) {
          this->process_a1_(packet);
        } else {
          this->process_a2_(packet);
        }

        if (this->request_phase_ == RequestPhase::WAIT_CMD_1) {
          if (packet_type == 0xA1) {
            this->retry_count_ = 0;
            this->request_phase_ = RequestPhase::WAIT_CMD_2;
            this->send_init_command_();
          }

        } else if (this->request_phase_ == RequestPhase::WAIT_CMD_2) {
          if (packet_type == 0xA2) {
            this->retry_count_ = 0;
            this->received_a1_ = false;
            this->received_a2_ = false;
            this->frame_a1_.clear();
            this->frame_a2_.clear();
            this->request_phase_ = RequestPhase::WAIT_DATA;
            this->response_deadline_ = millis() + 5000;
          }

        } else if (this->request_phase_ == RequestPhase::WAIT_DATA) {
          if (packet_type == 0xA1) {
            this->frame_a1_ = packet;
            this->received_a1_ = true;
          } else if (packet_type == 0xA2) {
            this->frame_a2_ = packet;
            this->received_a2_ = true;
          }

          if (this->received_a1_ && this->received_a2_) {
            ESP_LOGI(TAG, "Vollständiger A1/A2-Datensatz empfangen");
            this->initialized_ = true;
            this->request_phase_ = RequestPhase::IDLE;
          }
        }
      }

      break;
    }

    default:
      break;
  }
}

void EcoworthyBms::update() {
  if (this->node_state != ble_client::espbt::ClientState::ESTABLISHED) {
    ESP_LOGV(TAG, "BLE-Verbindung noch nicht bereit");
    return;
  }

  if (this->char_write_handle_ == 0) {
    ESP_LOGW(TAG, "Write-Characteristic noch nicht verfügbar");
    return;
  }

  if (this->initialized_ || this->request_phase_ != RequestPhase::IDLE) {
    return;
  }

  this->retry_count_ = 0;
  this->received_a1_ = false;
  this->received_a2_ = false;
  this->frame_a1_.clear();
  this->frame_a2_.clear();

  this->request_phase_ = RequestPhase::WAIT_CMD_1;
  this->send_init_command_();
}

void EcoworthyBms::send_init_command_() {
  const uint8_t *data;
  size_t length;

  if (this->request_phase_ == RequestPhase::WAIT_CMD_1) {
    data = INIT_CMD_1;
    length = sizeof(INIT_CMD_1);
  } else if (this->request_phase_ == RequestPhase::WAIT_CMD_2) {
    data = INIT_CMD_2;
    length = sizeof(INIT_CMD_2);
  } else {
    return;
  }

  auto status = esp_ble_gattc_write_char(
      this->parent_->get_gattc_if(),
      this->parent_->get_conn_id(),
      this->char_write_handle_,
      length,
      const_cast<uint8_t *>(data),
      ESP_GATT_WRITE_TYPE_NO_RSP,
      ESP_GATT_AUTH_REQ_NONE);

  if (status != ESP_OK) {
    ESP_LOGW(TAG, "BLE-Kommando fehlgeschlagen: %d", status);
    this->request_phase_ = RequestPhase::IDLE;
    this->retry_count_ = 0;
    return;
  }

  this->response_deadline_ = millis() + 1000;

  ESP_LOGI(TAG, "Initialisierungsbefehl %u gesendet",
           this->request_phase_ == RequestPhase::WAIT_CMD_1 ? 1U : 2U);
}

void EcoworthyBms::loop() {
  if (this->request_phase_ == RequestPhase::IDLE) {
    return;
  }

  if (static_cast<int32_t>(millis() - this->response_deadline_) < 0) {
    return;
  }

  if (this->request_phase_ == RequestPhase::WAIT_DATA) {
    ESP_LOGW(TAG, "Timeout: A1/A2-Datensatz unvollständig");
    this->request_phase_ = RequestPhase::IDLE;
    this->received_a1_ = false;
    this->received_a2_ = false;
    this->frame_a1_.clear();
    this->frame_a2_.clear();
    return;
  }

  if (this->retry_count_ < 2) {
    this->retry_count_++;
    ESP_LOGW(TAG, "Timeout bei Initialisierungsbefehl, Versuch %u/3",
             static_cast<unsigned>(this->retry_count_ + 1));
    this->send_init_command_();
    return;
  }

  ESP_LOGE(TAG, "Keine Antwort auf Initialisierungsbefehl");
  this->request_phase_ = RequestPhase::IDLE;
  this->retry_count_ = 0;
}

void EcoworthyBms::process_a1_(const std::vector<uint8_t> &data) {
  if (data.size() < 51) {
    ESP_LOGW(TAG, "A1-Paket zu kurz: %u Bytes",
             static_cast<unsigned>(data.size()));
    return;
  }

  if (this->show_log_raw_data_) {
    ESP_LOGI(TAG, "A1-Rohdaten (%u Bytes):",
             static_cast<unsigned>(data.size()));

    for (size_t i = 0; i < data.size(); i += 16) {
      char hex[16 * 3 + 1] = {};
      size_t pos = 0;

    for (size_t j = i; j < data.size() && j < i + 16; j++) {
      pos += snprintf(hex + pos, sizeof(hex) - pos,
                      "%02X ", data[j]);
    }

    ESP_LOGI(TAG, "A1 [%03u]: %s",
             static_cast<unsigned>(i), hex);
    }
  }

  const float voltage =
      static_cast<float>((data[18] << 8) | data[19]) / 100.0f;

  const uint8_t soc = data[15];

  const int16_t current_raw =
      static_cast<int16_t>((data[20] << 8) | data[21]);

  const float current = static_cast<float>(current_raw) / 10.0f;
  const float power = voltage * current;

  const float capacity =
      static_cast<float>((data[24] << 8) | data[25]) / 100.0f;

  const uint16_t battery_health =
      (static_cast<uint16_t>(data[16]) << 8) | data[17];

  const uint16_t problem_code =
      (static_cast<uint16_t>(data[49]) << 8) | data[50];

  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(voltage);
  }

  if (this->soc_sensor_ != nullptr) {
    this->soc_sensor_->publish_state(soc);
  }

  if (this->current_sensor_ != nullptr) {
    this->current_sensor_->publish_state(current);
  }

  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(power);
  }

  if (this->capacity_sensor_ != nullptr) {
    this->capacity_sensor_->publish_state(capacity);
  }

  if (this->battery_health_sensor_ != nullptr) {
    this->battery_health_sensor_->publish_state(battery_health);
  }

  if (this->problem_code_sensor_ != nullptr) {
    this->problem_code_sensor_->publish_state(problem_code);
  }

  if (this->show_log_values_) {
    ESP_LOGI(TAG,
             "A1: Spannung=%.2f V, SoC=%u %%, Strom=%.1f A, "
             "Leistung=%.2f W, Kapazitaet=%.2f Ah",
             voltage, soc, current, power, capacity);
  }

  if (this->last_successful_update_sensor_ != nullptr &&
      this->time_ != nullptr) {
    auto now = this->time_->now();
    if (now.is_valid()) {
      this->last_successful_update_sensor_->publish_state(
          now.strftime("%d.%m.%Y %H:%M:%S"));
    } else {
      ESP_LOGW(TAG, "Zeit noch nicht synchronisiert; Zeitstempel nicht aktualisiert");
    }
  }
}

void EcoworthyBms::process_a2_(const std::vector<uint8_t> &data) {
  if (data.size() < 22) {
    ESP_LOGW(TAG, "A2-Paket zu kurz: %u Bytes",
             static_cast<unsigned>(data.size()));
    return;
  }

  if (this->show_log_raw_data_) {
    ESP_LOGI(TAG, "A2-Rohdaten (%u Bytes):",
             static_cast<unsigned>(data.size()));

    for (size_t i = 0; i < data.size(); i += 16) {
      char hex[16 * 3 + 1] = {};
      size_t pos = 0;

      for (size_t j = i; j < data.size() && j < i + 16; j++) {
        pos += snprintf(hex + pos, sizeof(hex) - pos,
                        "%02X ", data[j]);
      }

      ESP_LOGI(TAG, "A2 [%03u]: %s",
               static_cast<unsigned>(i), hex);
    }
  }

  if (data.size() < 80) {
    ESP_LOGW(TAG, "A2-Paket zu kurz fuer Temperaturanzahl");
    return;
  }

  const uint16_t temperature_count =
      (static_cast<uint16_t>(data[78]) << 8) | data[79];

  if (temperature_count > 0 && data.size() < 80 + temperature_count * 2) {
    ESP_LOGW(TAG, "A2-Paket zu kurz fuer Temperaturwerte");
    return;
  }

  if (temperature_count > 0) {
    const uint16_t temperature1_raw =
        (static_cast<uint16_t>(data[80]) << 8) | data[81];

    if (this->temperature1_sensor_ != nullptr) {
      this->temperature1_sensor_->publish_state(
          static_cast<float>(temperature1_raw) / 10.0f);
    }
  }

  if (temperature_count > 1) {
    const uint16_t temperature2_raw =
        (static_cast<uint16_t>(data[82]) << 8) | data[83];

    if (this->temperature2_sensor_ != nullptr) {
      this->temperature2_sensor_->publish_state(
          static_cast<float>(temperature2_raw) / 10.0f);
    }
  }

  const uint8_t cell_count = data[13];
  float cells[4];
  float min_voltage = 100.0f;
  float max_voltage = 0.0f;

  for (size_t i = 0; i < 4; i++) {
    const size_t offset = 14 + i * 2;
    const uint16_t mv =
        (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];

    if (mv < 2500 || mv > 4500) {
      ESP_LOGW(TAG, "Ungueltige Zellspannung an Position %u: %u mV",
               static_cast<unsigned>(i + 1), mv);
      return;
    }

    cells[i] = static_cast<float>(mv) / 1000.0f;

    if (cells[i] < min_voltage) {
      min_voltage = cells[i];
    }

    if (cells[i] > max_voltage) {
      max_voltage = cells[i];
    }
  }

  if (this->cell1_voltage_sensor_ != nullptr) {
    this->cell1_voltage_sensor_->publish_state(cells[0]);
  }

  if (this->cell2_voltage_sensor_ != nullptr) {
    this->cell2_voltage_sensor_->publish_state(cells[1]);
  }

  if (this->cell3_voltage_sensor_ != nullptr) {
    this->cell3_voltage_sensor_->publish_state(cells[2]);
  }

  if (this->cell4_voltage_sensor_ != nullptr) {
    this->cell4_voltage_sensor_->publish_state(cells[3]);
  }

  if (this->cell_min_voltage_sensor_ != nullptr) {
    this->cell_min_voltage_sensor_->publish_state(min_voltage);
  }

  if (this->cell_max_voltage_sensor_ != nullptr) {
    this->cell_max_voltage_sensor_->publish_state(max_voltage);
  }

  if (this->cell_voltage_diff_sensor_ != nullptr) {
    this->cell_voltage_diff_sensor_->publish_state(max_voltage - min_voltage);
  }

  if (this->show_log_values_) {
  ESP_LOGI(TAG,
           "A2: Zellanzahl=%u, Zellen=%.3f/%.3f/%.3f/%.3f V, "
           "Min=%.3f V, Max=%.3f V, Differenz=%.3f V",
           cell_count,
           cells[0], cells[1], cells[2], cells[3],
           min_voltage, max_voltage, max_voltage - min_voltage);
  }
}

void EcoworthyBms::dump_config() {
  ESP_LOGCONFIG(TAG, "ECO-WORTHY BMS:");
  ESP_LOGCONFIG(TAG, "  Service UUID: 0x%04X", SERVICE_UUID);
  ESP_LOGCONFIG(TAG, "  Notify UUID: 0x%04X", NOTIFY_UUID);
  ESP_LOGCONFIG(TAG, "  Write UUID: 0x%04X", WRITE_UUID);
}

}  // namespace ecoworthy_bms
}  // namespace esphome

#endif  // USE_ESP32
