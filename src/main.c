#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>

#define UART1_NODE DT_NODELABEL(uart1)

void main(void)
{
    const struct device *uart1 = DEVICE_DT_GET(UART1_NODE);
    if (!device_is_ready(uart1)) {
        printk("UART1 não está pronta!\n");
        return;
        }

       /* Envia mensagens periódicas pela UART1 */
    while (1) {
        const char msg[] = "UART1!\n";

        /* Transmite pela UART1 */
        for (int i = 0; i < sizeof(msg) - 1; i++) {
            uart_poll_out(uart1, msg[i]);
        }

        /* Lê qualquer coisa recebida pela UART1 e imprime no console (UART0) */
        uint8_t c;
        while (uart_poll_in(uart1, &c) == 0) {
            printk("RX UART1: %c\n", c);
        }

        k_sleep(K_MSEC(1000));
    }
}