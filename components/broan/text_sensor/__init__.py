import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_BROAN_ID, BroanComponent

DEPENDENCIES = ["broan"]

CONF_MODEL = "model"
CONF_FIRMWARE = "firmware"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_HARDWARE_REV = "hardware_revision"
CONF_WARNING_CODE = "warning_code"
CONF_FAULT_CODE = "fault_code"
CONF_ACTIVE_MODE = "active_mode"
CONF_BASE_MODE = "base_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
        cv.Optional(CONF_MODEL): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:tag",
        ),
        cv.Optional(CONF_FIRMWARE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:chip",
        ),
        cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:numeric",
        ),
        cv.Optional(CONF_HARDWARE_REV): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:numeric",
        ),
        cv.Optional(CONF_FAULT_CODE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:alert-circle",
        ),
        cv.Optional(CONF_WARNING_CODE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:alert",
        ),
        cv.Optional(CONF_ACTIVE_MODE): text_sensor.text_sensor_schema(
            icon="mdi:fan",
        ),
        cv.Optional(CONF_BASE_MODE): text_sensor.text_sensor_schema(
            icon="mdi:fan",
        ),
    }
)


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])

    if model_config := config.get(CONF_MODEL):
        s = await text_sensor.new_text_sensor(model_config)
        cg.add(broan_component.set_model_text_sensor(s))

    if firmware_config := config.get(CONF_FIRMWARE):
        s = await text_sensor.new_text_sensor(firmware_config)
        cg.add(broan_component.set_firmware_text_sensor(s))

    if fw_version_config := config.get(CONF_FIRMWARE_VERSION):
        s = await text_sensor.new_text_sensor(fw_version_config)
        cg.add(broan_component.set_firmware_version_text_sensor(s))

    if hw_rev_config := config.get(CONF_HARDWARE_REV):
        s = await text_sensor.new_text_sensor(hw_rev_config)
        cg.add(broan_component.set_hardware_revision_text_sensor(s))

    if fault_code_config := config.get(CONF_FAULT_CODE):
        s = await text_sensor.new_text_sensor(fault_code_config)
        cg.add(broan_component.set_fault_code_text_sensor(s))

    if warning_code_config := config.get(CONF_WARNING_CODE):
        s = await text_sensor.new_text_sensor(warning_code_config)
        cg.add(broan_component.set_warning_code_text_sensor(s))

    if active_mode_config := config.get(CONF_ACTIVE_MODE):
        s = await text_sensor.new_text_sensor(active_mode_config)
        cg.add(broan_component.set_active_mode_text_sensor(s))

    if base_mode_config := config.get(CONF_BASE_MODE):
        s = await text_sensor.new_text_sensor(base_mode_config)
        cg.add(broan_component.set_base_mode_text_sensor(s))
