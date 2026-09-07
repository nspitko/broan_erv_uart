import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_POWER,
    CONF_TEMPERATURE,
    DEVICE_CLASS_POWER ,
    DEVICE_CLASS_HUMIDITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_TEMPERATURE,
    ICON_POWER,
    ICON_THERMOMETER,
    ICON_AIR_FILTER,
    ICON_FAN,
    UNIT_WATT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

CONF_FILTER_LIFE = "filter_life"
CONF_TEMPERATURE_OUT = "temperature_out"
CONF_HUMIDITY_IN = "humidity_in"
CONF_HUMIDITY_OUT = "humidity_out"
CONF_SUPPLY_CFM = "supply_fan_cfm"
CONF_EXHAUST_CFM = "exhaust_fan_cfm"
CONF_SUPPLY_RPM = "supply_fan_rpm"
CONF_EXHAUST_RPM = "exhaust_fan_rpm"
CONF_FAULT_CODE = "fault_code"
CONF_WARNING_CODE = "warning_code"
CONF_BASE_MODE_CODE = "base_mode_code"
CONF_ACTIVE_MODE_CODE = "active_mode_code"

UNIT_CFM = "CFM"
UNIT_RPM = "RPM"
UNIT_DAY = "d"

from . import CONF_BROAN_ID, BroanComponent

DEPENDENCIES = ["broan"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            device_class=DEVICE_CLASS_POWER,
            icon=ICON_POWER,
            unit_of_measurement=UNIT_WATT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_TEMPERATURE,
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
        ),
        cv.Optional(CONF_TEMPERATURE_OUT): sensor.sensor_schema(
            device_class=DEVICE_CLASS_TEMPERATURE,
            icon=ICON_THERMOMETER,
            unit_of_measurement=UNIT_CELSIUS,
        ),
        cv.Optional(CONF_HUMIDITY_IN): sensor.sensor_schema(
            device_class=DEVICE_CLASS_HUMIDITY,
            unit_of_measurement=UNIT_PERCENT,
        ),
        cv.Optional(CONF_HUMIDITY_OUT): sensor.sensor_schema(
            device_class=DEVICE_CLASS_HUMIDITY,
            unit_of_measurement=UNIT_PERCENT,
        ),
        cv.Optional(CONF_FILTER_LIFE): sensor.sensor_schema(
            icon=ICON_AIR_FILTER,
            unit_of_measurement=UNIT_DAY,
        ),
        cv.Optional(CONF_SUPPLY_CFM): sensor.sensor_schema(
            icon=ICON_FAN,
            unit_of_measurement=UNIT_CFM,
        ),
        cv.Optional(CONF_EXHAUST_CFM): sensor.sensor_schema(
            icon=ICON_FAN,
            unit_of_measurement=UNIT_CFM,
        ),
        cv.Optional(CONF_SUPPLY_RPM): sensor.sensor_schema(
            icon=ICON_FAN,
            unit_of_measurement=UNIT_RPM,
        ),
        cv.Optional(CONF_EXHAUST_RPM): sensor.sensor_schema(
            icon=ICON_FAN,
            unit_of_measurement=UNIT_RPM,
        ),

        # Info / Diagnostic
        cv.Optional(CONF_FAULT_CODE): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        cv.Optional(CONF_WARNING_CODE): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        cv.Optional(CONF_BASE_MODE_CODE): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        cv.Optional(CONF_ACTIVE_MODE_CODE): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])
    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(broan_component.set_power_sensor(sens))

    if temperature_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temperature_config)
        cg.add(broan_component.set_temperature_sensor(sens))

    if temperature_out_config := config.get(CONF_TEMPERATURE_OUT):
        sens = await sensor.new_sensor(temperature_out_config)
        cg.add(broan_component.set_temperature_out_sensor(sens))

    if humidity_in_config := config.get(CONF_HUMIDITY_IN):
        sens = await sensor.new_sensor(humidity_in_config)
        cg.add(broan_component.set_humidity_in_sensor(sens))

    if humidity_out_config := config.get(CONF_HUMIDITY_OUT):
        sens = await sensor.new_sensor(humidity_out_config)
        cg.add(broan_component.set_humidity_out_sensor(sens))

    if filter_life_config := config.get(CONF_FILTER_LIFE):
        sens = await sensor.new_sensor(filter_life_config)
        cg.add(broan_component.set_filter_life_sensor(sens))

    if supply_cfm_config := config.get(CONF_SUPPLY_CFM):
        sens = await sensor.new_sensor(supply_cfm_config)
        cg.add(broan_component.set_supply_cfm_sensor(sens))

    if exhaust_cfm_config := config.get(CONF_EXHAUST_CFM):
        sens = await sensor.new_sensor(exhaust_cfm_config)
        cg.add(broan_component.set_exhaust_cfm_sensor(sens))

    if supply_rpm_config := config.get(CONF_SUPPLY_RPM):
        sens = await sensor.new_sensor(supply_rpm_config)
        cg.add(broan_component.set_supply_rpm_sensor(sens))

    if exhaust_rpm_config := config.get(CONF_EXHAUST_RPM):
        sens = await sensor.new_sensor(exhaust_rpm_config)
        cg.add(broan_component.set_exhaust_rpm_sensor(sens))

    if fault_code_config := config.get(CONF_FAULT_CODE):
        sens = await sensor.new_sensor(fault_code_config)
        cg.add(broan_component.set_fault_code_sensor(sens))

    if warning_code_config := config.get(CONF_WARNING_CODE):
        sens = await sensor.new_sensor(warning_code_config)
        cg.add(broan_component.set_warning_code_sensor(sens))

    if base_mode_code_config := config.get(CONF_BASE_MODE_CODE):
        sens = await sensor.new_sensor(base_mode_code_config)
        cg.add(broan_component.set_base_mode_code_sensor(sens))

    if active_mode_code_config := config.get(CONF_ACTIVE_MODE_CODE):
        sens = await sensor.new_sensor(active_mode_code_config)
        cg.add(broan_component.set_active_mode_code_sensor(sens))
