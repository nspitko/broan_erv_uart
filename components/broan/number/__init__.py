import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SPEED ,
    ENTITY_CATEGORY_CONFIG,
    ICON_FAN ,
    UNIT_PERCENT,
)

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

FanSpeedNumber = broan_ns.class_("FanSpeedNumber", number.Number)

CONF_FAN_SPEED = "fan_speed"

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
