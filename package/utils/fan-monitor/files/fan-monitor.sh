#!/bin/sh

DEBUG_MODE="false"
TEMP_FILE="/sys/devices/virtual/thermal/thermal_zone0/temp"
SPEED_FILE="/sys/devices/virtual/thermal/cooling_device1/cur_state"

get_temperature() {
	temperature=$(cat $TEMP_FILE)
	temperature=$((temperature/1000))
	echo "$temperature"
}

set_fan_speed() {
	echo "$1" > $SPEED_FILE
}

compare_temperatures() {
	awk -v a="$1" -v b="$2" 'BEGIN{print(a>b)}'
}

calculate_fan_speed() {
	local temperature=$1
	local fan_speed=0

	if [ "$(compare_temperatures $temperature 70)" = "1" ]; then
		fan_speed=255
	elif [ "$(compare_temperatures $temperature 66)" = "1" ]; then
		fan_speed=220
	elif [ "$(compare_temperatures $temperature 62)" = "1" ]; then
		fan_speed=185
	elif [ "$(compare_temperatures $temperature 58)" = "1" ]; then
		fan_speed=150
	elif [ "$(compare_temperatures $temperature 54)" = "1" ]; then
		fan_speed=115
	elif [ "$(compare_temperatures $temperature 50)" = "1" ]; then
		fan_speed=95
	elif [ "$(compare_temperatures $temperature 48)" = "1" ]; then
		fan_speed=80
	elif [ "$(compare_temperatures $temperature 46)" = "1" ]; then
		fan_speed=60
	elif [ "$(compare_temperatures $temperature 45)" = "1" ]; then
		fan_speed=30
	else
		fan_speed=0
	fi

	echo "$fan_speed"
}

set_max_fan_speed() {
	set_fan_speed 255
}

handle_termination() {
	set_max_fan_speed
	exit 0
}

trap handle_termination SIGINT SIGTERM

monitor_fan() {
	if [ ! -f "$SPEED_FILE" ]; then
		echo "Cooling device not found!"
		exit 1
	fi
	while true; do
		temperature=$(get_temperature)
		fan_speed=$(calculate_fan_speed $temperature)

		set_fan_speed $fan_speed

		if [ "$DEBUG_MODE" = "true" ]; then
			echo "Temperature: ${temperature}°C, Fan Speed: $(cat $SPEED_FILE)"
		fi

		sleep 15
	done
}

monitor_fan

