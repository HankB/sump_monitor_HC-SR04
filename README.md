# sump monitor using HC-SR04 ultrasonic sensor

Read an HC-SR04 suspended above a sump to determine water level in the sump. (It actually reads distance form sensor to water.)

## Important version information

This branch of the project is a work in progress to rewrite the compiled version in C++ and use the V2.2 version of the GPIOD library which ships with Debian/RPiOS Trixie. If you need a binary that will run on Bookworm or earlier, switch to the `V1.6_using_C` branch


## Motivation

Use an HC-SR04 to measure distance with a minimum of computer resources (targeted at a Pi 0). This argues for a compiled language that uses an interrupt to measure the return pulse time. (There are a lot of existing projects on Github that use Python and/or C/C++ and which poll for the return pulse, tying up the single core available in the Pi 0,)

* <https://www.handsontec.com/dataspecs/HC-SR04-Ultrasonic.pdf> user guide.

## References

* <https://eclipse.dev/paho/files/cppdoc/index.html> Paho MQTT C++ Documentation
* <https://libgpiod.readthedocs.io/en/master/> libgpiod documentation
* <https://libgpiod.readthedocs.io/en/master/cpp_api.html> libgpiod C++ bindings API
* <https://github.com/brgl/libgpiod/tree/master/bindings/cxx/examples> C++ examples

## Plan

Rewrite sump_monitor using the libgpiod C++ bindings which have been found to be easier to use than the C bindings. I already have code that uses the V2.2 GPIOD library at <https://github.com/HankB/event-driven-HC-SR04> and will use that as a starting point.

Regarding MQTT, this application will run, collect readings, publish, exit. There is not a compelling need to include support for MQTT so that will be delegated to `mosquitto_pub`. This program will write the (JSON formatted) payload to STDOUT which will them be piped to `mosquitto_pub` which will publish.

## Status

* 2026-05-19 Start work on Libgpiod V2.2 (Trixie) version of the project.

## H/W configuration

S/W is targeted for a Pi Zero W and development is performed on a Pi 3B with the HC-SR04 connected as follows:

|cable|signal|pin|GPIO|notes|
|---|---|---|---|---|
|green|GND|20|
|orange|echo|18|23|Resistor divider at sensor end.|
|yellow|trigger|16|24|
|red|VCC|4|

Color codes are relative to the DuPont jumpers I used. The resistor divider is 1.5K Echo pin to orange and 3K orange to ground.

## Device details

* <https://www.digikey.com/htmldatasheets/production/1979760/0/0/1/hc-sr04.html> " we suggest to use over 60ms measurement cycle, in order to prevent trigger signal to the echo signal."
* <https://randomnerdtutorials.com/complete-guide-for-ultrasonic-sensor-hc-sr04/> "This sensor reads from 2cm to 400cm (0.8 inch to 157 inch) with an accuracy of 0.3cm (0.1inches), ..."

## Build Requirements

```text
gcc -Wall -o sump_monitor sump_monitor.c  -l gpiod
```

### Build host

* `build-essential` 
* `libgpiod3`
* `libgpiod-dev`

### Target host

* `libgpiod3`

## Usage

```text
sump_monitor | mosquitto_pub -h mqtt -l -t "HA/brandywine/garage/door_test"
```

## Errata

* `libgpiod` was selected over WiringPi because `libgpiod` provides timestamped transitions that eliminate the need to poll the echo GPIO. 

* The existing Python installation uses GPIO23 for the trigger and GPIO24 to read the echo and reports the distance in CM. This code will be modified to do the same to facilitate easy comparisons and make it a drop in replacement. This change results in roughly comparable measurements except that the C version produces far fewer bogus values. (But is not completely immune - `"readings":[35.7, 35.7, 35.6, 35.7, 181.0]`)
