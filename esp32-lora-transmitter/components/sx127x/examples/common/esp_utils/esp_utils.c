#include "esp_utils.h"
#include <esp_log.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

int total_packets_received = 0;
const UBaseType_t xArrayIndex = 0;
TaskHandle_t handle_interrupt;
static const char *TAG = "esp_utils";
void (*global_tx_callback)(sx127x *device);

void IRAM_ATTR handle_interrupt_fromisr(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveIndexedFromISR(handle_interrupt, xArrayIndex, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void handle_interrupt_task(void *arg) {
  while (1) {
    if (ulTaskNotifyTakeIndexed(xArrayIndex, pdTRUE, portMAX_DELAY) > 0) {
      sx127x_handle_interrupt((sx127x *) arg);
    }
  }
}

void handle_interrupt_tx_task(void *arg) {
  sx127x *device = (sx127x *) arg;
  
    // Transmit "Hello"
    global_tx_callback(device);

    // Check if an interrupt has occurred (non-blocking version)
    if (ulTaskNotifyTakeIndexed(xArrayIndex, pdTRUE, 0) > 0) {
      sx127x_handle_interrupt(device);
    }
    vTaskDelete(NULL);
}

void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
  uint8_t payload[2090 * 2];
  const char SYMBOLS[] = "0123456789ABCDEF";
  for (size_t i = 0; i < data_length; i++) {
    uint8_t cur = data[i];
    payload[2 * i] = SYMBOLS[cur >> 4];
    payload[2 * i + 1] = SYMBOLS[cur & 0x0F];
  }
  payload[data_length * 2] = '\0';

  int32_t frequency_error;
  ESP_ERROR_CHECK(sx127x_rx_get_frequency_error(device, &frequency_error));
  int16_t rssi;
  int code = sx127x_rx_get_packet_rssi(device, &rssi);
  if (code == SX127X_ERR_NOT_FOUND) {
    ESP_LOGI(TAG, "received: %d %s freq_error: %" PRId32, data_length, payload, frequency_error);
  } else {
    ESP_LOGI(TAG, "received: %d %s rssi: %d freq_error: %" PRId32, data_length, payload, rssi, frequency_error);
  }
  total_packets_received++;
}

void lora_rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
  uint8_t payload[514];
  const char SYMBOLS[] = "0123456789ABCDEF";
  for (size_t i = 0; i < data_length; i++) {
    uint8_t cur = data[i];
    payload[2 * i] = SYMBOLS[cur >> 4];
    payload[2 * i + 1] = SYMBOLS[cur & 0x0F];
  }
  payload[data_length * 2] = '\0';

  int16_t rssi;
  ESP_ERROR_CHECK(sx127x_rx_get_packet_rssi(device, &rssi));
  float snr;
  ESP_ERROR_CHECK(sx127x_lora_rx_get_packet_snr(device, &snr));
  int32_t frequency_error;
  ESP_ERROR_CHECK(sx127x_rx_get_frequency_error(device, &frequency_error));
  ESP_LOGI(TAG, "received: %d %s rssi: %d snr: %f freq_error: %" PRId32, data_length, payload, rssi, snr, frequency_error);
  total_packets_received++;
}

void cad_callback(sx127x *device, int cad_detected) {
  if (cad_detected == 0) {
    ESP_LOGI(TAG, "cad not detected");
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_CAD, SX127x_MODULATION_LORA, device));
    return;
  }
  // put into RX mode first to handle interrupt as soon as possible
  ESP_ERROR_CHECK(sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA, device));
  ESP_LOGI(TAG, "cad detected\n");
}

void setup_gpio_interrupts(gpio_num_t gpio, sx127x *device, gpio_int_type_t type) {
  ESP_ERROR_CHECK(gpio_set_direction(gpio, GPIO_MODE_INPUT));
  ESP_ERROR_CHECK(gpio_pulldown_en(gpio));
  ESP_ERROR_CHECK(gpio_pullup_dis(gpio));
  ESP_ERROR_CHECK(gpio_set_intr_type(gpio, type));
  ESP_ERROR_CHECK(gpio_isr_handler_add(gpio, handle_interrupt_fromisr, (void *) device));
}

esp_err_t setup_task(sx127x *device) {
  BaseType_t task_code = xTaskCreatePinnedToCore(handle_interrupt_task, "handle interrupt", 8196, device, 2, &handle_interrupt, xPortGetCoreID());
  if (task_code != pdPASS) {
    ESP_LOGE(TAG, "can't create task %d", task_code);
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t setup_tx_task(sx127x *device, void (*tx_callback)(sx127x *device)) {
  global_tx_callback = tx_callback;
  BaseType_t task_code = xTaskCreatePinnedToCore(handle_interrupt_tx_task, "handle interrupt", 8196, device, 2, &handle_interrupt, xPortGetCoreID());
  if (task_code != pdPASS) {
    ESP_LOGE(TAG, "can't create task %d", task_code);
    return ESP_FAIL;
  }
  return ESP_OK;
}

void sx127x_reset() {
  ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t) RST, GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_level((gpio_num_t) RST, 0));
  vTaskDelay(pdMS_TO_TICKS(5));
  ESP_ERROR_CHECK(gpio_set_level((gpio_num_t) RST, 1));
  vTaskDelay(pdMS_TO_TICKS(10));
  ESP_LOGI(TAG, "sx127x was reset");
}


void sx127x_init_spi(spi_device_handle_t *handle) {
  ESP_LOGI(TAG, "Initializing SPI bus...");

  spi_bus_config_t buscfg = {
      .mosi_io_num = MOSI,
      .miso_io_num = MISO,
      .sclk_io_num = SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0,
  };
  esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, 1);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(err));
      return;
  }

  ESP_LOGI(TAG, "Configuring chip select (CS) pin...");
  gpio_set_direction(SS, GPIO_MODE_OUTPUT);
  gpio_set_level(SS, 1); // CS HIGH before adding the device

  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 1E6, // Reduce speed for debugging
      .spics_io_num = SS,
      .queue_size = 16,
      .command_bits = 0,
      .address_bits = 8,
      .dummy_bits = 0,
      .mode = 0, // If fails, try mode 3
  };

  err = spi_bus_add_device(SPI2_HOST, &devcfg, handle);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(err));
      return;
  }

  ESP_LOGI(TAG, "SPI initialized successfully.");
}

uint8_t sx127x_read_new_register(spi_device_handle_t spi, uint8_t reg) {
  uint8_t tx_data[2] = { reg & 0x7F, 0x00 }; // Read mode (MSB = 0)
  uint8_t rx_data[2] = {0};

  spi_transaction_t t = {
      .length = 16, // Two bytes
      .tx_buffer = tx_data,
      .rx_buffer = rx_data,
  };

  gpio_set_level(SS, 0); // CS LOW
  esp_err_t ret = spi_device_transmit(spi, &t);
  gpio_set_level(SS, 1); // CS HIGH

  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SPI transaction failed: %s", esp_err_to_name(ret));
      return 0xFF;
  }

  ESP_LOGI(TAG, "Register 0x%02X = 0x%02X", reg, rx_data[1]); // rx_data[1] is the response
  return rx_data[1];
}