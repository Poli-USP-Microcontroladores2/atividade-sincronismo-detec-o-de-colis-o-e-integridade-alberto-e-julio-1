#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/atomic.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(LOG_INF_APP, LOG_LEVEL_INF);

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define CYCLE_DURATION_MS 10000
#define STACKSIZE 512
#define MSG_SIZE 32
#define PRIORITY 5
#define BOARD_TYPE 1 //Placa A: 0, Placa B: 1 Serve para definir o comportamento inicial ao pressionar o botão de sincronismo.

//LEDs
#define LED_GREEN_NODE DT_ALIAS(led0)  // Green LED (PTA19)
#define LED_BLUE_NODE  DT_ALIAS(led1)  // Blue LED (PTA18)
#define LED_RED_NODE   DT_ALIAS(led2)  // Red LED (PTA17)
#define BUTTON_NODE_SYNCB DT_NODELABEL(user_button_0) //Botao de sincronismo PTA16

static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
static const struct gpio_dt_spec buttonSync = GPIO_DT_SPEC_GET(BUTTON_NODE_SYNCB, gpios);
static struct gpio_callback button_cbsync_data;
int64_t button_sync_debounce = 0;

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

K_THREAD_STACK_DEFINE(transmit_stack_area, STACKSIZE);
K_THREAD_STACK_DEFINE(receive_stack_area, STACKSIZE);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* --- State Machine --- */
enum state {
	STATE_TRANSMIT,
	STATE_RECEIVE
};

atomic_t g_current_state = ATOMIC_INIT(STATE_TRANSMIT);
atomic_t g_is_receiving = ATOMIC_INIT(0); // 0 for false, 1 for true
atomic_t g_is_Idle = ATOMIC_INIT(1); //1 for true, 0 for false

/* --- Threads --- Probably should just change the order...*/
k_tid_t transmit_tid;
struct k_thread transmit_thread_data;
k_tid_t receive_tid;
struct k_thread receive_thread_data;

/* Forward declarations --- Probably should just change the order... */
extern struct k_timer cycle_timer;
void print_uart(char *buf);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

//ISR do botão de Sincronismo
void buttonSync_isr(const struct device *devsync, struct gpio_callback *cbsync, uint32_t pins)
{
    unsigned int key = irq_lock();
    if ((k_cyc_to_ms_floor32(k_cycle_get_32() - button_sync_debounce)) >= 100)
    {
		if (atomic_cas(&g_is_Idle, 1, 0)) { // If we were idle, this is the first press
			LOG_INF("Button pressed: Exiting Idle Mode.");
			
			//Turn off the White LED
			gpio_pin_set_dt(&led_red, 0);
			gpio_pin_set_dt(&led_green, 0);
			gpio_pin_set_dt(&led_blue, 0);

			// Start the cycle timer for the first time
			k_timer_start(&cycle_timer, K_MSEC(CYCLE_DURATION_MS), K_MSEC(CYCLE_DURATION_MS));
		} else {
			LOG_INF("Button pressed: Forcing %s mode.", (BOARD_TYPE == 1) ? "Receive" : "Transmit");
			// If not idle, just restart the timer to reset the cycle period
			k_timer_start(&cycle_timer, K_MSEC(CYCLE_DURATION_MS), K_MSEC(CYCLE_DURATION_MS));
		}

		// Force state based on BOARD_TYPE
		if (BOARD_TYPE == 1) { // Board B: Force Receive Mode
			k_thread_suspend(transmit_tid);
			atomic_set(&g_current_state, STATE_RECEIVE);
			atomic_set(&g_is_receiving, 1);
			k_msgq_purge(&uart_msgq);
			rx_buf_pos = 0;
			k_thread_resume(receive_tid);
			LOG_INF("\n--- Switched to Receive Phase by Sync Button---\r\n");
		} else { // Board A (BOARD_TYPE == 0): Force Transmit Mode
			k_thread_suspend(receive_tid);
			atomic_set(&g_current_state, STATE_TRANSMIT);
			atomic_set(&g_is_receiving, 0);
			k_thread_resume(transmit_tid);
			LOG_INF("\n--- Switched to Transmit Phase by Sync Button ---\r\n");
		}
		button_sync_debounce = k_cycle_get_32();
    }
    irq_unlock(key);
}

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	// If the system is in idle mode, ignore all incoming data.
	if (atomic_get(&g_is_Idle)) {
		// Read and discard all characters from the FIFO to keep it clear.
		while (uart_fifo_read(uart_dev, &c, 1) == 1) {
			// Do nothing with the character 'c'
		}
		return; // Exit the ISR
	}

	bool is_receiving = atomic_get(&g_is_receiving);

	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if (!is_receiving) {
			// Received a character while in transmit mode. This indicates a collision or sync issue.
			// Force switch to receive mode.
			LOG_WRN("Collision detected! Forcing switch to Receive Mode.");

			// Blink Red LED to indicate collision
			gpio_pin_set_dt(&led_red, 1);
			k_busy_wait(100000); // 100ms busy wait
			gpio_pin_set_dt(&led_red, 0);

			// Switch to receive state
			k_thread_suspend(transmit_tid);
			atomic_set(&g_current_state, STATE_RECEIVE);
			atomic_set(&g_is_receiving, 1);
			k_msgq_purge(&uart_msgq);
			rx_buf_pos = 0;
			k_thread_resume(receive_tid);

			// Restart the cycle timer
			k_timer_start(&cycle_timer, K_MSEC(CYCLE_DURATION_MS), K_MSEC(CYCLE_DURATION_MS));
			return; // Exit ISR, state has been changed.
		}

		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}


//Print a null-terminated string character by character to the UART interface
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

void transmit_thread(void *p1, void *p2, void *p3)
{
	uint8_t random_char_selector;
	while (1) {
		sys_rand_get(&random_char_selector, sizeof(random_char_selector));

		// Blink blue LED to indicate a message is being sent
		gpio_pin_set_dt(&led_blue, 1);
		k_msleep(50); // Short blink duration

		switch (random_char_selector % 3) {
		case 0:
			print_uart("Message 1\r\n");
			break;
		case 1:
			print_uart("Option 2\r\n");
			break;
		case 2:
			print_uart("Third Option\r\n");
			break;
		}

		gpio_pin_set_dt(&led_blue, 0);

		// Sleep to avoid flooding the channel
		k_msleep(500);
	}
}

void receive_thread(void *p1, void *p2, void *p3)
{
	char tx_buf[MSG_SIZE];
	while (1) {
		// Wait forever for a message
		if (k_msgq_get(&uart_msgq, tx_buf, K_FOREVER) == 0) {
			// Blink green LED to indicate a message was received.
			gpio_pin_set_dt(&led_green, 1);
			k_msleep(50); // Short blink duration
			gpio_pin_set_dt(&led_green, 0);

			if (strcmp(tx_buf, "red") == 0) { // Case-sensitive match
				gpio_pin_set_dt(&led_red, 1);
				k_msleep(100);
				gpio_pin_set_dt(&led_red, 0);
			} else if (strcmp(tx_buf, "green") == 0) { // Case-sensitive match
				gpio_pin_set_dt(&led_green, 1);
				k_msleep(100);
				gpio_pin_set_dt(&led_green, 0);
			} else if (strcmp(tx_buf, "blue") == 0) { // Case-sensitive match
				gpio_pin_set_dt(&led_blue, 1);
				k_msleep(100);
				gpio_pin_set_dt(&led_blue, 0);
			}
		}
	}
}

void cycle_timer_handler(struct k_timer *timer_id)
{
	enum state current = atomic_get(&g_current_state);
	if (current == STATE_TRANSMIT) {
		// Suspend transmit thread and switch to receive
		k_thread_suspend(transmit_tid);

		atomic_set(&g_current_state, STATE_RECEIVE);
		atomic_set(&g_is_receiving, 1); // Allow ISR to process data
		k_msgq_purge(&uart_msgq); // Purge queue to remove stale data
		rx_buf_pos = 0; // Reset buffer position
		k_thread_resume(receive_tid);
		print_uart("--- Receive Phase ---\r\n");
	} else {
		// Suspend receive thread and switch to transmit
		k_thread_suspend(receive_tid);

		atomic_set(&g_current_state, STATE_TRANSMIT);
		atomic_set(&g_is_receiving, 0); // Prevent ISR from processing data
		k_thread_resume(transmit_tid);
		print_uart("--- Transmission Phase ---\r\n");
	}
}

K_TIMER_DEFINE(cycle_timer, cycle_timer_handler, NULL);

int main(void)
{
	LOG_INF("Main Thread - Starting... - V: %s - %s \n", __DATE__, __TIME__);
	k_msleep(100); //Allow time to print the log
	// Setup LEDs
    if (!device_is_ready(led_green.port) || !device_is_ready(led_red.port) || !device_is_ready(led_blue.port)){
        LOG_ERR("LED is not ready");
        return 1;
    }

    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_ACTIVE);

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	// Sync Button
	gpio_pin_configure_dt(&buttonSync, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&buttonSync, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cbsync_data, buttonSync_isr, BIT(buttonSync.pin));
    gpio_add_callback(buttonSync.port, &button_cbsync_data);
    button_sync_debounce = k_cycle_get_32();

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}

	uart_irq_rx_enable(uart_dev); // Enable RX interrupts permanently

	// Create and start threads
	transmit_tid = k_thread_create(&transmit_thread_data, transmit_stack_area,
			STACKSIZE, transmit_thread, NULL, NULL, NULL,
			PRIORITY, K_USER, K_NO_WAIT);

	receive_tid = k_thread_create(&receive_thread_data, receive_stack_area,
			STACKSIZE, receive_thread, NULL, NULL, NULL,
			PRIORITY, K_USER, K_NO_WAIT);

	// Start with both threads suspended for Idle mode
	k_thread_suspend(receive_tid); // Start with receive thread suspended
	k_thread_suspend(transmit_tid); // Start with transmit thread suspended
	LOG_INF("System is in Idle Mode. Press the sync button to start.");
	LOG_INF("Starting in receive mode in 5 seconds if no button is pressed...");

	k_msleep(100); //Time to LOG/Print

	// Wait for 5 seconds, checking every 100ms if the button has been pressed
	for (int i = 0; i < 50; i++) {
		if (atomic_get(&g_is_Idle) == 0) {
			// Button was pressed, ISR handled the startup.
			break;
		}
		k_msleep(100);
	}

	// If after 5s we are still idle, start in receive mode automatically
	if (atomic_cas(&g_is_Idle, 1, 0)) {
		LOG_INF("5s timeout expired. Starting automatically in Receive Mode.");
		//Turn off the White LED
		gpio_pin_set_dt(&led_red, 0);
		gpio_pin_set_dt(&led_green, 0);
		gpio_pin_set_dt(&led_blue, 0);
		k_timer_start(&cycle_timer, K_MSEC(CYCLE_DURATION_MS), K_MSEC(CYCLE_DURATION_MS));
		atomic_set(&g_current_state, STATE_RECEIVE);
		atomic_set(&g_is_receiving, 1);
		k_thread_resume(receive_tid);
		print_uart("--- Started in Receive Phase (Timeout) ---\r\n");
	}

	// The main thread can sleep
	while(1){
		k_sleep(K_FOREVER); // The main thread has nothing else to do, so it can sleep forever.
	}

	return 0;
}
