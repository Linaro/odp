#!/bin/sh
#
# Copyright (c) 2022, Nokia
# All rights reserved.
#
# SPDX-License-Identifier:      BSD-3-Clause

TEST_DIR="${TEST_DIR:-$(dirname $0)}"

# Run with a small number of iterations in make check

$TEST_DIR/odp_crypto${EXEEXT} -i 100

if [ $? -ne 0 ] ; then
    echo Test FAILED
    exit 1
fi

exit 0
