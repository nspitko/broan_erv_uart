# Broan ERV serial component

An ESP32 component to communicate with Broan, Nutone, Venmar, and VanEE ERVs via their rs485 interface.

**This is a work in progress, expect bugs, missing features, and schema changes**

The protocol is documented [here](https://spitko.net/2025/08/08/Reverse-Engineering-an-ERV/)

Right now we're mostly missing information on which registers do what. If you have a VTTOUCHW, we could use packet captures from this.

## Requirements
1) You will need an esphome device that can communicate over rs485. Some devices come with this out of the box (waveshare esp32-s3-relay-6ch), or you can just buy an external tranceiver and wire it to uart.
2) This library does not coexist with other serial wall remotes. This is a software limitation on the ERV itself, it will only ever respond to one device on the bus. It's theoretically possible to MITM a remote but that's outside the scope of this component. If you want to retain a wall control device, buy one of the aux remotes that use the dry contact interface, those will continue to function as expected.
3) You will ideally want to power this directly from the 12v output on the erv itself. this isn't a hard requirement but if the ERV is power cycled it make take some time for things to re-intitialize. It's generally OK to restart the ESP at any time though. Some devices support this out of the box, but most do not, so if you're not using the waveshare device you'll want a buck converter.

## Supported features
* Setting fan mode (Standby, Min, Max, Intermittent, Turbo, and Med, which is treated as manual control)
* Setting fan speed in manual mode
* Intake temperature
* Filter life left

More features will be added as time allows. I've documented many fields that aren't supported yet.

## FAQ
Q: I see errors about failed communication

A: Either you have the RS495 wires reversed, or you didn't restart the ERV after connecting the ESP32.

Q: Why doesn't it support X?

A: Maybe! Not everything is actually exposed, like the life CFM readings much to my chagrin. For other stuff, I may need dumps from a unit with the feature you're requesting, or it might already be exposed and just needs to be plumbed though. Feel free to open an issue or PR

Q: What if I still want wall controls?

A: You can use the aux remotes, those use the dry contact interface which is a hard override. This is how Broan bypasses the protocol limitation.

### ESPhome yaml
Add this to an existing config.
```
external_components:
  - source:
      type: git
      url: https://github.com/nspitko/broan_erv_uart
      ref: main
    components: [ broan ]

uart:
  id: rs485
  tx_pin: GPIO17 # Change these to match your rs485 tranceiver. 17/18 is txd1/rxd1
  rx_pin: GPIO18
  baud_rate: 38400
  rx_buffer_size: 2048
  #debug:
   # direction: BOTH
   # dummy_receiver: false
   # sequence:
   #   - lambda: UARTDebug::log_hex(direction, bytes, ' ');

broan:
  uart_id: rs485

select:
  - platform: broan
    fan_mode:
      name: "fan mode"

number:
  - platform: broan
    fan_speed:
      name: "fan speed"

sensor:
  - platform: broan
    power:
      name: Power draw
    temperature:
      name: Temperature
    filter_life:
      name: Remaining Filter life

```
