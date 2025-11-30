#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#define UART1_NODE DT_NODELABEL(uart1)
#define LED2_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

void main(void)
{
    const struct device *uart1 = DEVICE_DT_GET(UART1_NODE);
    if (!device_is_ready(uart1)) {
        printk("UART1 não está pronta!\n");
        return;
    }

    if (!gpio_is_ready_dt(&blue_led)) {
        printk("GPIO for blue LED is not ready!\n");
        return;
    }

    if (gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE) < 0) {
        printk("Failed to configure blue LED!\n");
        return;
    }

    /* Envia mensagens periódicas pela UART1 e pisca o LED */
    while (1) {
        const char msg[] = "UART1!\n";

        /* Turn the LED on to indicate UART transmission */
        gpio_pin_set_dt(&blue_led, 1);

        /* Transmite pela UART1 */
        for (int i = 0; i < sizeof(msg) - 1; i++) {
            uart_poll_out(uart1, msg[i]);
        }

        /* Keep the LED on for a short duration to make the blink visible */
        k_msleep(50);
        /* Turn the LED off */
        gpio_pin_set_dt(&blue_led, 0);

        k_sleep(K_MSEC(950));
    }
}