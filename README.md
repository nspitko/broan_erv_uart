# Broan ERV serial component

!! This is a work in progress !!

The protocol is documented [here](https://spitko.net/2025/08/08/Reverse-Engineering-an-ERV/)

Right now we're mostly missing information on which registers do what. If you have a VTTOUCHW, we could use packet captures from this.

Here's an ESPHome config for the esp32-s3-6ch-relay board waveshare makes. Currently it only supports setting fan mode and speed.

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

```
