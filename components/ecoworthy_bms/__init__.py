import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

CONF_ECOWORTHY_BMS_ID = "ecoworthy_bms_id"

ecoworthy_bms_ns = cg.esphome_ns.namespace("ecoworthy_bms")

EcoworthyBms = ecoworthy_bms_ns.class_(
"EcoworthyBms", ble_client.BLEClientNode, cg.PollingComponent
)

CONF_VOLTAGE = "voltage"
CONF_SOC = "soc"
CONF_CURRENT = "current"
CONF_POWER = "power"
CONF_CAPACITY = "capacity"
CONF_CELL1_VOLTAGE = "cell1_voltage"
CONF_CELL2_VOLTAGE = "cell2_voltage"
CONF_CELL3_VOLTAGE = "cell3_voltage"
CONF_CELL4_VOLTAGE = "cell4_voltage"
CONF_CELL_MIN_VOLTAGE = "cell_min_voltage"
CONF_CELL_MAX_VOLTAGE = "cell_max_voltage"
CONF_CELL_VOLTAGE_DIFF = "cell_voltage_diff"
CONF_BATTERY_HEALTH = "battery_health"
CONF_PROBLEM_CODE = "problem_code"
CONF_TEMPERATURE1 = "temperature1"
CONF_TEMPERATURE2 = "temperature2"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EcoworthyBms),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=2,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_SOC): sensor.sensor_schema(
            unit_of_measurement="%",
            accuracy_decimals=0,
            device_class="battery",
            state_class="measurement",
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement="A",
            accuracy_decimals=1,
            device_class="current",
            state_class="measurement",
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement="W",
            accuracy_decimals=2,
            device_class="power",
            state_class="measurement",
        ),
        cv.Optional(CONF_CAPACITY): sensor.sensor_schema(
            unit_of_measurement="Ah",
            accuracy_decimals=2,
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL1_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL2_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL3_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL4_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL_MIN_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL_MAX_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_CELL_VOLTAGE_DIFF): sensor.sensor_schema(
            unit_of_measurement="V",
            accuracy_decimals=3,
            device_class="voltage",
            state_class="measurement",
        ),
        cv.Optional(CONF_BATTERY_HEALTH): sensor.sensor_schema(
            unit_of_measurement="%",
            accuracy_decimals=0,
            state_class="measurement",
        ),
        cv.Optional(CONF_PROBLEM_CODE): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_TEMPERATURE1): sensor.sensor_schema(
            unit_of_measurement="°C",
            accuracy_decimals=1,
            device_class="temperature",
            state_class="measurement",
        ),
        cv.Optional(CONF_TEMPERATURE2): sensor.sensor_schema(
            unit_of_measurement="°C",
            accuracy_decimals=1,
            device_class="temperature",
            state_class="measurement",
        ),
    })

    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("2s")),
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    sensor_map = {
        CONF_VOLTAGE: "set_voltage_sensor",
        CONF_SOC: "set_soc_sensor",
        CONF_CURRENT: "set_current_sensor",
        CONF_POWER: "set_power_sensor",
        CONF_CAPACITY: "set_capacity_sensor",
        CONF_CELL1_VOLTAGE: "set_cell1_voltage_sensor",
        CONF_CELL2_VOLTAGE: "set_cell2_voltage_sensor",
        CONF_CELL3_VOLTAGE: "set_cell3_voltage_sensor",
        CONF_CELL4_VOLTAGE: "set_cell4_voltage_sensor",
        CONF_CELL_MIN_VOLTAGE: "set_cell_min_voltage_sensor",
        CONF_CELL_MAX_VOLTAGE: "set_cell_max_voltage_sensor",
        CONF_CELL_VOLTAGE_DIFF: "set_cell_voltage_diff_sensor",
        CONF_BATTERY_HEALTH: "set_battery_health_sensor",
        CONF_PROBLEM_CODE: "set_problem_code_sensor",
        CONF_TEMPERATURE1: "set_temperature1_sensor",
        CONF_TEMPERATURE2: "set_temperature2_sensor",
    }

    for key, method in sensor_map.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, method)(sens))