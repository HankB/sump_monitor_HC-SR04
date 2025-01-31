# garage-door

Use an HC-SR04 ul;trasonic distance sensor to determine garage door position. The sensor is mounted above the door resting position when it is open. The distance measured will be

1. Distance to the door.
1. Distance ot the floor.
1. Distance to the vehicle parked in the garage.

This code is copied from <https://github.com/HankB/event-driven-HC-SR04> and modified as needed to replace `garage-door-position.py` from <http://oak:8080/HankB/garage-door-position>. Once each second the python script:

1. Captures 5 consecutive readings (Delaying 0.1s between each.)
1. Selects the median value.
1. Determines "open" or "closed (>40 cm is open.)
1. Constructs a JSON payload including a timestamp, position, previous position, selected reading and 5 readings.
1. Execute `mosquitto_pub` with appropriate arguments and passes it the payload.
1. ssppe for a second.

A typical payload looks like

```text
 {"t": 1737153764, "position": "closed", "previous_position": "open", "selected": 78.28975915908813, "readings": [77.4106502532959, 370.0108051300049, 200.085186958313, 78.28975915908813, 72.38950729370117]}
 ```
## Motivation

Use an HC-SR04 to measure distance with a minimum of computer resources (targeted at a Pi 0). This argues for a compiled language that uses an interrupt to measure the return pulse time. (There are a lot of existing projects on Github that use Python and/or C/C++ and which poll for the return pulse, tying up the single core available in the Pi 0,)

* <https://www.handsontec.com/dataspecs/HC-SR04-Ultrasonic.pdf> user guide.

## Plan

Rename `hcsr04_distance.c` to `garage_door.c` and modify to support the requirements listed above.

## Status

* 2025-01-14 Copied from original project, removed debug code.
* 2025-01-14 5 readings, report median.

## H/W configuration

(Initially the same as the original project.)

S/W is targeted for a Pi Zero and development is performed on a Pi 3B with the HC-SR04 connected as follows:

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
gcc -Wall -o garage_door garage_door.c  -l gpiod
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
garage_door | mosquitto_pub -h mqtt -l -t "HA/brandywine/garage/door_test"
```

See also comments in the Systemd service file

```text
# Place in ~/.config/systemd/user/
# systemctl --user daemon-reload
# systemctl --user status garage_door
# systemctl --user start garage_door
# systemctl --user enable garage_door
```

Initial tests seem to indicate that this works.

## Errata

`libgpiod` was selected over WiringPi because `libgpiod` provides timestamped transitions that eliminate the need to poll the echo GPIO. On line documentation for `libgpiod` is for V2 whereas V1.6 is available in the repos. Documentation for V1.6 could not be found on the web abnd is avilable by installing the `libgpiod-doc` package.

The existing Python installation uses GPIO23 for the trigger and GPIO24 to read the echo and reports the distance in CM. This code will be modified to do the same to facilitate easy comparisons and make it a drop in replacement. This change results in roughly comparable measurements except that the C version produces far fewer bogus values. (But is not completely immune - `"readings":[35.7, 35.7, 35.6, 35.7, 181.0]`)
