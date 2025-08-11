# __init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

AUTO_LOAD = []
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@nspitko"]

broan_ns = cg.esphome_ns.namespace("broan")
BroanComponent = broan_ns.class_("BroanComponent", cg.Component, uart.UARTDevice)

CONF_BROAN_ID = "broan_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BroanComponent),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

BroanBaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
    },
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "broan",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


