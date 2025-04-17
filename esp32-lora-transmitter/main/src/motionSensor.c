#include "../include/motionSensor.h"

volatile bool motionDetected = false;  

// Interrupt Service Routine (ISR) - sets motionDetected to true when triggered
void IRAM_ATTR pir_isr_handler(void *arg) {
    motionDetected = true;
}

// Function to check motion and reset flag
bool isMotionDetected() {
    if (motionDetected) {
        motionDetected = false;  // Reset flag after reading
        return true;
    }
    return false;
}

// Function to initialize the PIR sensor
void initMotionSensor() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE  // Trigger on motion detection
    };
    gpio_config(&io_conf);

    // Install ISR handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_SENSOR_PIN, pir_isr_handler, NULL);
}