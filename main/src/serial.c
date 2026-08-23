#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "serial.h"

#define UART_PORT	   UART_NUM_2
#define UART_BAUD_RATE 115200

#define UART_TX_PIN    GPIO_NUM_17
#define UART_RX_PIN    GPIO_NUM_16

#define UART_BUFFER_SIZE 1024

static const char *TAG = "UART";

void read_serial_task(void *pvParameters) {
	uart_config_t uart_config = {
		.baud_rate	= UART_BAUD_RATE,
		.data_bits	= UART_DATA_8_BITS,
		.parity		= UART_PARITY_DISABLE,
		.stop_bits	= UART_STOP_BITS_1,
		.flow_ctrl	= UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT
	};


	ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

	ESP_ERROR_CHECK(
		uart_set_pin(
			UART_PORT,
			UART_TX_PIN,
			UART_RX_PIN,
			UART_PIN_NO_CHANGE,
			UART_PIN_NO_CHANGE
		)
	);

	ESP_ERROR_CHECK(
		uart_driver_install(
			UART_PORT,
			UART_BUFFER_SIZE,
			0,
			0,
			NULL,
			0
		)
	);

	ESP_LOGI(TAG, "UART inicializado");

	uint8_t data[UART_BUFFER_SIZE];

	while (1) {
		int length = uart_read_bytes(
			UART_PORT,
			data,
			UART_BUFFER_SIZE - 1,
			pdMS_TO_TICKS(1000)
		);

		if (length > 0) {
			data[length] = '\0';

			ESP_LOGI(TAG, "Received: %s", (char *)data);

			printf("RAW: ");

			for (int i = 0; i < length; i++) {
				printf("%02X", data[i]);
			}

			printf("\n");
		}
	}
}
