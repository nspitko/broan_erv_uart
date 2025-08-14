import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_POWER,
    DEVICE_CLASS_POWER ,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_POWER ,
    UNIT_WATT,
)

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
    }
)

async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])
    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(broan_component.set_power_sensor(sens))
