/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "apps/sntp/sntp.h"
#include "iot_lcd.h"
#include "iot_wifi_conn.h"
#include "aws_iot_demo.h"
#include "image.h"

const char *AWSIOTTAG = "aws_iot";

/* sonda temperatura ds18b20 */

#include <inttypes.h>

// #include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
// #include "esp_log.h"

#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#include "driver/uart.h"
/**
 * This is an example which echos any data it receives on UART1 back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: UART1
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below
 */

// #define ECHO_TEST_TXD (GPIO_NUM_4)
// #define ECHO_TEST_RXD (GPIO_NUM_5)
// #define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
// #define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

// #define BUF_SIZE (1024)
#define BUF_SIZE (2048)

struct Node *headMessagesFromPIC32 = NULL;
struct Node *headMessagesFromMosquitto = NULL;

// #define GPIO_DS18B20_0       (CONFIG_ONE_WIRE_GPIO)
#define GPIO_DS18B20_0 27
#define MAX_DEVICES (8)
#define DS18B20_RESOLUTION (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD (5000) // milliseconds

OneWireBus *owb;
int num_devices = 0;
DS18B20_Info *devices[MAX_DEVICES] = {0};

void ds18b20_startup()
{
    // Override global log level
    esp_log_level_set("*", ESP_LOG_INFO);

    // To debug OWB, use 'make menuconfig' to set default Log level to DEBUG, then uncomment:
    //esp_log_level_set("owb", ESP_LOG_DEBUG);

    // Stable readings require a brief period before communication
    vTaskDelay(2000.0 / portTICK_PERIOD_MS);

    // Create a 1-Wire bus, using the RMT timeslot driver
    // OneWireBus * owb;
    owb_rmt_driver_info rmt_driver_info;
    owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(owb, true); // enable CRC check for ROM code

    // Find all connected devices
    printf("Find devices:\n");
    OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
    // int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(owb, &search_state, &found);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        printf("  %d : %s\n", num_devices, rom_code_s);
        device_rom_codes[num_devices] = search_state.rom_code;
        ++num_devices;
        owb_search_next(owb, &search_state, &found);
    }
    printf("Found %d device%s\n", num_devices, num_devices == 1 ? "" : "s");

    // In this example, if a single device is present, then the ROM code is probably
    // not very interesting, so just print it out. If there are multiple devices,
    // then it may be useful to check that a specific device is present.

    if (num_devices == 1)
    {
        // For a single device only:
        OneWireBus_ROMCode rom_code;
        owb_status status = owb_read_rom(owb, &rom_code);
        if (status == OWB_STATUS_OK)
        {
            char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
            owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
            printf("Single device %s present\n", rom_code_s);
        }
        else
        {
            printf("An error occurred reading ROM code: %d", status);
        }
    }
    // else
    // {
    //     // Search for a known ROM code (LSB first):
    //     // For example: 0x1502162ca5b2ee28
    //     OneWireBus_ROMCode known_device = {
    //         .fields.family = { 0x28 },
    //         .fields.serial_number = { 0xee, 0xb2, 0xa5, 0x2c, 0x16, 0x02 },
    //         .fields.crc = { 0x15 },
    //     };
    //     char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
    //     owb_string_from_rom_code(known_device, rom_code_s, sizeof(rom_code_s));
    //     bool is_present = false;

    //     owb_status search_status = owb_verify_rom(owb, known_device, &is_present);
    //     if (search_status == OWB_STATUS_OK)
    //     {
    //         printf("Device %s is %s\n", rom_code_s, is_present ? "present" : "not present");
    //     }
    //     else
    //     {
    //         printf("An error occurred searching for known device: %d", search_status);
    //     }
    // }

    // Create DS18B20 devices on the 1-Wire bus
    // DS18B20_Info * devices[MAX_DEVICES] = {0};
    for (int i = 0; i < num_devices; ++i)
    {
        DS18B20_Info *ds18b20_info = ds18b20_malloc(); // heap allocation
        devices[i] = ds18b20_info;

        if (num_devices == 1)
        {
            printf("Single device optimisations enabled\n");
            ds18b20_init_solo(ds18b20_info, owb); // only one device on bus
        }
        else
        {
            ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true); // enable CRC check for temperature readings
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }

    //    // Read temperatures from all sensors sequentially
    //    while (1)
    //    {
    //        printf("\nTemperature readings (degrees C):\n");
    //        for (int i = 0; i < num_devices; ++i)
    //        {
    //            float temp = ds18b20_get_temp(devices[i]);
    //            printf("  %d: %.3f\n", i, temp);
    //        }
    //        vTaskDelay(1000 / portTICK_PERIOD_MS);
    //    }
}

void ds18b20_task(void *)
{
    printf("ds18b20_task\nstarting...\n");
    // Read temperatures more efficiently by starting conversions on all devices at the same time
    int errors_count[MAX_DEVICES] = {0};
    int sample_count = 0;
    if (num_devices > 0)
    {
        TickType_t last_wake_time = xTaskGetTickCount();

        while (1)
        {
            last_wake_time = xTaskGetTickCount();

            ds18b20_convert_all(owb);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(devices[0]);

            // Read the results immediately after conversion otherwise it may fail
            // (using printf before reading may take too long)
            float readings[MAX_DEVICES] = {0};
            DS18B20_ERROR errors[MAX_DEVICES] = {DS18B20_OK};

            for (int i = 0; i < num_devices; ++i)
            {
                errors[i] = ds18b20_read_temp(devices[i], &readings[i]);
            }

            // Print results in a separate loop, after all have been read
            printf("\nTemperature readings (degrees C): sample %d\n", ++sample_count);
            for (int i = 0; i < num_devices; ++i)
            {
                if (errors[i] != DS18B20_OK)
                {
                    ++errors_count[i];
                }

                printf("  %d: %.1f    %d errors\n", i, readings[i], errors_count[i]);
            }

            vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
        }
    }

    /*     // clean up dynamically allocated data
    for (int i = 0; i < num_devices; ++i)
    {
        ds18b20_free(&devices[i]);
    }
    owb_uninitialize(owb);

    printf("Restarting now.\n");
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart(); */
}

static void app_sntp_init()
{
    time_t now = 0; // wait for time to be set
    struct tm timeinfo = {0};
    int retry = 0;
    char strftime_buf[64];
    ESP_LOGI("Time", "Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char *)"pool.ntp.org");
    sntp_init();

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < 10)
    {
        ESP_LOGI("Time", "Waiting for system time to be set... (%d/%d)\n",
                 retry, 10);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    /*Acquire new time*/
    setenv("TZ", "GMT-8", 1); // Set timezone to Shanghai time
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI("Time", "%s\n", strftime_buf);
}

void receiveMassgesFromPIC21_task(void *params)
{
    int bufferlen = 0;
    char mensagem[240];
    printf("receiveMassgesFromPIC21_task\nstarting...\n");

    int inicio = 0, indice = 0, parte = 0;
    struct Node *novo = NULL;

    int k, len;
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 230400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .use_ref_tick = true};

    uart_param_config(UART_NUM_2, &uart_config);
    // uart_set_pin(UART_NUM_2, ECHO_TEST_TXD, ECHO_TEST_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_pin(UART_NUM_2, GPIO_NUM_16, GPIO_NUM_17, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    while (1)
    {
        // Read data from the UART
        len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        if (len)
        {
            for (k = 0; k < len; k++)
            {
                if (data[k] == '\r' || data[k] == '\n')
                {
                    if (bufferlen > 0)
                    {
                        novo = (struct Node *)malloc(sizeof(struct Node));
                        novo->mensagem = (char *)malloc(bufferlen + 1);
                        strncpy(novo->mensagem, mensagem, bufferlen);
                        (novo->mensagem)[bufferlen] = '\0';
                        novo->next = headMessagesFromPIC32;
                        headMessagesFromPIC32 = novo;

                        bufferlen = 0;
                    }
                }
                else
                {
                    mensagem[bufferlen] = data[k];
                    bufferlen++;
                }
            }
        }
        // uart_write_bytes(UART_NUM_2, (const char *)data, len);
    }
}

void imprimeListaMensagens(struct Node *n)
{
    int k = 0;
    printf("---8<-------------------------------------------------------\n");
    while (n != NULL)
    {
        printf("Mensagem %d: %s\n", k, n->mensagem);
        // uart_write_bytes(UART_NUM_2, (const char *)n->mensagem, strlen(n->mensagem));
        n = n->next;
        k++;
    }
    printf("---8<-------------------------------------------------------\n");
}

struct Node *processaMensagens(struct Node *cabeca)
{
    int res = 0;
    struct Node *penultimo;
    if (cabeca == NULL)
    {
        return NULL;
    }
    else
    {
        if (cabeca->next == NULL)
        {
            res = uart_write_bytes(UART_NUM_2, (const char *)cabeca->mensagem, strlen(cabeca->mensagem));
            printf("%s %d enviada - res: %d\n", cabeca->mensagem, strlen(cabeca->mensagem), res);
            free(cabeca->mensagem);
            free(cabeca);
            return NULL;
        }
        else
        {
            penultimo = cabeca;
            while (penultimo->next->next != NULL)
                penultimo = penultimo->next;
            res = uart_write_bytes(UART_NUM_2, (const char *)penultimo->next->mensagem, strlen(penultimo->next->mensagem));
            printf("%s %d enviada - res: %d\n", penultimo->next->mensagem, strlen(penultimo->next->mensagem), res);
            free(penultimo->next->mensagem);
            free(penultimo->next);
            penultimo->next = NULL;
            return cabeca;
        }
    }
}

void sendMessages2PIC32_task(void *params)
{
    int t;
    printf("print_task\nstarting...\n");
    TickType_t last_wake_time = xTaskGetTickCount();
    while (1)
    {
        last_wake_time = xTaskGetTickCount();
        headMessagesFromMosquitto = processaMensagens(headMessagesFromMosquitto);
        // vTaskDelayUntil(&last_wake_time, 1000 / portTICK_PERIOD_MS);
    }
}

void stats_task(void *params)
{
    printf("stats_task\nstarting...\n");
    TickType_t last_wake_time = xTaskGetTickCount();
    while (1)
    {
        printf("---8<--fromMosquitto-----------------------------------------------------\n");
        imprimeListaMensagens(headMessagesFromMosquitto);
        printf("---8<--fromPIC32    -----------------------------------------------------\n");
        imprimeListaMensagens(headMessagesFromPIC32);
        vTaskDelayUntil(&last_wake_time, 5000 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main()
{
    app_lcd_init();
    app_lcd_wifi_connecting();
    CWiFi *my_wifi = CWiFi::GetInstance(WIFI_MODE_STA);
    printf("connect wifi\n");
    my_wifi->Connect(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS, portMAX_DELAY);

    app_sntp_init();
#ifdef CONFIG_MBEDTLS_DEBUG
    const size_t stack_size = 36 * 1024;
#else
    const size_t stack_size = 36 * 1024;
#endif

    ds18b20_startup();

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    // xTaskCreate(&ds18b20_task, "ds18b20", 1024 * 8, NULL, 12, NULL);

    // xTaskCreate(echo_task, "uart_echo_task", 4096, NULL, 10, NULL);
    // xTaskCreate(print_task, "print_task", 4096, NULL, 10, NULL);

    // xTaskCreatePinnedToCore(&receiveMassgesFromPIC21_task, "receiveMassgesFromPIC21_task", stack_size, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore(&sendMessages2PIC32_task, "sendMessages2PIC32_task", stack_size, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore(&stats_task, "stats_task", stack_size, NULL, 5, NULL, 1);

    xTaskCreate(receiveMassgesFromPIC21_task, "receiveMassgesFromPIC21_task", stack_size, NULL, 7, NULL);
    xTaskCreate(sendMessages2PIC32_task, "sendMessages2PIC32_task", stack_size, NULL, 5, NULL);
    xTaskCreate(stats_task, "stats_task", stack_size, NULL, 8, NULL);

    /*Start AWS task*/
    // ok
    // xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", stack_size, NULL, 5, NULL, 1);
    xTaskCreate(aws_iot_task, "aws_iot_task", stack_size, NULL, 10, NULL);
}
