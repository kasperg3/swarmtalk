#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h> // Include FreeRTOS mutex header
#include "esp_log.h"
#include <iostream>
#include <vector>
#include <stdio.h>
#include "esp_random.h"
#include "driver/usb_serial_jtag.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#define MAC2STRING(mac) mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
#define CHECK_ERROR_AND_ABORT(ret, msg)                  \
    if (ret != ESP_OK)                                   \
    {                                                    \
        ESP_LOGE(TAG, msg ": %s", esp_err_to_name(ret)); \
        abort();                                         \
    }
#define BUF_SIZE (ESP_NOW_MAX_DATA_LEN_V2)
#define TAG "ESP-COMM"
using std::exception;

// Data structure for ESP-NOW
struct Message
{
    uint8_t data[BUF_SIZE];
    size_t len;
};

QueueHandle_t espNowTransmitQueue;
QueueHandle_t serialWriteQueue;
// Remove this for final version
QueueHandle_t spiffsWriteQueue;

bool dataSent = true;
SemaphoreHandle_t dataSentMutex; // Declare a mutex handle

class ESPNowHandler
{
public:
    ESPNowHandler()
    {
        esp_err_t ret;

        // Initialize NVS
        ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        auto wifi_ifx = WIFI_IF_STA;
        // Initialize Wi-Fi in station mode
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_protocol(wifi_ifx, WIFI_PROTOCOL_LR));
        ESP_ERROR_CHECK(esp_wifi_start());

        // Initialize ESP-NOW
        CHECK_ERROR_AND_ABORT(esp_now_init(), "ESP-NOW initialization failed")

        // Register callbacks
        esp_now_register_recv_cb(onDataReceive);
        esp_now_register_send_cb(onDataSend);

        // // Add a unicast peer
        // esp_now_peer_info_t unicastPeerInfo{};
        // std::memcpy(unicastPeerInfo.peer_addr, unicastAddress, ESP_NOW_ETH_ALEN);
        // unicastPeerInfo.channel = 0;
        // unicastPeerInfo.encrypt = false;
        // unicastPeerInfo.ifidx = wifi_ifx;
        // CHECK_ERROR_AND_ABORT(esp_now_add_peer(&unicastPeerInfo), "Failed to add unicast peer");

        // Add a broadcast peer
        esp_now_peer_info_t peerInfo{};
        std::memset(peerInfo.peer_addr, 0xFF, ESP_NOW_ETH_ALEN); // Broadcast address
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        peerInfo.ifidx = wifi_ifx;
        CHECK_ERROR_AND_ABORT(esp_now_add_peer(&peerInfo), "Failed to add broadcast peer");

        // Set rate config
        esp_now_rate_config_t rate_config = {
            .phymode = WIFI_PHY_MODE_LR,     // Set to WIFI_PHY_MODE_11B for short distance
            .rate = WIFI_PHY_RATE_LORA_500K, // Set to WIFI_PHY_RATE_LORA_500K for LoRa
            .ersu = true,                    // Enable ERSU (Extended Range Single User)
            .dcm = true                      //
        };

        CHECK_ERROR_AND_ABORT(esp_now_set_peer_rate_config(peerInfo.peer_addr, &rate_config), "Failed to set peer rate config");
        CHECK_ERROR_AND_ABORT(esp_wifi_set_max_tx_power(82), "Failed to set the tx power"); // Set maximum TX power (0-82, where 82 is the highest)

        // Initialize the mutex
        dataSentMutex = xSemaphoreCreateMutex();
        if (dataSentMutex == nullptr)
        {
            ESP_LOGE(TAG, "Failed to create mutex");
            abort();
        }
    }

    static void onDataReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
    {
        ESP_LOGI(TAG, "Data received from %02x:%02x:%02x:%02x:%02x:%02x, Length: %d",
                 MAC2STRING(info->src_addr), len);
        ESP_LOGI(TAG, "RSSI: %d dBm", info->rx_ctrl->rssi);
        if (len > BUF_SIZE)
        {
            ESP_LOGE(TAG, "Received data exceeds buffer size");
            return;
        }

        // Remove for the final version
        int rssi = info->rx_ctrl->rssi;
        char formatted_data[128];
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        int64_t time_since_start = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
        snprintf(formatted_data, sizeof(formatted_data), "%lld,%d,%zu,%02x:%02x:%02x:%02x:%02x:%02x,%02x:%02x:%02x:%02x:%02x:%02x\n",
             time_since_start, rssi, len,  MAC2STRING(info->src_addr),MAC2STRING(mac));
        if (xQueueSend(spiffsWriteQueue, &formatted_data, pdMS_TO_TICKS(1000)) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to send extended message to ESP-NOW queue");
        }

        // TODO Enable this again for the final version
        // Message message;
        // std::memcpy(message.data, data, len);
        // message.len = len;
        // ESP_LOGI(TAG, "Data received from MAC: %02x:%02x:%02x:%02x:%02x:%02x", MAC2STRING(info->src_addr));
        // if (xQueueSend(serialWriteQueue, &message, portMAX_DELAY) != pdPASS)
        // {
        //     ESP_LOGE(TAG, "Failed to send data to UART queue");
        // }
    }

    static void onDataSend(const uint8_t *mac_addr, esp_now_send_status_t status)
    {
        ESP_LOGI(TAG, "Data sent via ESP-NOW to %02x:%02x:%02x:%02x:%02x:%02x, Status: %s",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5],
                 status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");

        xSemaphoreTake(dataSentMutex, portMAX_DELAY);
        dataSent = true;
        xSemaphoreGive(dataSentMutex);
    }

    // No logging inside this function!!!!
    void sendData(const uint8_t *peer_addr, Message message)
    {
        if (message.len > ESP_NOW_MAX_DATA_LEN_V2)
        {
            // ESP_LOGE(TAG, "Data size exceeds ESP-NOW maximum limit");
        }
        else
        {
            while (true)
            {
                xSemaphoreTake(dataSentMutex, portMAX_DELAY);
                if (dataSent)
                {
                    dataSent = false;
                    xSemaphoreGive(dataSentMutex);
                    break;
                }
                xSemaphoreGive(dataSentMutex);
                vTaskDelay(pdMS_TO_TICKS(10)); // Wait for the previous message to be sent

                // ESP_LOGW(TAG, "Waiting for previous message to be sent");
            }
            
            auto result = esp_now_send(peer_addr, message.data, message.len);
            if (result != ESP_OK)
            {
                // ESP_LOGE(TAG, "Error sending data: %s", esp_err_to_name(result));
                xSemaphoreTake(dataSentMutex, portMAX_DELAY);
                dataSent = true; // Reset the flag if sending failed
                xSemaphoreGive(dataSentMutex);
            }
        }
    }

    ~ESPNowHandler()
    {
        esp_now_deinit();
        esp_wifi_stop();
        esp_wifi_deinit();
        vSemaphoreDelete(dataSentMutex); // Delete the mutex
    }
};

// The rest of the code remains unchanged

class SerialHandler
{
private:
    const uint8_t START_DELIMITER = 0x02;
    const uint8_t END_DELIMITER = 0x03;
    const uint8_t ESCAPE_CHAR = 0x1B;

public:
    SerialHandler()
    {
        init();
    }


    int read(uint8_t *buffer, size_t buffer_size)
    {
        size_t total_len = 0;
        while (total_len < buffer_size)
        {
            // Read each byte
            uint8_t byte;
            int len = usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(10));
            // Check for the start delimiter and start reading the data
            if (len > 0)
            {
                if (byte == START_DELIMITER)
                {
                    total_len = 0; // Reset buffer if start delimiter is found
                    std::memset(buffer, 0, buffer_size);
                }
                else if (byte == END_DELIMITER)
                {
                    break; // End delimiter found, exit the loop
                }
                else if (byte == ESCAPE_CHAR)
                {
                    // Read the next byte after escape character
                    len = usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(10));
                    if (len > 0)
                    {
                        buffer[total_len++] = byte;
                    }
                }
                else
                {
                    buffer[total_len++] = byte;
                }
            }
        }
        return total_len;
    }

    void write(uint8_t *serialMessage, size_t len)
    {
        // Combine start sequence, escaped message, and stop sequence into a single buffer
        std::vector<uint8_t> combinedMessage;
        combinedMessage.push_back(START_DELIMITER);

        // Escape the message
        for (size_t i = 0; i < len; ++i)
        {
            if (serialMessage[i] == START_DELIMITER || serialMessage[i] == END_DELIMITER || serialMessage[i] == ESCAPE_CHAR)
            {
                combinedMessage.push_back(ESCAPE_CHAR);
            }
            combinedMessage.push_back(serialMessage[i]);
        }

        combinedMessage.push_back(END_DELIMITER);
        // Write the combined message in one go
        usb_serial_jtag_write_bytes(combinedMessage.data(), combinedMessage.size(), pdMS_TO_TICKS(10));
        // This blocks if there are no listener on the serial
        // usb_serial_jtag_wait_tx_done(portMAX_DELAY);
        usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(100));

    }

    ~SerialHandler()
    {
    }

    void init()
    {
        usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        usb_serial_jtag_config.tx_buffer_size = BUF_SIZE*2;
        usb_serial_jtag_config.rx_buffer_size = BUF_SIZE*4;

        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    }
};

void espNowSendTask(void *pvParameters)
{
    uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast
    ESPNowHandler *handler = static_cast<ESPNowHandler *>(pvParameters);
    Message message;
    while (xQueueReceive(espNowTransmitQueue, &message, portMAX_DELAY) == pdPASS)
    {
            handler->sendData(broadcastAddress, message);
    }
}

void serialReadTask(void *pvParameters)
{
    SerialHandler *handler = static_cast<SerialHandler *>(pvParameters);
    Message message;
    while (1)
    {
        int len = handler->read(message.data, BUF_SIZE);

        if (len > 0)
        {
            message.len = len;
            ESP_LOGI(TAG, "Serial Data received: %d Bytes", message.len);
            if (xQueueSend(espNowTransmitQueue, &message, pdMS_TO_TICKS(1000)) != pdPASS)
            {
                ESP_LOGE(TAG, "Failed to send data to ESP-NOW queue");
            }
        }
    }
}

// Serial write task
void serialWriteTask(void *pvParameters)
{
    SerialHandler *handler = static_cast<SerialHandler *>(pvParameters);
    Message message;
    std::memset(&message, 0, sizeof(Message));
    while (xQueueReceive(serialWriteQueue, &message, portMAX_DELAY) == pdPASS)
    {
        ESP_LOGI(TAG, "Writing %d Bytes to serial", message.len);
        handler->write(message.data, message.len);
    }
}

#define ESP_NOW_QUEUE_SIZE 50
#define SERIAL_QUEUE_SIZE 50

class SPIFFSHandler
{
private:
    static const size_t BUFFER_SIZE = 1024;
    uint8_t data_buffer[BUFFER_SIZE];
    size_t buffer_index = 0;

public:
    SPIFFSHandler()
    {
        init_spiffs();
    }

    ~SPIFFSHandler()
    {
        // Unmount SPIFFS
        esp_vfs_spiffs_unregister(NULL);
        ESP_LOGI(TAG, "SPIFFS unmounted");
    }

    // Function to initialize and mount SPIFFS
    void init_spiffs(void) {
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true
        };

        esp_err_t ret = esp_vfs_spiffs_register(&conf);

        if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                ESP_LOGE(TAG, "Failed to mount or format filesystem");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Failed to find SPIFFS partition");
            } else {
                ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            }
            return;
        }

        size_t total = 0, used = 0;
        ret = esp_spiffs_info(NULL, &total, &used);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
        }
    }

    // Function to append data to SPIFFS
    void append_data_to_spiffs(const uint8_t *data, size_t len) {
        FILE *f = fopen("/spiffs/data.txt", "a+");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for appending");
            return;
        }
        
        fputs(reinterpret_cast<const char*>(data), f);
        fclose(f);
    }

    // Function to read data from SPIFFS and output to serial
    void read_spiffs_to_serial(void) {
        FILE *f = fopen("/spiffs/data.txt", "r");
        if (f == NULL) {
            ESP_LOGI(TAG, "No data file found in SPIFFS");
            return;
        }
        char line[128];
        while (fgets(line, sizeof(line), f) != NULL) {
            printf("%s", line);
            vTaskDelay( 1 / portTICK_PERIOD_MS );
        }
        fclose(f);
        ESP_LOGI(TAG, "Data output to serial");
    }

    void remove_spiffs_file(void) {
        if (remove("/spiffs/data.txt") != 0) {
            ESP_LOGE(TAG, "Failed to remove file");
        } else {
            ESP_LOGI(TAG, "File removed");
        }
    }
};


void send_random_message_task(void *pvParameters)
{
    Message message;
    while (true)
    {
        // Generate a random message with 200 bytes
        size_t random_len = 200;
        random_len++; // Ensure random_len is at least 1
        if (random_len >= sizeof(message.data)) {
            random_len = sizeof(message.data) - 1; // Limit to buffer size
        }

        for (size_t i = 0; i < random_len; ++i)
        {
            message.data[i] = static_cast<uint8_t>(esp_random() % 256); // Fill with random data
        }

        message.len = random_len;

        // Add the message to the queue
        if (xQueueSend(espNowTransmitQueue, &message, pdMS_TO_TICKS(1000)) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to send message to ESP-NOW queue");
        }
        // Prepare to log data to spiffs
        int64_t time_since_start = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast
        char formatted_data[128];
        snprintf(formatted_data, sizeof(formatted_data), "%lld,%d,%zu,%02x:%02x:%02x:%02x:%02x:%02x,%02x:%02x:%02x:%02x:%02x:%02x\n",
             time_since_start, 0, message.len,MAC2STRING(mac),MAC2STRING(broadcastAddress));
        // send the data to the Queue
        if (xQueueSend(spiffsWriteQueue, &formatted_data, pdMS_TO_TICKS(1000)) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to send data to SPIFFS queue");
        }

        // Delay for a while before sending the next message
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay for 0.5 seconds
    }
}
void spiffsWriteTask(void *pvParameters){
    SPIFFSHandler *handler = static_cast<SPIFFSHandler *>(pvParameters);
    char data[128];
    std::vector<std::string> batch;
    const size_t batchSize = 20;

    while (xQueueReceive(spiffsWriteQueue, &data, portMAX_DELAY) == pdPASS)
    {
        batch.push_back(std::string(data));
        if (batch.size() >= batchSize)
        {
            for (const auto& msg : batch)
            {
                handler->append_data_to_spiffs(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
            }
            batch.clear();
        }
    }

    // Write any remaining messages in the batch
    for (const auto& msg : batch)
    {
        handler->append_data_to_spiffs(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
    }
}

void buttonPressedTask(void *pvParameters)
{
    SPIFFSHandler *handler = static_cast<SPIFFSHandler *>(pvParameters);
    gpio_num_t button_gpio = GPIO_NUM_0; // Assuming the integrated button is connected to GPIO0

    // Configure the button GPIO
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Trigger on falling edge
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << button_gpio);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    while (true)
    {
        // Wait for the button press
        if (gpio_get_level(button_gpio) == 0)
        {
            // Debounce the button press
            vTaskDelay(pdMS_TO_TICKS(50));
            if (gpio_get_level(button_gpio) == 0)
            {
                ESP_LOGI(TAG, "Button pressed, deleting SPIFFS content");
                handler->remove_spiffs_file();

                // Wait for the button to be released
                while (gpio_get_level(button_gpio) == 0)
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Polling delay
    }
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Starting ESP Communication");
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    // Initialize queues
    espNowTransmitQueue = xQueueCreate(ESP_NOW_QUEUE_SIZE, sizeof(Message));
    serialWriteQueue = xQueueCreate(SERIAL_QUEUE_SIZE, sizeof(Message));
    spiffsWriteQueue = xQueueCreate(SERIAL_QUEUE_SIZE, sizeof(char)*128);

    if (!espNowTransmitQueue || !serialWriteQueue)
    {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }
    
    static SPIFFSHandler spiffsHandler;
    static ESPNowHandler espNowHandler;
    static SerialHandler serialHandler;
    
     // Initialize SPIFFS
    spiffsHandler.append_data_to_spiffs((const uint8_t*)"Device rebooted\n", 14); // Append data to SPIFFS
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
    // Read and output data from SPIFFS to serial

    spiffsHandler.read_spiffs_to_serial();

    xTaskCreate(send_random_message_task, "send_data", 8192, nullptr, 1, nullptr);
    xTaskCreate(spiffsWriteTask, "spiffsWriteQueue", 8192, &spiffsHandler, 1, nullptr);
    xTaskCreate(buttonPressedTask, "buttonTask", 8192, &spiffsHandler, 1, nullptr);

    xTaskCreate(espNowSendTask, "espNowSendTask", 8192, &espNowHandler, 1, nullptr);

    // xTaskCreate(serialReadTask, "serialReadTask", 8192, &serialHandler, 1, nullptr);
    // xTaskCreate(serialWriteTask, "serialWriteTask", 8192, &serialHandler, 1, nullptr);
}
