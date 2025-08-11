# Broan ERV serial component

!! This is a work in progress !!

Theree is some kind of protocol bug right now that causes random misalignment. The code corrects for this but it means some messages get lost, this means it may take several seconds for a value to make its way through.

If you'd like to help, flash this to an esp32 with a rs485 tranceiver, open up the web log, change values and settings on your erv, and note which values change and to what. Please open up an issue with any reporting so we can lock down what each register does.

Additionally, if you want to do packet capture and have a wall remote, we may be able to discover new registers. I'm sure we're missing at least some, as I don't see current CFM reflected in the dataset currently.

Here's an ESPHome config for the esp32-s3-6ch-relay board waveshare makes. Currently it only supports setting fan mode. You'll need to update the external component to point to your local checkout of this repo.

### ESPhome yaml
```
esphome:
  name: esp-erv
  friendly_name: esp-erv

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

# Enable logging
logger:
  baud_rate: 0

external_components:
  - source: custom_components

ota:
  - platform: esphome
    password: "changeme!!!!!!!!!!!!!!!!" 

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "Esp-Erv Fallback Hotspot"
    password: "changeme!!!" 

captive_portal:

uart:
  id: rs485
  tx_pin: GPIO17
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
    
    
```
