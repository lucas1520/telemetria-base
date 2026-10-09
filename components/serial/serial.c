#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "common/mavlink.h"
#include "serial.h"
#include "telemetry_store.h"
#include "types.h"

#define UART_PORT	   UART_NUM_2
#define UART_BAUD_RATE 57600

#define UART_TX_PIN    GPIO_NUM_17
#define UART_RX_PIN    GPIO_NUM_16

#define UART_BUFFER_SIZE 1024
#define UART_READ_CHUNK  128
#define UART_READ_TIMEOUT_MS 50

// 0 = aceita qualquer sysid
#define MAVLINK_EXPECTED_SYSID 0
// -1 = aceita qualquer id de bateria
#define MAVLINK_EXPECTED_BATTERY_ID -1

#define BOAT_QUEUE_TIMEOUT_MS 10
#define MAVLINK_STATS_PERIOD_US (5 * 1000 * 1000)

static void handle_mavlink_message(const mavlink_message_t *msg, QueueHandle_t data_boat_queue);
static void handle_battery_status(const mavlink_message_t *msg, QueueHandle_t data_boat_queue);
static bool battery_voltage_v(const mavlink_battery_status_t *battery, float *voltage);

static const char *TAG = "UART";

void read_serial_task(void *pvParameters) {
	QueueHandle_t data_boat_queue = (QueueHandle_t)pvParameters;

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

	uint8_t data[UART_READ_CHUNK];
	mavlink_message_t msg;
	mavlink_status_t status;
	int64_t last_stats_us = esp_timer_get_time();
	bool stack_logged = false;
	uint32_t rx_bytes = 0;

	while (1) {
		int length = uart_read_bytes(
			UART_PORT,
			data,
			sizeof(data),
			pdMS_TO_TICKS(UART_READ_TIMEOUT_MS)
		);

		if (length > 0) {
			rx_bytes += length;
		}

		for (int i = 0; i < length; i++) {
			if (mavlink_parse_char(MAVLINK_COMM_0, data[i], &msg, &status)) {
				handle_mavlink_message(&msg, data_boat_queue);
			}
		}

		int64_t now_us = esp_timer_get_time();
		if (now_us - last_stats_us >= MAVLINK_STATS_PERIOD_US) {
			last_stats_us = now_us;
			mavlink_status_t *stats = mavlink_get_channel_status(MAVLINK_COMM_0);
			ESP_LOGI(
				TAG,
				"MAVLink bytes=%u rx_ok=%u rx_drop=%u parse_error=%u",
				(unsigned)rx_bytes,
				(unsigned)stats->packet_rx_success_count,
				(unsigned)stats->packet_rx_drop_count,
				(unsigned)stats->parse_error
			);

			if (!stack_logged) {
				stack_logged = true;
				ESP_LOGI(TAG, "stack livre minima: %u bytes", (unsigned)uxTaskGetStackHighWaterMark(NULL));
			}
		}
	}
}

static void handle_mavlink_message(const mavlink_message_t *msg, QueueHandle_t data_boat_queue) {
	if (MAVLINK_EXPECTED_SYSID != 0 && msg->sysid != MAVLINK_EXPECTED_SYSID) {
		return;
	}

	switch (msg->msgid) {
	case MAVLINK_MSG_ID_HEARTBEAT:
		telemetry_store_update_heartbeat();
		break;
	case MAVLINK_MSG_ID_BATTERY_STATUS:
		handle_battery_status(msg, data_boat_queue);
		break;
	default:
		break;
	}
}

static void handle_battery_status(const mavlink_message_t *msg, QueueHandle_t data_boat_queue) {
	mavlink_battery_status_t battery;
	mavlink_msg_battery_status_decode(msg, &battery);

#if MAVLINK_EXPECTED_BATTERY_ID >= 0
	if (battery.id != MAVLINK_EXPECTED_BATTERY_ID) {
		return;
	}
#endif

	float voltage;
	if (!battery_voltage_v(&battery, &voltage) || battery.current_battery == -1) {
		ESP_LOGW(TAG, "BATTERY_STATUS sem tensao/corrente validas, amostra descartada");
		return;
	}

	struct data_boat sample = {
		.voltage_instant = voltage,
		.current_instant = battery.current_battery / 100.0f
	};

	telemetry_store_update_battery(sample.voltage_instant, sample.current_instant);

	if (xQueueSend(data_boat_queue, &sample, pdMS_TO_TICKS(BOAT_QUEUE_TIMEOUT_MS)) != pdTRUE) {
		ESP_LOGW(TAG, "data_boat_queue cheia, amostra descartada");
	}
}

// Tensao do pack = soma das celulas validas (mV -> V).
// Celulas 1-10: UINT16_MAX = nao usada. Celulas 11-14 (voltages_ext): 0 = nao usada.
static bool battery_voltage_v(const mavlink_battery_status_t *battery, float *voltage) {
	uint32_t total_mv = 0;
	bool valid = false;

	for (int i = 0; i < 10; i++) {
		if (battery->voltages[i] != UINT16_MAX) {
			total_mv += battery->voltages[i];
			valid = true;
		}
	}

	for (int i = 0; i < 4; i++) {
		if (battery->voltages_ext[i] != 0) {
			total_mv += battery->voltages_ext[i];
			valid = true;
		}
	}

	if (!valid) {
		return false;
	}

	*voltage = total_mv / 1000.0f;
	return true;
}
