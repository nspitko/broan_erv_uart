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
* Setting fan mode (Standby, Min, Max, Intermittent, Turbo, and Manual)
* Setting fan speed in manual mode

More features will be added if we can get serial dumps from more advanced units, or get lucky with brute forcing.


## FAQ
Q: I see errors about failed communication
A: Disconnect your wall remote and restart the ERV. the wall remote will consume a different ID and the ERV will never talk to the ESP32 until restarted.

Q: Why doesn't it support X?
A: We most likely need serial capture from a wall remote and/or ERV with that feature. If you have a VTTOUCHW and can do rs485 serial capture, we need dumps that contain: 5 seconds of initial state, then press the button you want supported, then 5-10 more seconds (as long as it takes the ERV to finish doing whatever it's doing). Open an issue and attach the dumps. It's possible to get packet capture by uncommenting the debug section in the uart block below, and commenting out the broan component. the main drawback to this is just it's annoying to get all of the data out of the web console.

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

```
