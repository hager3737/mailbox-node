#ifndef MOTIONSENSOR_H
#define MOTIONSENSOR_H

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"

#define PIR_SENSOR_PIN GPIO_NUM_14  // Change this to your actual PIR sensor pin

extern volatile bool motionDetected;  // Declare the variable, but define it in .c

void pir_isr_handler(void *arg);
bool isMotionDetected();
void initMotionSensor();

#endif  // MOTIONSENSOR_H