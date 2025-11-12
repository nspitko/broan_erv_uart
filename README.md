# Broan ERV serial component

An ESP32 component to communicate with Broan, Nutone, Venmar, and VanEE ERVs via their rs485 interface.

**This is a work in progress, expect bugs, missing features, and schema changes**

The protocol is documented [here](https://spitko.net/2025/08/08/Reverse-Engineering-an-ERV/)

Right now we're mostly missing information on which registers do what. If you have a VTTOUCHW, we could use packet captures from this.

## Requirements
1) You will need an esphome device that can communicate over rs485. Some devices come with this out of the box (waveshare esp32-s3-relay-6ch), or you can just buy an external tranceiver and wire it to the uart of your choice.
2) This library does not coexist with other serial wall remotes. This is a software limitation on the ERV itself, it will only ever respond to one device on the bus. It's theoretically possible to MITM a remote but that's outside the scope of this component. If you want to retain a wall control device, buy one of the aux remotes that use the dry contact interface, those will continue to function as expected.
3) You will ideally want to power this directly from the 12v output on the erv itself. this isn't a hard requirement but if the ERV is power cycled it make take some time for things to re-intitialize. It's generally OK to restart the ESP at any time though. Some devices support this out of the box, but most do not, so if you're not using the waveshare device you'll want a buck converter.

## Installation
Near the control interface on the ERV, look for a green terminal block that has D+, D-, and GND on it (typically a 6 terminal block that includes 12V, LED, and OVR). Connect the D+, D-, and GND connectors to your RS485 tranceiver. Ideally you can also use the +12v but very few ESP32s are set up to handle input voltages above 5V to check with your spec sheet first. If not, just use a USB cable and wall charger. If you go this route, you may need to additionally run a wire from the GND terminal on the ERV to the GND pin on the esp32, but this will depend on how your rs485 tranceiver is set up. Only do this if you're getting unexplained communication errors or crashes.

Some rs485 trancevers have a jumper for the terminating resistor, some do not. In my case I found I did not need a termination resister, but you may need one. Try enabling this, or adding a resistor manually across the terminals on the ESP32 side, if you have communication issues. You'll know if you flip these because you'll see a bunch of spew about alignment errors and unknown commands.

Also be aware some RS485 devices will label their pins A and B instead of D+ and D-. Somewhat confusingly, A is D- and B is D+

## Supported features
* Setting fan mode (Standby, Min, Max, Intermittent, Turbo, Override, and Med, which is treated as manual control)
* Setting fan speed in manual mode
* Humidity control mode
* Intake temperature
* Filter life left

More features will be added as time allows. I've documented many fields that aren't supported yet. If there's a specific feature you want prioritized, open an issue. This project is at a point where it "works for me" so I don't have a lot of guiding light on what else should be added without external input.

## Humidity Control Mode
In Humidity Control Mode, the controller sets a target humidity level and the ERV automatically runs if the humidity is above this level. The ERV does not have a humidity sensor - the current humidity reading is sent periodically from the controller.  Since we are replacing the original controller, we need to send current humidity readings from the ESP device. This can be done from a lambda in your ESPHome config calling the setCurrentHumidity() function.  For an example configuration, see the [humidity_control_sample.yaml](./examples/humidity_control_example.yaml) configuration in the [examples](./examples/) directory.

To use humidity control mode once it is enabled, set the desired humidity with "Humidity Setpoint", and then turn on the Humidity Control switch.

## FAQ
Q: I see errors about failed communication

A: Either you have the RS495 wires reversed, or you didn't restart the ERV after connecting the ESP32.

Q: Why doesn't it support X?

A: Not everything is actually exposed via the rs485 interface, like the life CFM readings much to my chagrin. For other stuff, I may need dumps from a unit with the feature you're requesting, or it might already be mapped and just needs to be plumbed though to Home Assistant. Feel free to open an issue or PR

Q: What if I still want wall controls?

A: You can use the aux remotes, those use the dry contact interface which is a hard override. The fan mode will indicate "ovr" (override) when these controls are used. This is how Broan bypasses the protocol limitation. If you don't want to control anything, it's possible to snoop the interface to read sensor values, I use this mode for debugging but it's not exposed as a feature yet since it doesn't seem useful to me Feel free to make a case for it.

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
    supply_fan_cfm:
      name: "Supply fan CFM"
    exhaust_fan_cfm:
      name: "Exhaust fan CFM"

    # Not all ERVs have this sensor, remove if you don't get readings from it.
    temperature_out:
      name: Temperature Out

button:
  - platform: broan
    filter_reset:
      name: Filter reset

```
