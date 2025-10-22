#-------------------------------------------------------------------------------
# Zephyr Example Application
#
# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

#target_sources(app PRIVATE src/main.c)
#target_sources(app PRIVATE src/main_thread.c)
#target_sources(app PRIVATE src/main_pwm_led.c)
target_sources(app PRIVATE src/main_pwm_led_fade.c)
