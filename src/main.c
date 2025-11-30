#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#include <string.h>
 
/* Define UART ports */
#define UART_TX_NODE DT_NODELABEL(uart0) /* UART for transmitting the echo (to PC) */
#define UART_RX_NODE DT_NODELABEL(uart1) /* UART for receiving from the other MCU */

/* Use led0 for visual debug feedback */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

#define MSG_SIZE 32

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_tx_dev = DEVICE_DT_GET(UART_TX_NODE);
static const struct device *const uart_rx_dev = DEVICE_DT_GET(UART_RX_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	/* Toggle the LED to confirm the interrupt is firing at all */
	gpio_pin_toggle_dt(&led);

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_tx_dev, buf[i]);
	}
}

int main(void)
{
	char tx_buf[MSG_SIZE];

	if (!device_is_ready(uart_tx_dev)) {
		printk("UART TX (UART0) device not found!");
		return 0;
	}

	/* --- Visual Debug Check for UART1 --- */
	if (!gpio_is_ready_dt(&led)) {
		printk("Debug LED not ready!");
		return 0;
	}
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE); /* LED is initially OFF */

	if (!device_is_ready(uart_rx_dev)) {
		printk("UART RX (UART1) device not found!");
		/* Blink LED rapidly to indicate a fatal error */
		while (1) {
			gpio_pin_toggle_dt(&led);
			k_msleep(100);
		}
		return 0; /* This line will not be reached */
	}

	/* Manually connect our callback to IRQ 13 (UART1) and enable it.
	 * This bypasses the driver's faulty IRQ_CONNECT mechanism.
	 */
	IRQ_CONNECT(DT_IRQN(UART_RX_NODE), DT_IRQ(UART_RX_NODE, priority), serial_cb, DEVICE_DT_GET(UART_RX_NODE), 0);
	irq_enable(DT_IRQN(UART_RX_NODE));
	uart_irq_rx_enable(uart_rx_dev);

	print_uart("UART bridge started. Listening on UART1, echoing to UART0.\r\n");

	/* indefinitely wait for input from the user */
	while (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == 0) {
		print_uart("ECHO:");
		print_uart(tx_buf);
		print_uart("\n");
	}

	return 0;
}