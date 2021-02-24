/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <spm.h>
#include <cortex_m/tz.h>
#include <sys/printk.h>
#include <stdio.h>

#include "secure_services.h"

void main(void)
{
	spm_config();
	spm_jump();
}
