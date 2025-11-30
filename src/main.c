#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#include <string.h>
 
/* Define LEDs for visual feedback */
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios); /* For receiving */
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios); /* For sending */

#define MSG_SIZE 32

/* Define UART devices */
static const struct device *const uart0_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
static const struct device *const uart1_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

/* Message queues for passing data between ISRs and threads */
K_MSGQ_DEFINE(uart0_rx_msgq, MSG_SIZE, 10, 4); // For data from console (UART0)
K_MSGQ_DEFINE(uart1_rx_msgq, MSG_SIZE, 10, 4); // For data from other MCU (UART1)

static void led_green_off_work_handler(struct k_work *work)
{
	gpio_pin_set_dt(&led_green, 0); /* Turn Green LED OFF */
}

static void led_blue_off_work_handler(struct k_work *work)
{
	gpio_pin_set_dt(&led_blue, 0); /* Turn Blue LED OFF */
}

K_WORK_DELAYABLE_DEFINE(led_green_off_work, led_green_off_work_handler);
K_WORK_DELAYABLE_DEFINE(led_blue_off_work, led_blue_off_work_handler);

/* RX buffers and positions for each UART */
static char rx0_buf[MSG_SIZE];
static int rx0_buf_pos;
static char rx1_buf[MSG_SIZE];
static int rx1_buf_pos;

/* Thread definitions */
void console_to_uart1_thread(void);
void uart1_to_console_thread(void);

/* Use preemptive priorities (negative values). Lower number = higher priority. */
/* Thread for printing received messages gets higher priority for responsiveness. */
K_THREAD_DEFINE(uart1_tid, 1024, uart1_to_console_thread, NULL, NULL, NULL, 5, 0, 0);
/* Thread for sending console input gets lower priority. */
K_THREAD_DEFINE(console_tid, 1024, console_to_uart1_thread, NULL, NULL, NULL, 6, 0, 0);

/*
 * UART0 ISR: handles characters from the console
 */
void serial_cb_uart0(const struct device *dev, void *user_data)
{
	uint8_t c;
	if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
		return;
	}

	while (uart_fifo_read(dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx0_buf_pos > 0) {
			rx0_buf[rx0_buf_pos] = '\0';
			k_msgq_put(&uart0_rx_msgq, &rx0_buf, K_NO_WAIT);
			rx0_buf_pos = 0;
		} else if (rx0_buf_pos < (sizeof(rx0_buf) - 1)) {
			if (c != '\r' && c != '\n') {
				rx0_buf[rx0_buf_pos++] = c;
			}
		}
	}
}

/*
 * UART1 ISR: handles characters from the other MCU
 */
void serial_cb_uart1(const struct device *dev, void *user_data)
{
	uint8_t c;
	if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
		return;
	}

	while (uart_fifo_read(dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx1_buf_pos > 0) {
			rx1_buf[rx1_buf_pos] = '\0';
			k_msgq_put(&uart1_rx_msgq, &rx1_buf, K_NO_WAIT);

			/* Blink LED to show a message was received */
			gpio_pin_set_dt(&led_green, 1); /* Turn Green LED ON */
			k_work_reschedule(&led_green_off_work, K_MSEC(50));

			rx1_buf_pos = 0;
		} else if (rx1_buf_pos < (sizeof(rx1_buf) - 1)) {
			if (c != '\r' && c != '\n') {
				rx1_buf[rx1_buf_pos++] = c;
			}
		}
	}
}

/* Thread to send data from console (UART0) to UART1 */
void console_to_uart1_thread(void)
{
	char tx_buf[MSG_SIZE];

	while (k_msgq_get(&uart0_rx_msgq, &tx_buf, K_FOREVER) == 0) {
		printk("Sending: %s\n", tx_buf);
		/* Blink Blue LED to show a message is being sent */
		gpio_pin_set_dt(&led_blue, 1);
		k_work_reschedule(&led_blue_off_work, K_MSEC(50));

		for (int i = 0; i < strlen(tx_buf); i++) {
			uart_poll_out(uart1_dev, tx_buf[i]);
		}
		uart_poll_out(uart1_dev, '\r');
		uart_poll_out(uart1_dev, '\n');
	}
}

/* Thread to print data from UART1 to console (UART0) */
void uart1_to_console_thread(void)
{
	char rx_buf[MSG_SIZE];

	while (k_msgq_get(&uart1_rx_msgq, &rx_buf, K_FOREVER) == 0) {
		printk("Received: %s\n", rx_buf);
	}
}

int main(void)
{
	if (!gpio_is_ready_dt(&led_green)) {
		printk("Green Debug LED not ready!");
		return 0;
	}
	if (!gpio_is_ready_dt(&led_blue)) {
		printk("Blue Debug LED not ready!");
		return 0;
	}
	gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE); /* Green LED is initially OFF */
	gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE); /* Blue LED is initially OFF */

	/* Check and configure UART0 */
	if (!device_is_ready(uart0_dev)) {
		printk("UART0 device not found!");
		return 0;
	}
	/* configure interrupt and callback to receive data from console (UART0) */
	int ret = uart_irq_callback_user_data_set(uart0_dev, serial_cb_uart0, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled for UART0\n");
		} else if (ret == -ENOSYS) {
			printk("UART0 device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART0 callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart0_dev);

	/* Check and configure UART1 */
	if (!device_is_ready(uart1_dev)) {
		printk("UART1 device not found!");
		/* Blink LED rapidly to indicate a fatal error */
		while (1) {
			gpio_pin_toggle_dt(&led_green);
			k_msleep(100);
		}
	}
	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(uart1)), DT_IRQ(DT_NODELABEL(uart1), priority),
				serial_cb_uart1, DEVICE_DT_GET(DT_NODELABEL(uart1)), 0);
	irq_enable(DT_IRQN(DT_NODELABEL(uart1)));
	uart_irq_rx_enable(uart1_dev);

	printk("MCU-to-MCU Chat Application Started\n");
	printk("Type a message and press Enter to send.\n");

	return 0;
}