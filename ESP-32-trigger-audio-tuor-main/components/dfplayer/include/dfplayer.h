/* File: dfplayer.h (Sửa chân TX để tránh xung đột) */
#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"

// --- Cấu hình DFPlayer (ĐÃ SỬA) ---
#define DFPLAYER_UART_PORT   (UART_NUM_1)
#define DFPLAYER_TX_PIN      (GPIO_NUM_19) // <-- ĐÃ ĐỔI TỪ 17 SANG 19
#define DFPLAYER_RX_PIN      (GPIO_NUM_18) // <-- Giữ nguyên (hoặc (UART_PIN_NO_CHANGE) nếu không nghe)
#define DFPLAYER_BAUD_RATE   9600
// --- Hết cấu hình ---

/**
 * @brief Khởi tạo UART cho DFPlayer
 */
void dfplayer_init(void);

/**
 * @brief Phát một bài hát cụ thể trong thư mục /mp3
 * @param track_number Số thứ tự bài hát (ví dụ: 1 cho file 0001.mp3)
 */
void dfplayer_play_track(uint16_t track_number);

/**
 * @brief Đặt âm lượng
 * @param volume Âm lượng (từ 0 đến 30)
 */
void dfplayer_set_volume(uint8_t volume);
void dfplayer_pause(void);

/**
 * @brief Tiếp tục phát nhạc (sau khi tạm dừng)
 */
void dfplayer_resume(void);