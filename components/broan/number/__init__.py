import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_HUMIDITY,
    ENTITY_CATEGORY_CONFIG,
    ICON_FAN,
    ICON_WATER,
    ICON_TIMER,
    UNIT_PERCENT,
)

UNIT_CFM = "CFM"
UNIT_PERIOD = "Period"

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

FanSpeedNumber = broan_ns.class_("FanSpeedNumber", number.Number)
HumiditySetpointNumber = broan_ns.class_("HumiditySetpointNumber", number.Number)
IntermittentPeriodNumber = broan_ns.class_("IntermittentPeriodNumber", number.Number)

CONF_FAN_SPEED = "fan_speed"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
CONF_INT_PERIOD = "intermittent_period"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
        cv.Optional(CONF_FAN_SPEED): number.number_schema(
            FanSpeedNumber,
            device_class=DEVICE_CLASS_SPEED,
            entity_category=ENTITY_CATEGORY_CONFIG,
			unit_of_measurement=UNIT_PERCENT,
            icon=ICON_FAN,
        ),
        cv.Optional(CONF_HUMIDITY_SETPOINT): number.number_schema(
            HumiditySetpointNumber,
            device_class=DEVICE_CLASS_HUMIDITY,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_INT_PERIOD): number.number_schema(
            IntermittentPeriodNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERIOD,
            icon=ICON_TIMER,
        )
    }
)


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])

    if fan_speed_config := config.get(CONF_FAN_SPEED):
        n = await number.new_number(
            fan_speed_config, min_value=0, max_value=100, step=10
        )
        await cg.register_parented(n, config[CONF_BROAN_ID])
        cg.add(broan_component.set_fan_speed_number(n))

    if humidity_setpoint_config := config.get(CONF_HUMIDITY_SETPOINT):
        h = await number.new_number(
            humidity_setpoint_config, min_value=30, max_value=55, step=5
        )
        await cg.register_parented(h, config[CONF_BROAN_ID])
        cg.add(broan_component.set_humidity_setpoint_number(h))

    if intermittent_period_config := config.get(CONF_INT_PERIOD):
        h = await number.new_number(
            intermittent_period_config, min_value=10, max_value=50, step=5
        )
        await cg.register_parented(h, config[CONF_BROAN_ID])
        cg.add(broan_component.set_intermittent_period_number(h))
