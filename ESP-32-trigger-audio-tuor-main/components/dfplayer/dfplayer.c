/* File: dfplayer.c (Không cần sửa) */
#include "dfplayer.h"
#include "string.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // Cần cho vTaskDelay
#include "freertos/task.h"

static const char *TAG = "dfplayer";

// Cấu trúc gói tin lệnh của DFPlayer
#define DFPLAYER_START_BYTE 0x7E
#define DFPLAYER_END_BYTE   0xEF
#define DFPLAYER_VERSION    0xFF
#define DFPLAYER_CMD_LEN    0x06 // 6 byte không tính start/end

static uint8_t cmd_buffer[10]; // 7E FF 06 CMD FEEDBACK D1 D2 SUMH SUML EF

/**
 * @brief Gửi một gói tin lệnh đến DFPlayer
 */
static void dfplayer_send_cmd(uint8_t cmd, uint8_t param1, uint8_t param2) {
    // ✅ Tính checksum
    uint16_t checksum = -(DFPLAYER_VERSION + DFPLAYER_CMD_LEN + cmd + 0x01 + param1 + param2);
    
    // ✅ Build cmd_buffer
    cmd_buffer[0] = DFPLAYER_START_BYTE;
    cmd_buffer[1] = DFPLAYER_VERSION;
    cmd_buffer[2] = DFPLAYER_CMD_LEN;
    cmd_buffer[3] = cmd;
    cmd_buffer[4] = 0x01;  // Feedback (0x00 = không cần feedback)
    cmd_buffer[5] = param1;
    cmd_buffer[6] = param2;
    cmd_buffer[7] = (uint8_t)(checksum >> 8);
    cmd_buffer[8] = (uint8_t)(checksum & 0xFF);
    cmd_buffer[9] = DFPLAYER_END_BYTE;
    
    // Gửi lệnh
    uart_write_bytes(DFPLAYER_UART_PORT, (const char *)cmd_buffer, 10);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // Đọc feedback
    uint8_t response[10];
    int len = uart_read_bytes(DFPLAYER_UART_PORT, response, 10, pdMS_TO_TICKS(500));
    
    if (len > 0 && response[0] == 0x7E && response[9] == 0xEF) {
        ESP_LOGI(TAG, "DF feedback OK: cmd=0x%02x, status=0x%02x", 
                 cmd, response[3]);
    } else {
        ESP_LOGW(TAG, "DF feedback FAIL hoac timeout (len=%d)", len);
    }
}

// === CÁC HÀM PUBLIC ===

void dfplayer_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = DFPLAYER_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Cài đặt driver UART
    ESP_ERROR_CHECK(uart_driver_install(DFPLAYER_UART_PORT, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(DFPLAYER_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(DFPLAYER_UART_PORT, DFPLAYER_TX_PIN, DFPLAYER_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "DFPlayer UART da khoi tao (Port %d, TX:%d, RX:%d)", 
             DFPLAYER_UART_PORT, DFPLAYER_TX_PIN, DFPLAYER_RX_PIN);
             
    // Đợi DFPlayer khởi động
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    dfplayer_set_volume(25); // Đặt âm lượng mặc định (0-30)
    ESP_LOGI(TAG, "Da dat am luong DFPlayer.");
}

void dfplayer_play_track(uint16_t track_number)
{
    dfplayer_send_cmd(0x03, (uint8_t)(track_number >> 8), (uint8_t)(track_number));
}

void dfplayer_set_volume(uint8_t volume)
{
    if (volume > 30) {
        volume = 30;
    }
    // Lệnh 0x06: Đặt âm lượng
    dfplayer_send_cmd(0x06, 0x00, volume);
}

void dfplayer_pause(void)
{
    // Lệnh 0x0E: Tạm dừng
    dfplayer_send_cmd(0x0E, 0x00, 0x00);
}

/**
 * @brief Tiếp tục phát nhạc (sau khi tạm dừng)
 */
void dfplayer_resume(void)
{
    // Lệnh 0x0D: Tiếp tục
    dfplayer_send_cmd(0x0D, 0x00, 0x00);
}