#include <stdlib.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "serial.h"
#include "types.h"
#include "data_boat_queue.h"

#define UART_PORT	   UART_NUM_2
#define UART_BAUD_RATE 115200

#define UART_TX_PIN    GPIO_NUM_17
#define UART_RX_PIN    GPIO_NUM_16

#define UART_BUFFER_SIZE 1024

static struct data_boat decode_data(char* raw_data);

static const char *TAG = "UART";
QueueHandle_t data_boat_queue;

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
	uint8_t count = 0;
	while (1) {
		int length = uart_read_bytes(
			UART_PORT,
			data,
			UART_BUFFER_SIZE - 1,
			pdMS_TO_TICKS(1000)
		);

		if (length > 0) {
			data[length] = '\0';

			ESP_LOGI(TAG, "%hhu, Received: %s", count++, (char *)data);

			// printf("RAW: ");
			struct data_boat formatted_data_boat = decode_data((char *)data);

			xQueueSend(data_boat_queue, &formatted_data_boat, portMAX_DELAY);

			// printf("current %f\n", formatted_data_boat.current_instant);
			// printf("voltage %f\n", formatted_data_boat.voltage_instant);

			// for (int i = 0; i < length; i++) {
			// 	printf("%02X", data[i]);
			// }
			//
			// printf("\n");
		}
	}
}

static struct data_boat decode_data(char* raw_data) {
	char delimiters[] = ",";

	char *token = strtok(raw_data, delimiters);

	struct data_boat data_boat = {
		.current_instant = 0.0f,
		.voltage_instant = 0.0f
	};
	// TODO: utilizar ponteiro para verificar quando conversao para float deu errado
	char *endptr;

	if (token == NULL) {
		ESP_LOGE(TAG, "No voltage received");
		return data_boat;
	}
	data_boat.voltage_instant = strtof(token, &endptr);

	token = strtok(NULL, delimiters);

	if (token == NULL) {
		ESP_LOGE(TAG, "No current received");
		return data_boat;
	}

	data_boat.current_instant = strtof(token, &endptr);

	return data_boat;
}
