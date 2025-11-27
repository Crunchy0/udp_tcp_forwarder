#!/bin/bash

ncat -k -l -e /bin/cat -o /dev/stdout -p $1
