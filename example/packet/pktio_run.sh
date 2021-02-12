#!/bin/bash
#
# Copyright (c) 2016-2018, Linaro Limited
# All rights reserved.
#
# SPDX-License-Identifier:     BSD-3-Clause
#

cd "$(dirname "$0")"

if  [ -f ./pktio_env ]; then
	. ./pktio_env
else
	echo "ERROR: file not found: $(pwd)/pktio"
        exit 1
fi

setup_interfaces

# burst mode
./odp_pktio${EXEEXT} -i $IF1 -t 5 -m 0
STATUS=$?
if [ ${STATUS} -ne 0 ]; then
	echo "Error: status ${STATUS}"
	exit 1
fi

validate_result
echo "Pass -m 0: status ${STATUS}"

# queue mode
./odp_pktio${EXEEXT} -i $IF1 -t 5 -m 1
STATUS=$?

if [ ${STATUS} -ne 0 ]; then
	echo "Error: status ${STATUS}"
	exit 2
fi

validate_result
echo "Pass -m 1: status ${STATUS}"

# sched/queue mode
./odp_pktio${EXEEXT} -i $IF1 -t 5 -m 2
STATUS=$?

if [ ${STATUS} -ne 0 ]; then
	echo "Error: status ${STATUS}"
	exit 3
fi

validate_result
echo "Pass -m 2: status ${STATUS}"

# cpu number option test 1
./odp_pktio${EXEEXT} -i $IF1 -t 5 -m 0 -c 1
STATUS=$?

if [ ${STATUS} -ne 0 ]; then
	echo "Error: status ${STATUS}"
	exit 4
fi

validate_result
echo "Pass -m 0 -c 1: status ${STATUS}"

cleanup_interfaces

exit 0
