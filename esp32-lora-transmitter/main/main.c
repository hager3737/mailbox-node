#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_intr_alloc.h>
#include <esp_log.h>
#include <freertos/task.h>
#include <sx127x.h>
#include <esp_utils.h>
#include <string.h>
#include "include/motionSensor.h"

static const char *TAG = "sx127x";

sx127x device;

void tx_callback(sx127x *device) {
  const char message[5] = {'H', 'e', 'l', 'l', 'o'}; // exactly 5 characters, no \0 sent
  const size_t message_length = 5;

  ESP_ERROR_CHECK(sx127x_tx_set_pa_config(SX127x_PA_PIN_BOOST, 7, device));
  ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_TX, SX127x_MODULATION_LORA, device));
  ESP_ERROR_CHECK(sx127x_lora_tx_set_for_transmission((const uint8_t *)message, message_length, device));

  for (int i = 0; i < 5; i++) {
    ESP_LOGI(TAG, "TX Byte %d: 0x%02X (%c)", i, message[i], message[i]);
}
}

void app_main() {
  ESP_LOGI(TAG, "starting up");
  sx127x_reset();

  vTaskDelay(100 / portTICK_PERIOD_MS);  

  spi_device_handle_t spi_device;
  sx127x_init_spi(&spi_device);

  uint8_t version = sx127x_read_new_register(spi_device, 0x42);
    ESP_LOGI(TAG, "SX1278 Version: 0x%02X", version);


  ESP_ERROR_CHECK(sx127x_create(spi_device, &device));
  ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_SLEEP, SX127x_MODULATION_LORA, &device));
  ESP_ERROR_CHECK(sx127x_set_frequency(437200012, &device));
  ESP_ERROR_CHECK(sx127x_lora_reset_fifo(&device));
  ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_STANDBY, SX127x_MODULATION_LORA, &device));
  ESP_ERROR_CHECK(sx127x_lora_set_bandwidth(SX127x_BW_125000, &device));
  ESP_ERROR_CHECK(sx127x_lora_set_implicit_header(NULL, &device));
  ESP_ERROR_CHECK(sx127x_lora_set_modem_config_2(SX127x_SF_9, &device));
  ESP_ERROR_CHECK(sx127x_lora_set_syncword(18, &device));
  ESP_ERROR_CHECK(sx127x_set_preamble_length(8, &device));
  sx127x_tx_set_callback(tx_callback, &device);

  initMotionSensor();


  setup_gpio_interrupts((gpio_num_t)DIO0, &device, GPIO_INTR_POSEDGE);

  sx127x_tx_header_t header = {
      .enable_crc = true,
      .coding_rate = SX127x_CR_4_5};
  ESP_ERROR_CHECK(sx127x_lora_tx_set_explicit_header(&header, &device));

  ESP_LOGI("ESP32", "Setup complete!");

  while(true) {
    if(isMotionDetected()) {
      ESP_LOGI("PIR", "Motion detected!");
      vTaskDelay(pdMS_TO_TICKS(200));
      ESP_ERROR_CHECK(setup_tx_task(&device, tx_callback));
    }
    vTaskDelay(pdMS_TO_TICKS(10000));  // Avoid busy-waiting (check every 10 seconds)
  }

}