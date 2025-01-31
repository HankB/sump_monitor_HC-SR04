# sump monitor using HC-SR04 ultrasonic sensor

This project uses the `garage-door` (private) project as a starting point in order to leverage existing code. The function will be to read the distance from a suitably mounted sensor to water surface and publish the values to an MQTT broker by piping JSON formatted results to `mosquitto_pub`. The task is intended to be run periodically from a `cron` job.

## Motivation

Use an HC-SR04 to measure distance with a minimum of computer resources (targeted at a Pi 0). This argues for a compiled language that uses an interrupt to measure the return pulse time. (There are a lot of existing projects on Github that use Python and/or C/C++ and which poll for the return pulse, tying up the single core available in the Pi 0,)

* <https://www.handsontec.com/dataspecs/HC-SR04-Ultrasonic.pdf> user guide.

## Plan

Rename `garage_door.c` to `sump_monitor.c` and modify to support the requirements listed above. Initially it will be modified to print readings to STDOUT in orde4r to facilitate a site survey to explore location and mounting of the sensor and a Pi Zero W.

## Status

* 2025-01-31 Copied from original project. (Dev on `chiron` on a 3B.)

## H/W configuration

(Initially the same as the original project.)

S/W is targeted for a Pi Zero W and development is performed on a Pi 3B with the HC-SR04 connected as follows:

|cable|signal|pin|GPIO|notes|
|---|---|---|---|---|
|green|GND|20|
|orange|echo|18|23|Resistor divider at sensor end.|
|yellow|trigger|16|24|
|red|VCC|4|

Color codes are relative to the DuPont jumpers I used. The resistor divider is 1.5K Echo to orange and 3K orange to ground.

## Device details

* <https://www.digikey.com/htmldatasheets/production/1979760/0/0/1/hc-sr04.html> " we suggest to use over 60ms measurement cycle, in order to prevent trigger signal to the echo signal."
* <https://randomnerdtutorials.com/complete-guide-for-ultrasonic-sensor-hc-sr04/> "This sensor reads from 2cm to 400cm (0.8 inch to 157 inch) with an accuracy of 0.3cm (0.1inches), ..."

## Build Requirements

```text
gcc -Wall -o sump_monitor sump_monitor.c  -l gpiod
```

### Build host

* `build-essential` 
* `libgpiod2` (Confusing name. There is a V2 of `libgpiod` but it is not in the Bookworm repos)
* `libgpiod-dev`
* `libgpiod-doc`

### Target host

* `libgpiod2`

## Usage

```text
sump_monitor | mosquitto_pub -h mqtt -l -t "HA/brandywine/garage/door_test"
```

## Errata

`libgpiod` was selected over WiringPi because `libgpiod` provides timestamped transitions that eliminate the need to poll the echo GPIO. On line documentation for `libgpiod` is for V2 whereas V1.6 is available in the repos. Documentation for V1.6 could not be found on the web abnd is avilable by installing the `libgpiod-doc` package.

The existing Python installation uses GPIO23 for the trigger and GPIO24 to read the echo and reports the distance in CM. This code will be modified to do the same to facilitate easy comparisons and make it a drop in replacement. This change results in roughly comparable measurements except that the C version produces far fewer bogus values. (But is not completely immune - `"readings":[35.7, 35.7, 35.6, 35.7, 181.0]`)

Files that include 'garage' in their name are probably going to go away.