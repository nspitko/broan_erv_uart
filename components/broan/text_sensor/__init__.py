import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_BROAN_ID, BroanComponent

DEPENDENCIES = ["broan"]

CONF_MODEL = "model"
CONF_FIRMWARE = "firmware"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_HARDWARE_REV = "hardware_rev"
CONF_FAULT_STATUS = "fault_status"
CONF_WARNING_STATUS = "warning_status"
CONF_ACTIVE_MODE = "active_mode"

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
        # Human-readable decode of the fault register (17 00): "OK" or the
        # service-manual E/W code + description.
        cv.Optional(CONF_FAULT_STATUS): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:alert-circle",
        ),
        # Human-readable decode of the warning register (1A 00): "OK" or the
        # service-manual W code + description.
        cv.Optional(CONF_WARNING_STATUS): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:alert",
        ),
        # Human-readable decode of the executing-airflow register (07 20):
        # "Idle" / "Max" / "Turbo" / "Intermittent" / "Running (code N)".
        cv.Optional(CONF_ACTIVE_MODE): text_sensor.text_sensor_schema(
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
        cg.add(broan_component.set_hardware_rev_text_sensor(s))

    if fault_status_config := config.get(CONF_FAULT_STATUS):
        s = await text_sensor.new_text_sensor(fault_status_config)
        cg.add(broan_component.set_fault_status_text_sensor(s))

    if warning_status_config := config.get(CONF_WARNING_STATUS):
        s = await text_sensor.new_text_sensor(warning_status_config)
        cg.add(broan_component.set_warning_status_text_sensor(s))

    if active_mode_config := config.get(CONF_ACTIVE_MODE):
        s = await text_sensor.new_text_sensor(active_mode_config)
        cg.add(broan_component.set_active_mode_text_sensor(s))
