import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_RUNNING

from . import CONF_BROAN_ID, BroanComponent

DEPENDENCIES = ["broan"]

CONF_FANS_RUNNING = "fans_running"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
        # Fans actually moving air right now, from the executing-airflow
        # register (07 20 != 0). Unlike inferring from CFM, this register is
        # always answered — it reads a clean 0 during intermittent off-windows.
        cv.Optional(CONF_FANS_RUNNING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
        ),
    }
)


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])

    if fans_running_config := config.get(CONF_FANS_RUNNING):
        b = await binary_sensor.new_binary_sensor(fans_running_config)
        cg.add(broan_component.set_fans_running_binary_sensor(b))
