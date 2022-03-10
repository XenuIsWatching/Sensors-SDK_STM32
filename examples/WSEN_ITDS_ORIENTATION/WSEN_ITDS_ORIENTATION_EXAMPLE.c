/*
 ***************************************************************************************************
 * This file is part of Sensors SDK:
 * https://www.we-online.com/sensors, https://github.com/WurthElektronik/Sensors-SDK_STM32
 *
 * THE SOFTWARE INCLUDING THE SOURCE CODE IS PROVIDED “AS IS”. YOU ACKNOWLEDGE THAT WÜRTH ELEKTRONIK
 * EISOS MAKES NO REPRESENTATIONS AND WARRANTIES OF ANY KIND RELATED TO, BUT NOT LIMITED
 * TO THE NON-INFRINGEMENT OF THIRD PARTIES’ INTELLECTUAL PROPERTY RIGHTS OR THE
 * MERCHANTABILITY OR FITNESS FOR YOUR INTENDED PURPOSE OR USAGE. WÜRTH ELEKTRONIK EISOS DOES NOT
 * WARRANT OR REPRESENT THAT ANY LICENSE, EITHER EXPRESS OR IMPLIED, IS GRANTED UNDER ANY PATENT
 * RIGHT, COPYRIGHT, MASK WORK RIGHT, OR OTHER INTELLECTUAL PROPERTY RIGHT RELATING TO ANY
 * COMBINATION, MACHINE, OR PROCESS IN WHICH THE PRODUCT IS USED. INFORMATION PUBLISHED BY
 * WÜRTH ELEKTRONIK EISOS REGARDING THIRD-PARTY PRODUCTS OR SERVICES DOES NOT CONSTITUTE A LICENSE
 * FROM WÜRTH ELEKTRONIK EISOS TO USE SUCH PRODUCTS OR SERVICES OR A WARRANTY OR ENDORSEMENT
 * THEREOF
 *
 * THIS SOURCE CODE IS PROTECTED BY A LICENSE.
 * FOR MORE INFORMATION PLEASE CAREFULLY READ THE LICENSE AGREEMENT FILE (license_terms_wsen_sdk.pdf)
 * LOCATED IN THE ROOT DIRECTORY OF THIS DRIVER PACKAGE.
 *
 * COPYRIGHT (c) 2022 Würth Elektronik eiSos GmbH & Co. KG
 *
 ***************************************************************************************************
 */

/**
 * @file
 * @brief WSEN_ITDS orientation example.
 *
 * Example for the ITDS accelerometer demonstrating the sensor's 6D and 4D (e.g. portrait/landscape)
 * orientation detection functionality.
 */

#include "WSEN_ITDS_ORIENTATION_EXAMPLE.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "usart.h"
#include "gpio.h"

#include "../SensorsSDK/WSEN_ITDS_2533020201601/WSEN_ITDS_2533020201601.h"

/* Is set to true if device orientation has changed. Is initialized to true,
 * so that orientation is printed on startup. */
bool orientationChanged = true;

/* Stores if sensor is in 4D mode (limitedTo4d = ITDS_enable = 1) or 6D mode (limitedTo4d = ITDS_disable = 0) */
ITDS_state_t limitedTo4d;

/* Sensor initialization function */
bool ITDS_init(void);

/* Debug output functions */
static void debugPrint(char _out[]);
static void debugPrintln(char _out[]);

/**
 * @brief Example initialization.
 * Call this function after HAL initialization.
 */
void WE_itdsOrientationExampleInit()
{
  char bufferMajor[4];
  char bufferMinor[4];
  sprintf(bufferMajor, "%d", WE_SENSOR_SDK_MAJOR_VERSION);
  sprintf(bufferMinor, "%d", WE_SENSOR_SDK_MINOR_VERSION);
  debugPrint("Wuerth Elektronik eiSos Sensors SDK version ");
  debugPrint(bufferMajor);
  debugPrint(".");
  debugPrintln(bufferMinor);
  debugPrintln("This is the \"orientation\" example program for the ITDS sensor.");
  debugPrintln("Note that for this example to work, the following pin/interrupt configuration is required:");
  debugPrintln("* INT_0 to PA0, rising edge interrupt only");

  /* init ITDS */
  if (false == ITDS_init())
  {
    debugPrintln("**** ITDS_Init() error. STOP ****");
    HAL_Delay(5);
    while(1);
  }

  /* Check if orientation detection is limited to 4D. */
  ITDS_is4DDetectionEnabled(&limitedTo4d);
}

/**
 * @brief Example main loop code.
 * Call this function in main loop (infinite loop).
 */
void WE_itdsOrientationExampleLoop()
{
  if (orientationChanged)
  {
    /* Device orientation has changed (set in interrupt) */

    orientationChanged = false;

    /* Get info on 6D orientation change event. */
    ITDS_6dEvent_t sixDEvent;
    ITDS_get6dEventRegister(&sixDEvent);
    debugPrint(limitedTo4d == ITDS_enable ? "4D" : "6D");
    debugPrint(" orientation changed");
    if (limitedTo4d == ITDS_enable)
    {
      bool portrait = (sixDEvent.xlOverThreshold == 1 || sixDEvent.xhOverThreshold == 1) &&
                (sixDEvent.ylOverThreshold == 0 && sixDEvent.yhOverThreshold == 0);
      bool landscape = (sixDEvent.ylOverThreshold == 1 || sixDEvent.yhOverThreshold == 1) &&
          (sixDEvent.xlOverThreshold == 0 && sixDEvent.xhOverThreshold == 0);
      if (portrait)
      {
        debugPrint(" (portrait)");
      }
      if (landscape)
      {
        debugPrint(" (landscape)");
      }
    }
    debugPrint(". [ZH,ZL,YH,YL,XH,YL]=[");
    debugPrint(sixDEvent.zhOverThreshold == 1 ? "1," : "0,");
    debugPrint(sixDEvent.zlOverThreshold == 1 ? "1," : "0,");
    debugPrint(sixDEvent.yhOverThreshold == 1 ? "1," : "0,");
    debugPrint(sixDEvent.ylOverThreshold == 1 ? "1," : "0,");
    debugPrint(sixDEvent.xhOverThreshold == 1 ? "1," : "0,");
    debugPrint(sixDEvent.xlOverThreshold == 1 ? "1" : "0");
    debugPrintln("]");
  }
}

/**
 * @brief Initializes the sensor for this example application.
 */
bool ITDS_init(void)
{
  /* Initialize sensor interface (i2c with ITDS address, burst mode activated) */
  WE_sensorInterface_t interface;
  ITDS_getInterface(&interface);
  interface.interfaceType = WE_i2c;
  interface.options.i2c.burstMode = 1;
  interface.handle = &hi2c1;
  ITDS_initInterface(&interface);

  /* Wait for boot */
  HAL_Delay(50);
  while (WE_SUCCESS != ITDS_isInterfaceReady())
  {
  }
  debugPrintln("**** ITDS_isInterfaceReady(): OK ****");

  HAL_Delay(5);

  /* First communication test */
  uint8_t deviceIdValue = 0;
  if (WE_SUCCESS == ITDS_getDeviceID(&deviceIdValue))
  {
    if (deviceIdValue == ITDS_DEVICE_ID_VALUE) /* who am i ? - i am WSEN-ITDS! */
    {
      debugPrintln("**** ITDS_DEVICE_ID_VALUE: OK ****");
    }
    else
    {
      debugPrintln("**** ITDS_DEVICE_ID_VALUE: NOT OK ****");
      return false;
    }
  }
  else
  {
    debugPrintln("**** ITDS_getDeviceID(): NOT OK ****");
    return false;
  }

  /* Perform soft reset of the sensor */
  ITDS_softReset(ITDS_enable);
  ITDS_state_t swReset;
  do
  {
    ITDS_getSoftResetState(&swReset);
  } while (swReset);
  debugPrintln("**** ITDS reset complete ****");

  /* Perform reboot (retrieve trimming parameters from nonvolatile memory) */
  ITDS_reboot(ITDS_enable);
  ITDS_state_t boot;
  do
  {
    ITDS_isRebooting(&boot);
  } while (boot);
  debugPrintln("**** ITDS reboot complete ****");

  /* Turn on accelerometer (high performance, 200Hz) */
  ITDS_setOperatingMode(ITDS_highPerformance);
  ITDS_setOutputDataRate(ITDS_odr6);

  /* Low noise mode */
  ITDS_enableLowNoise(ITDS_enable);

  /* 2g range */
  ITDS_setFullScale(ITDS_twoG);

  /* Do not use low-pass filter for 6D */
  ITDS_enableLowPassOn6D(ITDS_disable);

  /* Set threshold for orientation change detection to 60° */
  ITDS_set6DThreshold(ITDS_sixtyDeg);

  /* Limit orientation-detection to portrait/landscape computation (common in mobile devices) */
//  ITDS_enable4DDetection(ITDS_enable);

  /* Enable interrupts */
  ITDS_enableInterrupts(ITDS_enable);

  /* Enable 6D orientation change interrupt on INT_0 */
  ITDS_enable6DOnINT0(ITDS_enable);

  return true;
}

#if defined(STM32L432xx)
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin);
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_Pin) == GPIO_PIN_SET)
  {
    HAL_GPIO_EXTI_Rising_Callback(GPIO_Pin);
  }
  else
  {
    HAL_GPIO_EXTI_Falling_Callback(GPIO_Pin);
  }
}
#endif

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_0)
  {
    /* Orientation change interrupt */

    /* Trigger event handling in main function. */
    orientationChanged = true;

    /* Toggle LED */
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
  }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  UNUSED(GPIO_Pin);
}

static void debugPrint(char _out[])
{
  HAL_UART_Transmit(&huart2, (uint8_t *) _out, strlen(_out), 10);
}

static void debugPrintln(char _out[])
{
  HAL_UART_Transmit(&huart2, (uint8_t *) _out, strlen(_out), 10);
  char newline[2] = "\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *) newline, 2, 10);
}
