#!/bin/sh

# script to run the garage door monitoring process

/home/hbarta/bin/garage_door 2> /tmp/garage_door.txt | /bin/mosquitto_pub -h mqtt -l -t "HA/brandywine/garage/door" 2>/tmp/garage_mqtt.txt 2>&1
