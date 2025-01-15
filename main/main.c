#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "dht.h" // Biblioteca para DHT11

// Configurações do DHT11
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;
static const gpio_num_t dht_gpio = 15;

// Definições dos LEDs RGB
#define LED_RED   14   
#define LED_GREEN 13        
#define LED_BLUE  12        

// Constantes para temperatura
#define TEMP_HIGH 300  // Limite para temperatura alta (30.0°C)
#define TEMP_LOW  200  // Limite para temperatura baixa (20.0°C)

// Tag para logs
static const char *TAG = "DHT_Example";

// Função para configurar os pinos dos LEDs
void configure_leds(void) {
    gpio_reset_pin(LED_RED);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_GREEN);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_BLUE);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);
}

// Função para atualizar o estado dos LEDs com base na temperatura
void update_leds(int16_t temperature) {
    // Apaga todos os LEDs
    gpio_set_level(LED_RED, 0);
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_BLUE, 0);

    // Acende o LED correspondente
    if (temperature > TEMP_HIGH) {
        gpio_set_level(LED_RED, 1);
    } else if (temperature > TEMP_LOW) {
        gpio_set_level(LED_GREEN, 1);
    } else {
        gpio_set_level(LED_BLUE, 1);
    }
}

// Função principal da tarefa
void dht_task(void *pvParameters) {
    int16_t temperature = 0;
    int16_t humidity = 0;

    configure_leds(); // Configura os pinos dos LEDs

    while (1) {
        // Tenta ler os dados do sensor
        if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK) {
            // Log dos dados lidos
            ESP_LOGI(TAG, "Umidade: %d.%d%%", humidity / 10, abs(humidity % 10));
            ESP_LOGI(TAG, "Temperatura: %d.%d°C", temperature / 10, abs(temperature % 10));

            // Atualiza os LEDs com base na temperatura
            update_leds(temperature);
        } else {
            ESP_LOGE(TAG, "Erro ao ler os dados do sensor");
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarda 5 segundos
    }
}

// Função principal
void app_main(void) {
    xTaskCreate(dht_task, "dht_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}
