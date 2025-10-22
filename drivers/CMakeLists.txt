# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# zephyr-keep-sorted-start
# Out-of-tree drivers for custom classes
add_subdirectory_ifdef(CONFIG_BLINK blink)

# Out-of-tree drivers for existing driver classes
add_subdirectory_ifdef(CONFIG_SENSOR sensor)

# zephyr-keep-sorted-end