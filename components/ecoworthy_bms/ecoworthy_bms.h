#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include <esp_gattc_api.h>
#include <vector>
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace ecoworthy_bms {

class EcoworthyBms : public ble_client::BLEClientNode, public PollingComponent {
 public:
 sensor::Sensor *voltage_sensor_{nullptr};
 sensor::Sensor *soc_sensor_{nullptr};
 sensor::Sensor *current_sensor_{nullptr};
 sensor::Sensor *power_sensor_{nullptr};
 sensor::Sensor *capacity_sensor_{nullptr};

 sensor::Sensor *cell1_voltage_sensor_{nullptr};
 sensor::Sensor *cell2_voltage_sensor_{nullptr};
 sensor::Sensor *cell3_voltage_sensor_{nullptr};
 sensor::Sensor *cell4_voltage_sensor_{nullptr};
 sensor::Sensor *cell_min_voltage_sensor_{nullptr};
 sensor::Sensor *cell_max_voltage_sensor_{nullptr};
 sensor::Sensor *cell_voltage_diff_sensor_{nullptr};

 sensor::Sensor *battery_health_sensor_{nullptr};
 sensor::Sensor *problem_code_sensor_{nullptr};
 sensor::Sensor *temperature1_sensor_{nullptr};
 sensor::Sensor *temperature2_sensor_{nullptr};
 text_sensor::TextSensor *last_successful_update_sensor_{nullptr};
 time::RealTimeClock *time_{nullptr};

 bool show_log_values_{false};
 bool show_log_raw_data_{false};

 void set_voltage_sensor(sensor::Sensor *sensor) { voltage_sensor_ = sensor; }
 void set_soc_sensor(sensor::Sensor *sensor) { soc_sensor_ = sensor; }
 void set_current_sensor(sensor::Sensor *sensor) { current_sensor_ = sensor; }
 void set_power_sensor(sensor::Sensor *sensor) { power_sensor_ = sensor; }
 void set_capacity_sensor(sensor::Sensor *sensor) { capacity_sensor_ = sensor; }

 void set_cell1_voltage_sensor(sensor::Sensor *sensor) { cell1_voltage_sensor_ = sensor; }
 void set_cell2_voltage_sensor(sensor::Sensor *sensor) { cell2_voltage_sensor_ = sensor; }
 void set_cell3_voltage_sensor(sensor::Sensor *sensor) { cell3_voltage_sensor_ = sensor; }
 void set_cell4_voltage_sensor(sensor::Sensor *sensor) { cell4_voltage_sensor_ = sensor; }
 void set_cell_min_voltage_sensor(sensor::Sensor *sensor) { cell_min_voltage_sensor_ = sensor; }
 void set_cell_max_voltage_sensor(sensor::Sensor *sensor) { cell_max_voltage_sensor_ = sensor; }
 void set_cell_voltage_diff_sensor(sensor::Sensor *sensor) { cell_voltage_diff_sensor_ = sensor; }

 void set_battery_health_sensor(sensor::Sensor *sensor) { battery_health_sensor_ = sensor; }
 void set_problem_code_sensor(sensor::Sensor *sensor) { problem_code_sensor_ = sensor; }
 void set_temperature1_sensor(sensor::Sensor *sensor) { temperature1_sensor_ = sensor; }
 void set_temperature2_sensor(sensor::Sensor *sensor) { temperature2_sensor_ = sensor; }
 void set_last_successful_update_sensor(text_sensor::TextSensor *sensor) {
   last_successful_update_sensor_ = sensor;
 }
 void set_time(time::RealTimeClock *time) { time_ = time; }

 enum class RequestPhase {
    IDLE,
    WAIT_CMD_1,
    WAIT_CMD_2,
    WAIT_DATA
  };

  RequestPhase request_phase_{RequestPhase::IDLE};

  void gattc_event_handler(esp_gattc_cb_event_t event,
                           esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void dump_config() override;
  void update() override;
  void loop() override;

  float get_setup_priority() const override {
    return setup_priority::DATA;
  }

 protected:
  bool initialized_{false};
  std::vector<uint8_t> frame_a1_;
  std::vector<uint8_t> frame_a2_;

  bool received_a1_{false};
  bool received_a2_{false};

  uint16_t char_notify_handle_{0};
  uint16_t char_write_handle_{0};

  uint8_t retry_count_{0};
  uint32_t response_deadline_{0};

  void send_init_command_();
  void process_a1_(const std::vector<uint8_t> &data);
  void process_a2_(const std::vector<uint8_t> &data);
};

}  // namespace ecoworthy_bms
}  // namespace esphome

