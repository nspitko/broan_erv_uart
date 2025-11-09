import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
    ICON_WATER,
)

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

HumidityControlSwitch = broan_ns.class_("HumidityControlSwitch", switch.Switch)

CONF_HUMIDITY_CONTROL = "humidity_control"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
    cv.Optional(CONF_HUMIDITY_CONTROL): switch.switch_schema(
        HumidityControlSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_WATER,
    ),
}


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])
    if humidity_control_config := config.get(CONF_HUMIDITY_CONTROL):
        s = await switch.new_switch(humidity_control_config)
        await cg.register_parented(s, config[CONF_BROAN_ID])
        cg.add(broan_component.set_humidity_control_switch(s))