#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "esp_wifi.h"

#include "dht.h" // Biblioteca para DHT11

// Configurações do DHT11
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;
static const gpio_num_t dht_gpio = 15;

// Definições dos LEDs RGB
#define LED_RED   14   
#define LED_GREEN 13        
#define LED_BLUE  12        
#define GPIO_BUTTON_PIN 2 
#define DEBOUNCE_DELAY_MS 200  
// Constantes para temperatura
#define TEMP_HIGH 300  // Limite para temperatura alta (30.0°C)
#define TEMP_LOW  200  // Limite para temperatura baixa (20.0°C)

// Tag para logs
static const char *TAG = "DHT_Example";

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");


/* Event handler  */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    }
}

// Função validação imagem novo firmware
static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
    // Check if the current version is the same as the new version
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is the same as the new one. We will not continue the update.");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

// OTA task 
void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Advanced OTA example");

    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,  // URL for the OTA update
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = (char *)server_cert_pem_start,
#endif
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
        .buffer_size = 1024, 
        .buffer_size_tx = 1024, 
        .disable_auto_redirect = true,

    };

    // OTA configuration
    esp_https_ota_config_t ota_config = {
        .bulk_flash_erase = true,
        .http_config = &config,
#ifdef CONFIG_EXAMPLE_ENABLE_PARTIAL_HTTP_DOWNLOAD
        .partial_http_download = true,
        .max_http_request_size = CONFIG_EXAMPLE_HTTP_REQUEST_SIZE,
#endif
    };

    // Start the OTA process
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        vTaskDelete(NULL);
    }

    // Get and validate the image description of the new firmware
    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Image header verification failed");
        goto ota_end;
    }

    // Perform the OTA process (download and write to flash)
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    // Check if the entire OTA data was received
    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for stability before rebooting
            esp_restart();  // Restart the ESP32 to apply the new firmware
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            vTaskDelete(NULL);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    vTaskDelete(NULL);
}

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
        //ESP_LOGI(TAG, "Temperatura alta!");
    } else if (temperature > TEMP_LOW) {
        gpio_set_level(LED_GREEN, 1);
        //ESP_LOGI(TAG, "Temperatura ok!");
    } else {
        gpio_set_level(LED_BLUE, 1);
        //ESP_LOGI(TAG, "Temperatura baixa!");

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

// Funcao para verificar se o botao foi pressionado
bool is_button_pressed()
{
    static uint32_t last_press_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Check if the button is pressed (low level)
    if (gpio_get_level(GPIO_BUTTON_PIN) == 1)
    {
        // Check if the debounce delay has passed
        if (current_time - last_press_time > DEBOUNCE_DELAY_MS)
        {
            last_press_time = current_time;
            return true;
        }
    }
    return false;
}

// Função principal
void app_main(void) {
    ESP_LOGI(TAG, "OTA example app_main start");

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running_partition, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Firmware Information:");
        ESP_LOGI(TAG, "  Version: %s", running_app_info.version);
        ESP_LOGI(TAG, "  Project Name: %s", running_app_info.project_name);
        ESP_LOGI(TAG, "  Compile Time: %s", running_app_info.time);
        ESP_LOGI(TAG, "  Compile Date: %s", running_app_info.date);
        ESP_LOGI(TAG, "  IDF Version: %s", running_app_info.idf_ver);
        ESP_LOGI(TAG, "  Secure Version: %" PRIu32, running_app_info.secure_version);
        ESP_LOGI(TAG, "  Magic Word: 0x%" PRIx32, running_app_info.magic_word);
        ESP_LOGI(TAG, "  Minimal eFuse block revision: %d", running_app_info.min_efuse_blk_rev_full);
        ESP_LOGI(TAG, "  Maximal eFuse block revision: %d", running_app_info.max_efuse_blk_rev_full);
        ESP_LOGI(TAG, "  ELF SHA256: ");
        for (int i = 0; i < sizeof(running_app_info.app_elf_sha256); i++) {
            printf("%02x", running_app_info.app_elf_sha256[i]);
        }
        printf("\n");
    } else {
        ESP_LOGE(TAG, "Failed to get running firmware information");
    }

    // Initialize NVS (Non-Volatile Storage)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );


    ESP_ERROR_CHECK(esp_netif_init());  // Initialize network interface
    ESP_ERROR_CHECK(esp_event_loop_create_default());  // Create the default event loop
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    // Connect to Wi-Fi or Ethernet (based on configuration)
    ESP_ERROR_CHECK(example_connect());
    
    xTaskCreate(dht_task, "dht_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);

    // Main loop to check button press and start OTA
    while (1) {
        if (is_button_pressed()) {
            ESP_LOGI(TAG, "Starting OTA...");
            // Create OTA task when button is pressed
            xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Delay between button checks (100 ms)
    }
}


