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

// Definição dos UARTs
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define UART1_NODE DT_NODELABEL(uart1)

#define CYCLE_DURATION_MS 5000
#define STACKSIZE 512
#define MSG_SIZE 32
#define PRIORITY 5

//LEDs
#define LED_GREEN_NODE DT_ALIAS(led0)  // Green LED (PTA19)
#define LED_BLUE_NODE  DT_ALIAS(led1)  // Blue LED (PTA18)
#define LED_RED_NODE   DT_ALIAS(led2)  // Red LED (PTA17)
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);

// Botão de sincronização
#define BUTTON_NODE DT_NODELABEL(user_button_0)
static const struct gpio_dt_spec botao = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios); //PTA16
static struct gpio_callback botao_cb_data;
    // Debounce: mínimo entre pressões em ms
#define DEBOUNCE_MS 300
static atomic_t last_press_ts = ATOMIC_INIT(0);

// === Semáforo de sincronização ===
K_SEM_DEFINE(sync_sem, 0, 100);

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

// === Nova fila para armazenar a ENTRADA DO USUÁRIO ===
K_MSGQ_DEFINE(user_input_msgq, MSG_SIZE, 10, 4);

K_THREAD_STACK_DEFINE(transmit_stack_area, STACKSIZE);
K_THREAD_STACK_DEFINE(receive_stack_area, STACKSIZE);

static const struct device *const uart0_dev = DEVICE_DT_GET(UART_DEVICE_NODE); // Para Logs e printk
const struct device *const uart1_dev = DEVICE_DT_GET(UART1_NODE); // UART1: Comunicação MCU-MCU

/* --- State Machine --- */
enum state {
	STATE_TRANSMIT,
	STATE_RECEIVE
};
atomic_t g_current_state = ATOMIC_INIT(STATE_RECEIVE);
atomic_t g_is_receiving = ATOMIC_INIT(1); // 0 for false, 1 for true
atomic_t g_channel_occupied = ATOMIC_INIT(0); // 0: Livre, 1: Ocupado

/* --- Threads --- */
k_tid_t transmit_tid;
struct k_thread transmit_thread_data;
k_tid_t receive_tid;
struct k_thread receive_thread_data;


/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

// === Novo buffer para a entrada do usuário ===
static char user_rx_buf[MSG_SIZE];
static int user_rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	// 1. Detecção de Ocupação: Sempre que a interrupção dispara, o canal está ocupado.
    atomic_set(&g_channel_occupied, 1);

	bool is_receiving = atomic_get(&g_is_receiving);

	/* read until FIFO empty */
	while (uart_fifo_read(dev, &c, 1) == 1) {
		if (!is_receiving) {
			// Not in receive state, just discard the character to keep FIFO clear
			continue;
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

// Novo callback: Lê a entrada do usuário no UART0 e enfileira para a TX
void serial_cb_user_input(const struct device *dev, void *user_data)
{
    uint8_t c;

    if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
        return;
    }

    while (uart_fifo_read(dev, &c, 1) == 1) {
        if ((c == '\n' || c == '\r') && user_rx_buf_pos > 0) {
            user_rx_buf[user_rx_buf_pos] = '\0';
            
            // Coloca a mensagem na fila para ser enviada pela Transmit Thread
            k_msgq_put(&user_input_msgq, user_rx_buf, K_NO_WAIT); 
            
            // Imprime a mensagem localmente no UART0 (echo)
            printk("Você: %s\r\n", user_rx_buf); 
            
            user_rx_buf_pos = 0;
        } else if (user_rx_buf_pos < (sizeof(user_rx_buf) - 1)) {
            user_rx_buf[user_rx_buf_pos++] = c;

        }
    }
}

//Print a null-terminated string character by character to the UART interface
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart1_dev, buf[i]);
	}
}

void transmit_thread(void *p1, void *p2, void *p3)
{
    char msg_to_send[MSG_SIZE];

    while (1) {
		// A thread ESPERA aqui até que o usuário digite algo (msgq_put no serial_cb_user_input)
        if (k_msgq_get(&user_input_msgq, msg_to_send, K_FOREVER) == 0) {
        
        	// 1. Limpa o flag de ocupação para 'escutar' o canal
        	atomic_set(&g_channel_occupied, 0); 
        
        	// 2. Pequena espera para Carrier Sense
        	k_msleep(5); 
        
        	// 3. Checa o status do canal
        	if (atomic_get(&g_channel_occupied) == 0) {
            	// Canal livre: Transmite.
            	print_uart(msg_to_send);
        	} else {
            	// Colisão detectada: Aborta a TX e sinaliza (Amarelo = Vermelho + Verde).
            	LOG_WRN("Colisão detectada! Canal ocupado. Abortando TX.");
            
            	// SINALIZAÇÃO VISUAL DE COLISÃO (LED AMARELO)
            	gpio_pin_set_dt(&led_red, 1);
            	gpio_pin_set_dt(&led_green, 1); 
            
            	k_msleep(100); // Pisca por 100ms
            
            	gpio_pin_set_dt(&led_red, 0);
            	gpio_pin_set_dt(&led_green, 0);
        	}
		}

        // Sleep para evitar flooding e respeitar o ciclo de transmissão.
        k_msleep(500);
    }
}

void receive_thread(void *p1, void *p2, void *p3)
{
	char tx_buf[MSG_SIZE];
	char rx_data[MSG_SIZE];
	while (1) {
		// Wait forever for a message
		if (k_msgq_get(&uart_msgq, tx_buf, K_FOREVER) == 0) {
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
			// Imprime o dado recebido no Serial Monitor (UART0) ===
            printk("Outro MCU: %s\r\n", rx_data);
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
		gpio_pin_set_dt(&led_blue, 1); // Blink Blue LED to indicate receive cycle
		k_msleep(100);
		gpio_pin_set_dt(&led_blue, 0);
	} else {
		// Suspend receive thread and switch to transmit
		k_thread_suspend(receive_tid);

		atomic_set(&g_current_state, STATE_TRANSMIT);
		atomic_set(&g_is_receiving, 0); // Prevent ISR from processing data
		k_thread_resume(transmit_tid);
		print_uart("--- Transmission Phase ---\r\n");
		gpio_pin_set_dt(&led_red, 1); // Blink Red LED to indicate transmit cycle
		k_msleep(100);
		gpio_pin_set_dt(&led_red, 0);
	}
}

// === Interrupção do botão ===
void botao_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    uint32_t now = (uint32_t)k_uptime_get_32();
    uint32_t last = atomic_get(&last_press_ts);

    if ((now - last) < DEBOUNCE_MS) {
        /* ignora bounce */
        return;
    }
    atomic_set(&last_press_ts, now);
    k_sem_give(&sync_sem);
    LOG_INF("Botão de sincronização pressionado (ISR)");
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

    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

	if (!device_is_ready(uart0_dev)) {
		printk("UART0 device not found!");
		return 0;
	}
	if (!device_is_ready(uart1_dev)) {
        printk("UART1 device (Comms) not found!");
        return 0;
    }

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart1_dev, serial_cb, NULL);
	// === Habilita RX interrupts no UART0 para a entrada do usuário ===
    int ret_user = uart_irq_callback_user_data_set(uart0_dev, serial_cb_user_input, NULL);
	
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

	uart_irq_rx_enable(uart1_dev); // Enable RX interrupts permanently
	uart_irq_rx_enable(uart0_dev); // Habilita RX interrupts no UART0

	// Create and start threads
	transmit_tid = k_thread_create(&transmit_thread_data, transmit_stack_area,
			STACKSIZE, transmit_thread, NULL, NULL, NULL,
			PRIORITY, K_USER, K_NO_WAIT);

	receive_tid = k_thread_create(&receive_thread_data, receive_stack_area,
			STACKSIZE, receive_thread, NULL, NULL, NULL,
			PRIORITY, K_USER, K_NO_WAIT);

	k_thread_suspend(transmit_tid); // Start with transmit thread suspended

    gpio_pin_configure_dt(&botao, GPIO_INPUT | GPIO_PULL_UP);

    // configurar interrupção do botão (borda de descida = pressionado)
    gpio_pin_interrupt_configure_dt(&botao, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&botao_cb_data, botao_isr, BIT(botao.pin));
    gpio_add_callback(botao.port, &botao_cb_data);

	// The main thread can sleep
	while(1){
		// A main thread espera pelo sinal do botão (sync_sem)
        if (k_sem_take(&sync_sem, K_FOREVER) == 0) {
            
            LOG_INF("Botão pressionado. Forçando sincronização para TX (5s).");
            
            // 1. Interrompe qualquer ciclo pendente
            k_timer_stop(&cycle_timer);

            // 2. Força o estado TX
            
            // Suspende a thread que estava rodando (RX ou a thread TX antiga)
            if (atomic_get(&g_current_state) == STATE_RECEIVE) {
                 k_thread_suspend(receive_tid);
            } else {
                 k_thread_suspend(transmit_tid);
            }
            
            atomic_set(&g_current_state, STATE_TRANSMIT);
            atomic_set(&g_is_receiving, 0); 
            k_thread_resume(transmit_tid); // Garante que a thread TX está rodando
            
            print_uart("--- FORCED Synchronization Phase (TX 5s) ---\r\n");
            
            // Sinalização visual
            gpio_pin_set_dt(&led_red, 1); 
            k_msleep(100);
            gpio_pin_set_dt(&led_red, 0);

            // 3. INICIA/REINICIA O CICLO: O próximo timeout será em 5s (fim do TX forçado),
            // e os ciclos subsequentes serão automáticos (5s/5s).
            k_timer_start(&cycle_timer, K_MSEC(CYCLE_DURATION_MS), K_MSEC(CYCLE_DURATION_MS));
        }
	}

	return 0;
}