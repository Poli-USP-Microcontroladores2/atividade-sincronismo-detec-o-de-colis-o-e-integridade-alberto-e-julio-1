#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
 
// Define UART ports
#define UART_TX_NODE DT_NODELABEL(uart0) // UART for transmitting the echo (to PC)
#define UART_RX_NODE DT_NODELABEL(uart1) // UART for receiving from the other MCU

// Visual Feedback
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

#define MSG_SIZE 32

// Queue to store up to 10 messages (aligned to 4-byte boundary)
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_tx_dev = DEVICE_DT_GET(UART_TX_NODE);
static const struct device *const uart_rx_dev = DEVICE_DT_GET(UART_RX_NODE);

// Receive buffer used in UART ISR callback
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

// Define application states
enum app_state {
	STATE_RECEIVING,
	STATE_TRANSMITTING
};


// Read characters from UART until line end is detected. Afterwards push the data to the message queue.
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;
	// This callback is only active during the RECEIVING state.

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	// read until FIFO empty
	while (uart_fifo_read(dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			// terminate string
			rx_buf[rx_buf_pos] = '\0';

			// if queue is full, message is silently dropped
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			// reset the buffer (it was copied to the msgq)
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		// else: characters beyond buffer size are dropped
	}
}



// Print a null-terminated string character by character to the UART interface
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_tx_dev, buf[i]);
	}
}

// Function to blink an LED
void blink_led(const struct gpio_dt_spec *led)
{
	gpio_pin_set_dt(led, 1);
	k_msleep(100);
	gpio_pin_set_dt(led, 0);
}


int main(void)
{
	// --- Device Readiness Checks ---
	if (!device_is_ready(uart_tx_dev)) {
		printk("Error: UART TX (UART0) device not found!");
		return 0;
	}

	if (!device_is_ready(uart_rx_dev)) {
		printk("Error: UART RX (UART1) device not found!");
		return 0;
	}

	if (!gpio_is_ready_dt(&led_g)) {
		printk("Error: Green LED not ready!");
		return 0;
	}

	if (!gpio_is_ready_dt(&led_b)) {
		printk("Error: Blue LED not ready!");
		return 0;
	}

	// --- GPIO Configuration ---
	gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);

	// --- UART Interrupt Configuration ---
	IRQ_CONNECT(DT_IRQN(UART_RX_NODE), DT_IRQ(UART_RX_NODE, priority), serial_cb, DEVICE_DT_GET(UART_RX_NODE), 0);
	irq_enable(DT_IRQN(UART_RX_NODE));

	// --- Main Application Loop ---
	enum app_state current_state = STATE_RECEIVING; // Start in receiving state
	char tx_buf[MSG_SIZE];

	while (1) {
		if (current_state == STATE_TRANSMITTING) {
			print_uart("--- Entering Transmitting Cycle (5s) ---\r\n");
			
			// Disable UART receive interrupt to ignore incoming messages
			uart_irq_rx_disable(uart_rx_dev);

			// Transmit "Message" every 1s for 5s
			for (int i = 0; i < 5; i++) {
				char *msg = "Message\r\n";
				int msg_len = strlen(msg);
				for (int j = 0; j < msg_len; j++) {
					uart_poll_out(uart_rx_dev, msg[j]);
				}
				print_uart("Sent: Message\r\n");
				blink_led(&led_b);
				k_sleep(K_SECONDS(1));
			}

			// Switch to receiving state for the next cycle
			current_state = STATE_RECEIVING;

		} else { // STATE_RECEIVING
			print_uart("--- Entering Receiving Cycle (5s) ---\r\n");

			// Flush any old data from the queue before starting
			k_msgq_purge(&uart_msgq);
			rx_buf_pos = 0;

			// Enable UART receive interrupt
			uart_irq_rx_enable(uart_rx_dev);

			// Wait for messages for 5 seconds
			int64_t end_time = k_uptime_get() + 5000;
			while (k_uptime_get() < end_time) {
				// Check for a message with a short timeout
				if (k_msgq_get(&uart_msgq, &tx_buf, K_MSEC(100)) == 0) {
					print_uart("Recieved: ");
					print_uart(tx_buf);
					print_uart("\r\n");
					blink_led(&led_g);
				}
			}

			// Switch to transmitting state for the next cycle
			current_state = STATE_TRANSMITTING;
		}
	}

	return 0;
}