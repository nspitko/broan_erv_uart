import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_GAUGE,
)

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

FanModeSelect = broan_ns.class_("FanModeSelect", select.Select)

CONF_FAN_MODE = 'fan_mode'

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),

    cv.Optional(CONF_FAN_MODE): select.select_schema(
        FanModeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_GAUGE,
    ),
}


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])
    if fan_mode_config := config.get(CONF_FAN_MODE):
        s = await select.new_select(
            fan_mode_config,
            options=[
                "off",
                "min",
                "max",
                "int",
                "manual",
                "turbo",
            ],
        )
        await cg.register_parented(s, config[CONF_BROAN_ID])
        cg.add(broan_component.set_fan_mode_select(s))