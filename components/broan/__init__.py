# __init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]  # Ensures UART is set up before this component
AUTO_LOAD = []           # Load no other components automatically

custom_rs485_ns = cg.esphome_ns.namespace("broan")
CustomRS485Component = custom_rs485_ns.class_("Broan", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CustomRS485Component),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], await cg.get_variable(config[uart.CONF_UART_ID]))
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)