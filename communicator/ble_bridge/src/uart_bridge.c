/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bidirectional UART bridge between USB CDC ACM #1 and uart0.
 * This provides host access to the nRF9151 console/shell via USB.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_bridge);

#define BRIDGE_BUF_SIZE 512

RING_BUF_DECLARE(uart_to_cdc_ring, BRIDGE_BUF_SIZE);
RING_BUF_DECLARE(cdc_to_uart_ring, BRIDGE_BUF_SIZE);

static const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
static const struct device *const cdc_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart1));

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			uint8_t buf[64];
			int len = uart_fifo_read(dev, buf, sizeof(buf));

			if (len > 0) {
				ring_buf_put(&uart_to_cdc_ring, buf, len);
				uart_irq_tx_enable(cdc_dev);
			}
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t *data;
			uint32_t len = ring_buf_get_claim(&cdc_to_uart_ring,
							  &data, 64);
			if (len > 0) {
				int sent = uart_fifo_fill(dev, data, len);

				ring_buf_get_finish(&cdc_to_uart_ring, sent);
			} else {
				uart_irq_tx_disable(dev);
			}
		}
	}
}

static void cdc_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			uint8_t buf[64];
			int len = uart_fifo_read(dev, buf, sizeof(buf));

			if (len > 0) {
				ring_buf_put(&cdc_to_uart_ring, buf, len);
				uart_irq_tx_enable(uart_dev);
			}
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t *data;
			uint32_t len = ring_buf_get_claim(&uart_to_cdc_ring,
							  &data, 64);
			if (len > 0) {
				int sent = uart_fifo_fill(dev, data, len);

				ring_buf_get_finish(&uart_to_cdc_ring, sent);
			} else {
				uart_irq_tx_disable(dev);
			}
		}
	}
}

static int uart_bridge_init(void)
{
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("uart0 not ready");
		return -ENODEV;
	}

	if (!device_is_ready(cdc_dev)) {
		LOG_ERR("cdc_acm_uart1 not ready");
		return -ENODEV;
	}

	uart_irq_callback_set(uart_dev, uart_isr);
	uart_irq_callback_set(cdc_dev, cdc_isr);

	uart_irq_rx_enable(uart_dev);
	uart_irq_rx_enable(cdc_dev);

	LOG_INF("UART bridge started: uart0 <-> cdc_acm_uart1");
	return 0;
}

SYS_INIT(uart_bridge_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
