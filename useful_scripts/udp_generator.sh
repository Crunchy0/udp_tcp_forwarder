#!/bin/bash

while true
do
	tr -dc A-Za-z0-9 </dev/urandom | head -c 13; echo | ncat -o /dev/stdout -u $1 $2
	sleep 0.01
done
