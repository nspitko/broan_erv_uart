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

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

FanSpeedNumber = broan_ns.class_("FanSpeedNumber", number.Number)
HumiditySetpointNumber = broan_ns.class_("HumiditySetpointNumber", number.Number)
IntermittentPeriodNumber = broan_ns.class_("IntermittentPeriodNumber", number.Number)
FilterIntervalNumber = broan_ns.class_("FilterIntervalNumber", number.Number)
OverrideDurationNumber = broan_ns.class_("OverrideDurationNumber", number.Number)
CFMNumber = broan_ns.class_("CFMNumber", number.Number)
BroanField = broan_ns.enum("BroanField")

CONF_FAN_SPEED = "fan_speed"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
CONF_INT_PERIOD = "intermittent_period"
CONF_FILTER_INTERVAL = "filter_interval"
CONF_OVERRIDE_DURATION = "override_duration"

# Per-speed supply/exhaust CFM setpoints -> BroanField enum member.
# Setting supply != exhaust gives unbalanced ventilation for that speed.
CFM_SETPOINTS = {
    "cfm_min_supply": "CFMIn_Min",
    "cfm_min_exhaust": "CFMOut_Min",
    "cfm_med_supply": "CFMIn_Medium",
    "cfm_med_exhaust": "CFMOut_Medium",
    "cfm_max_supply": "CFMIn_Max",
    "cfm_max_exhaust": "CFMOut_Max",
}

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
            unit_of_measurement="min",
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_FILTER_INTERVAL): number.number_schema(
            FilterIntervalNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement="d",
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_OVERRIDE_DURATION): number.number_schema(
            OverrideDurationNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement="min",
            icon=ICON_TIMER,
        ),
        **{
            cv.Optional(key): number.number_schema(
                CFMNumber,
                device_class=DEVICE_CLASS_SPEED,
                entity_category=ENTITY_CATEGORY_CONFIG,
                unit_of_measurement=UNIT_CFM,
                icon=ICON_FAN,
            )
            for key in CFM_SETPOINTS
        },
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
        # minutes on-time per hour (0-60); converted to seconds in control()
        h = await number.new_number(
            intermittent_period_config, min_value=0, max_value=60, step=1
        )
        await cg.register_parented(h, config[CONF_BROAN_ID])
        cg.add(broan_component.set_intermittent_period_number(h))

    if filter_interval_config := config.get(CONF_FILTER_INTERVAL):
        f = await number.new_number(
            filter_interval_config, min_value=30, max_value=365, step=1
        )
        await cg.register_parented(f, config[CONF_BROAN_ID])
        cg.add(broan_component.set_filter_interval_number(f))

    if override_duration_config := config.get(CONF_OVERRIDE_DURATION):
        o = await number.new_number(
            override_duration_config, min_value=0, max_value=240, step=1
        )
        await cg.register_parented(o, config[CONF_BROAN_ID])
        cg.add(broan_component.set_override_duration_number(o))

    for key, field_name in CFM_SETPOINTS.items():
        if cfm_config := config.get(key):
            c = await number.new_number(
                cfm_config, min_value=0, max_value=200, step=1
            )
            await cg.register_parented(c, config[CONF_BROAN_ID])
            field = getattr(BroanField, field_name)
            cg.add(c.set_field(field))
            cg.add(broan_component.register_cfm_number(field, c))
