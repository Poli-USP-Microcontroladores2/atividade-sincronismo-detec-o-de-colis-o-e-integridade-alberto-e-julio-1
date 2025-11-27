	#include <zephyr/kernel.h>
	#include <zephyr/device.h>
	#include <zephyr/drivers/uart.h>
	#include <zephyr/drivers/gpio.h>
	#include <zephyr/logging/log.h>
	#include <zephyr/sys/atomic.h>
	#include <stdio.h>
	#include <string.h>

	LOG_MODULE_REGISTER(LOG_INF_APP, LOG_LEVEL_INF);

	// Definição dos UARTs
	#define UART0_NODE DT_CHOSEN(zephyr_shell_uart)
	#define UART1_NODE DT_NODELABEL(uart1)

	#define STACKSIZE 1024 // Aumentado para acomodar a lógica de polling
	#define MSG_SIZE 32
	#define PRIORITY 5

	// LEDs
	#define LED_GREEN_NODE DT_ALIAS(led0)
	#define LED_BLUE_NODE DT_ALIAS(led1)
	#define LED_RED_NODE DT_ALIAS(led2)

	static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
	static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);
	static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);

	// === Dispositivos UART ===
	static const struct device *const uart0_dev = DEVICE_DT_GET(UART0_NODE); 
	static const struct device *const uart1_dev = DEVICE_DT_GET(UART1_NODE); 

	K_THREAD_STACK_DEFINE(transmit_stack_area, STACKSIZE);
	K_THREAD_STACK_DEFINE(receive_stack_area, STACKSIZE);

	/* --- Variáveis de Estado (CSMA/CD) --- */
	// 0: Livre, 1: Ocupado. Detectado pela leitura do UART1.
	atomic_t g_channel_occupied = ATOMIC_INIT(0); 

	/* --- Threads --- */
	k_tid_t transmit_tid;
	struct k_thread transmit_thread_data;
	k_tid_t receive_tid;
	struct k_thread receive_thread_data;

	// Variáveis para comunicação entre threads
	char g_user_message[MSG_SIZE] = {0}; // Mensagem digitada pelo usuário
	atomic_t g_message_ready = ATOMIC_INIT(0); // Flag: 1 quando o usuário digitou uma msg

	// === Funções Auxiliares ===

	// Print a null-terminated string character by character to the UART1 interface (TX)
	void print_uart(char *buf)
	{
		int msg_len = strlen(buf);

		for (int i = 0; i < msg_len; i++) {
			uart_poll_out(uart1_dev, buf[i]);
		}
	}

	/**
	 * Tenta ler uma linha completa (terminada por \r ou \n) do UART0 (Terminal).
	 * Usa polling ativo.
	 * buf Buffer para armazenar a mensagem.
	 * len Tamanho máximo do buffer.
	 * true se uma linha foi lida, false caso contrário.
	 */
	bool uart0_read_line(char *buf, size_t len)
	{
		static int pos = 0;
		unsigned char c;

		// Polling: Tenta ler um caractere
		if (uart_poll_in(uart0_dev, &c) == 0) {
			// Caractere recebido
			uart_poll_out(uart0_dev, c);

			if (c == '\n' || c == '\r') {
				if (pos > 0) {
					buf[pos] = '\0'; // Termina a string
					pos = 0;
					return true; // Linha completa
				}
			} else if (pos < len - 1) {
				buf[pos++] = c;
			}
		}
		return false; // Nenhuma linha completa recebida ainda
	}

	/**
	 * @brief Tenta ler uma linha completa (terminada por \r ou \n) do UART1 (Outro MCU).
	 * Usa polling ativo.
	 * @param buf Buffer para armazenar a mensagem.
	 * @param len Tamanho máximo do buffer.
	 * @return true se uma linha foi lida, false caso contrário.
	 */
	bool uart1_read_line(char *buf, size_t len)
	{
		static int pos = 0;
		unsigned char c;
		int ret;

		// Polling: Tenta ler um caractere
		ret = uart_poll_in(uart1_dev, &c);
		
		if (ret == 0) {
			// Caractere recebido -> O canal está ocupado
			atomic_set(&g_channel_occupied, 1); 

			if (c == '\n' || c == '\r') {
				if (pos > 0) {
					buf[pos] = '\0';
					pos = 0;
					return true;
				}
			} else if (pos < len - 1) {
				buf[pos++] = c;
			}
		} else {
			// Se a leitura falhou, o canal pode estar livre (será resetado na TX thread)
		}
		return false;
	}

	// === Threads ===

	void transmit_thread(void *p1, void *p2, void *p3)
	{
		char msg_to_send[MSG_SIZE]; 

		while (1) {
			// 1. Espera a flag de mensagem do usuário ser levantada pela receive_thread
			if (atomic_get(&g_message_ready) == 1) {

				// Copia a mensagem e reseta a flag
				memcpy(msg_to_send, g_user_message, MSG_SIZE);
				atomic_set(&g_message_ready, 0); 

				// 2. Limpa o flag de ocupação para 'escutar' o canal
				atomic_set(&g_channel_occupied, 0); 
				
				// 3. Pequena espera para Carrier Sense
				k_msleep(5); 
				
				// 4. Checa o status do canal (CSMA/CD básico)
				if (atomic_get(&g_channel_occupied) == 0) {
					// Canal livre: Transmite a mensagem do usuário via UART1.
					print_uart(msg_to_send);
					
					// SINALIZAÇÃO VISUAL DE TX BEM-SUCEDIDA (LED VERDE)
					gpio_pin_set_dt(&led_green, 1);
					k_msleep(50);
					gpio_pin_set_dt(&led_green, 0);
				} else {
					// Colisão detectada: Aborta a TX e sinaliza.
					LOG_WRN("Colisão detectada! Canal ocupado. Abortando TX.");
					
					// SINALIZAÇÃO VISUAL DE COLISÃO (LED AMARELO)
					gpio_pin_set_dt(&led_red, 1);
					gpio_pin_set_dt(&led_green, 1); 
					k_msleep(100); 
					gpio_pin_set_dt(&led_red, 0);
					gpio_pin_set_dt(&led_green, 0);
				}
			}
			k_msleep(1); // Pausa para não monopolizar a CPU
		}
	}

	void receive_thread(void *p1, void *p2, void *p3)
	{
		char rx_data_mcu[MSG_SIZE];
		
		while (1) {
			// === Polling para a entrada do usuário (UART0) ===
			if (uart0_read_line(g_user_message, MSG_SIZE)) {
				// DEBUG: Confirma que a linha completa foi lida
            	printk("\r\n--- DEBUG: Mensagem Lida: %s ---\r\n", g_user_message);
				// Mensagem do usuário lida, sinaliza para a thread de transmissão
				atomic_set(&g_message_ready, 1);
			}

			// === Polling para dados do outro MCU (UART1) ===
			if (uart1_read_line(rx_data_mcu, MSG_SIZE)) {
				// IMPRIME A MENSAGEM RECEBIDA NO SERIAL MONITOR (UART0)
				printk("Outro MCU: %s\r\n", rx_data_mcu); 

				// SINALIZAÇÃO VISUAL DE RX BEM-SUCEDIDA (LED AZUL)
				gpio_pin_set_dt(&led_blue, 1);
				k_msleep(100);
				gpio_pin_set_dt(&led_blue, 0);
				
				// ... (Lógica de comando LED) ...
				if (strcmp(rx_data_mcu, "red") == 0) { 
					gpio_pin_set_dt(&led_red, 1);
					k_msleep(100);
					gpio_pin_set_dt(&led_red, 0);
				} 
				// ... (Continua com outros comandos se desejar) ...
			}
			
			k_msleep(1); // Pausa crucial para não monopolizar a CPU
		}
	}


	int main(void)
	{
		LOG_INF("Main Thread - Chat Polling CSMA/CD - V: %s - %s \n", __DATE__, __TIME__);
		
		// 1. Configuração dos LEDs
		if (!device_is_ready(led_green.port) || !device_is_ready(led_red.port) || !device_is_ready(led_blue.port)){
			LOG_ERR("LED is not ready");
			return 1;
		}

		gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
		gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
		
		// 2. Verificação dos UARTs
		if (!device_is_ready(uart0_dev)) {
			printk("UART0 device (Shell) not found!");
			return 0;
		}
		if (!device_is_ready(uart1_dev)) {
			printk("UART1 device (Comms) not found!");
			return 0;
		}
		
		// 3. Criação e início das threads
		// NOTA: No polling, a receive_thread faz o trabalho de monitorar o UART0 para entrada
		transmit_tid = k_thread_create(&transmit_thread_data, transmit_stack_area,
				STACKSIZE, transmit_thread, NULL, NULL, NULL,
				PRIORITY, K_USER, K_NO_WAIT);

		receive_tid = k_thread_create(&receive_thread_data, receive_stack_area,
				STACKSIZE, receive_thread, NULL, NULL, NULL,
				PRIORITY, K_USER, K_NO_WAIT);

		printk("\n--- Chat Polling Pronto. Digite sua mensagem no terminal. ---\r\n");

		// A main thread apenas dorme
		while(1){
			k_sleep(K_FOREVER);
		}

		return 0;
	}