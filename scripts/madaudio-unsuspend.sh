#!/bin/sh

if [ x"$ACTION" = x"change" -a x"$POWER_SUPPLY_TYPE" = x"USB" ]
then
	if [ $POWER_SUPPLY_ONLINE -eq 1 ]
	then
		killall -usr1 madaudio-unsuspend
	else
		killall -usr2 madaudio-unsuspend
	fi
fi
