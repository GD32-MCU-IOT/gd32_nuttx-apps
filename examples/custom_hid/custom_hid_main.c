/****************************************************************************
 * apps/examples/custom_hid/custom_hid_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <nuttx/leds/userled.h>
#include <nuttx/input/buttons.h>
#include <nuttx/usb/usbdev_custom_hid.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_EXAMPLES_CUSTOM_HID_DEVPATH
#  define CONFIG_EXAMPLES_CUSTOM_HID_DEVPATH "/dev/hid0"
#endif

#define LED_DEVPATH  "/dev/userleds"
#define BTN_DEVPATH  "/dev/buttons"

/* Button bit positions (match board.h: BUTTON_WAKEUP=0, BUTTON_TAMPER=1) */

#define BTN_WAKEUP_BIT  (1 << 0)
#define BTN_TAMPER_BIT  (1 << 1)

/* Poll period for draining reports and sampling buttons */

#define CUSTOM_HID_LOOP_US  20000

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   Bidirectional Custom HID demo:
 *     - Host OUTPUT reports (id 0x11-0x14) drive board LEDs via
 *       /dev/userleds.
 *     - Board button changes (/dev/buttons) are reported to the host as
 *       INPUT reports (id 0x15 wakeup, 0x16 tamper).
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint8_t rpt[CONFIG_CUSTOM_HID_EPSIZE];
  uint8_t inrpt[2];
  userled_set_t ledset = 0;
  btn_buttonset_t btnprev = 0;
  int hidfd;
  int ledfd;
  int btnfd;

  /* Open the HID device non-blocking so reads drain without stalling */

  hidfd = open(CONFIG_EXAMPLES_CUSTOM_HID_DEVPATH, O_RDWR | O_NONBLOCK);
  if (hidfd < 0)
    {
      fprintf(stderr, "ERROR: Failed to open %s: %d\n",
              CONFIG_EXAMPLES_CUSTOM_HID_DEVPATH, errno);
      return 1;
    }

  /* LED and button devices are optional */

  ledfd = open(LED_DEVPATH, O_RDWR);
  btnfd = open(BTN_DEVPATH, O_RDONLY);

  printf("custom_hid: running (hid=%d led=%d btn=%d)\n",
         hidfd, ledfd, btnfd);

  for (; ; )
    {
      ssize_t n;

      /* 1. Drain all pending host OUTPUT reports and drive LEDs */

      while ((n = read(hidfd, rpt, sizeof(rpt))) >= 2)
        {
          int idx = -1;

          switch (rpt[0])
            {
            case CUSTOM_HID_REPORTID_LED1: idx = 0; break;
            case CUSTOM_HID_REPORTID_LED2: idx = 1; break;
            case CUSTOM_HID_REPORTID_LED3: idx = 2; break;
            case CUSTOM_HID_REPORTID_LED4: idx = 3; break;
            default: break;
            }

          if (idx >= 0)
            {
              if (rpt[1] != 0)
                {
                  ledset |= (1 << idx);
                }
              else
                {
                  ledset &= ~(1 << idx);
                }

              if (ledfd >= 0)
                {
                  ioctl(ledfd, ULEDIOC_SETALL, (unsigned long)ledset);
                }

              printf("custom_hid: LED%d -> %s\n", idx + 1,
                     rpt[1] ? "ON" : "OFF");
            }
        }

      /* 2. Sample buttons and report changes to the host */

      if (btnfd >= 0)
        {
          btn_buttonset_t btnset = 0;

          if (read(btnfd, &btnset, sizeof(btnset)) == sizeof(btnset) &&
              btnset != btnprev)
            {
              if (((btnset ^ btnprev) & BTN_WAKEUP_BIT) != 0)
                {
                  inrpt[0] = CUSTOM_HID_REPORTID_BUTTON1;
                  inrpt[1] = (btnset & BTN_WAKEUP_BIT) ? 1 : 0;
                  write(hidfd, inrpt, sizeof(inrpt));
                }

              if (((btnset ^ btnprev) & BTN_TAMPER_BIT) != 0)
                {
                  inrpt[0] = CUSTOM_HID_REPORTID_BUTTON2;
                  inrpt[1] = (btnset & BTN_TAMPER_BIT) ? 1 : 0;
                  write(hidfd, inrpt, sizeof(inrpt));
                }

              btnprev = btnset;
            }
        }

      usleep(CUSTOM_HID_LOOP_US);
    }

  return 0;
}
