/**
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
 * COPYRIGHT (c) 2021 Würth Elektronik eiSos GmbH & Co. KG
 *
 ***************************************************************************************************
 **/

#include "WSEN_ITDS_SELF_TEST_EXAMPLE.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "usart.h"
#include "gpio.h"

#include "../SensorsSDK/WSEN_ITDS_2533020201601/WSEN_ITDS_2533020201601.h"

/* Number of samples to use for self-test. The recommended number is 5. */
#define ITDS_SELF_TEST_SAMPLE_COUNT 5

/* Self-test positive difference [mg] */
#define ITDS_SELF_TEST_MIN_POS      70.0f
#define ITDS_SELF_TEST_MAX_POS    1500.0f

/* Sensor initialization function */
bool ITDS_init(void);
bool ITDS_selfTest(void);
bool ITDS_discardOldData(void);
bool ITDS_checkSelfTestValue(float value, float valueSelfTest);

/* Debug output functions */
static void debugPrint(char _out[]);
static void debugPrintln(char _out[]);

/**
 * @brief Example initialization.
 * Call this function after HAL initialization.
 */
void WE_itdsSelfTestExampleInit()
{
  char bufferMajor[4];
  char bufferMinor[4];
  sprintf(bufferMajor, "%d", WE_SENSOR_SDK_MAJOR_VERSION);
  sprintf(bufferMinor, "%d", WE_SENSOR_SDK_MINOR_VERSION);
  debugPrint("Wuerth Elektronik eiSos Sensors SDK version ");
  debugPrint(bufferMajor);
  debugPrint(".");
  debugPrintln(bufferMinor);
  debugPrintln("This is the \"self-test\" example program for the ITDS sensor.");

  /* init ITDS */
  if (false == ITDS_init())
  {
    debugPrintln("**** ITDS_Init() error. STOP ****");
    HAL_Delay(5);
    while(1);
  }

  /* LED on */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
  HAL_Delay(5);
}

/**
 * @brief Example main loop code.
 * Call this function in main loop (infinite loop).
 */
void WE_itdsSelfTestExampleLoop()
{
  bool ok = ITDS_selfTest();
  if (ok)
  {
    debugPrintln("Self-test completed successfully.");
  }
  else
  {
    debugPrintln("Self-test completed with errors.");
  }
  debugPrintln("");

  HAL_Delay(1000);
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

  return true;
}

/**
 * @brief Runs the ITDS self-test.
 * @retval True if the test was successful, false if not.
 */
bool ITDS_selfTest(void)
{
  /* Last read raw acceleration values */
  int16_t rawAccX;
  int16_t rawAccY;
  int16_t rawAccZ;

  /* Average of accelerations captured with self-test mode disabled [mg] */
  float avgX = 0;
  float avgY = 0;
  float avgZ = 0;

  /* Average of accelerations captured with self-test mode enabled [mg] */
  float avgXSelfTest = 0;
  float avgYSelfTest = 0;
  float avgZSelfTest = 0;

  /* Perform soft reset of the sensor */
  ITDS_softReset(ITDS_enable);
  ITDS_state_t swReset;
  do
  {
    ITDS_getSoftResetState(&swReset);
  } while (swReset);

  /* Enable block data update */
  ITDS_enableBlockDataUpdate(ITDS_enable);

  /* Full scale +-4g */
  ITDS_setFullScale(ITDS_fourG);

  /* Enable high performance mode */
  ITDS_setOperatingMode(ITDS_highPerformance);

  /* Sampling rate of 50 Hz */
  ITDS_setOutputDataRate(ITDS_odr4);

  /* Wait 100 ms for stable sensor output */
  HAL_Delay(100);

  /* Clear old samples (if any) */
  ITDS_discardOldData();

  /* Collect ITDS_SELF_TEST_SAMPLE_COUNT samples (with sensor not in self-test mode) */
  for (uint8_t i = 0; i < ITDS_SELF_TEST_SAMPLE_COUNT; )
  {
    ITDS_state_t dataReady;
    ITDS_isAccelerationDataReady(&dataReady);

    if (dataReady == ITDS_enable)
    {
      /* Read accelerometer data */
      if (WE_FAIL == ITDS_getRawAccelerations(1, &rawAccX, &rawAccY, &rawAccZ))
      {
        return false;
      }

      /* Convert to mg and add to sum */
      avgX += rawAccX * 0.122f;
      avgY += rawAccY * 0.122f;
      avgZ += rawAccZ * 0.122f;

      i++;
    }
  }

  /* Compute average */
  avgX /= ITDS_SELF_TEST_SAMPLE_COUNT;
  avgY /= ITDS_SELF_TEST_SAMPLE_COUNT;
  avgZ /= ITDS_SELF_TEST_SAMPLE_COUNT;


  /* Enable self-test mode (positive) */
  ITDS_setSelfTestMode(ITDS_positiveAxis);

  /* Wait 100 ms for stable sensor output */
  HAL_Delay(100);

  /* Clear old samples (if any) */
  ITDS_discardOldData();

  /* Collect ITDS_SELF_TEST_SAMPLE_COUNT samples (with sensor in self-test mode) */
  for (uint8_t i = 0; i < ITDS_SELF_TEST_SAMPLE_COUNT; )
  {
    ITDS_state_t dataReady;
    ITDS_isAccelerationDataReady(&dataReady);

    if (dataReady == ITDS_enable)
    {
      /* Read accelerometer data */
      if (WE_FAIL == ITDS_getRawAccelerations(1, &rawAccX, &rawAccY, &rawAccZ))
      {
        return false;
      }

      /* Convert to mg and add to sum */
      avgXSelfTest += rawAccX * 0.122f;
      avgYSelfTest += rawAccY * 0.122f;
      avgZSelfTest += rawAccZ * 0.122f;

      i++;
    }
  }

  /* Compute average */
  avgXSelfTest /= ITDS_SELF_TEST_SAMPLE_COUNT;
  avgYSelfTest /= ITDS_SELF_TEST_SAMPLE_COUNT;
  avgZSelfTest /= ITDS_SELF_TEST_SAMPLE_COUNT;


  /* Check value range for all three axes */
  bool xPass = ITDS_checkSelfTestValue(avgX, avgXSelfTest);
  bool yPass = ITDS_checkSelfTestValue(avgY, avgYSelfTest);
  bool zPass = ITDS_checkSelfTestValue(avgZ, avgZSelfTest);


  /* Print results */
  debugPrintln("********************************");
  debugPrintln("**** ITDS self-test results ****");
  debugPrint("****      X axis: ");
  debugPrint(xPass ? "PASS" : "FAIL");
  debugPrintln("      ****");
  debugPrint("****      Y axis: ");
  debugPrint(yPass ? "PASS" : "FAIL");
  debugPrintln("      ****");
  debugPrint("****      Z axis: ");
  debugPrint(zPass ? "PASS" : "FAIL");
  debugPrintln("      ****");
  debugPrintln("********************************");

  /* Disable self-test mode */
  ITDS_setOutputDataRate(ITDS_odr0);
  ITDS_setSelfTestMode(ITDS_off);

  return (xPass && yPass && zPass);
}

/**
 * @brief Discards the current sample (if any).
 */
bool ITDS_discardOldData(void)
{
  ITDS_state_t dataReady;
  ITDS_isAccelerationDataReady(&dataReady);

  if (dataReady == ITDS_enable)
  {
    int16_t xRawAcc, yRawAcc, zRawAcc;
    if (WE_FAIL == ITDS_getRawAccelerations(1, &xRawAcc, &yRawAcc, &zRawAcc))
    {
      return false;
    }
  }
  return true;
}

/**
 * @brief Checks if the validity of the supplied acceleration values.
 * @param value Acceleration value captured with self-test mode disabled.
 * @param valueSelfTest Acceleration value captured with self-test mode enabled.
 * @retval True if the difference of the supplied values is in the valid range.
 */
bool ITDS_checkSelfTestValue(float value, float valueSelfTest)
{
  float diff = fabs(valueSelfTest - value);
  return (diff >= ITDS_SELF_TEST_MIN_POS && diff <= ITDS_SELF_TEST_MAX_POS);
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