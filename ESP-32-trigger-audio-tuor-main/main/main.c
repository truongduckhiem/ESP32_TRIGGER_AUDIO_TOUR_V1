#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"

//COMPONENTS
#include "nmea_parser.h"
#include "dfplayer.h"

//WIFI
#define WIFI_SSID      "TOUR_GUIDE_CONFIG"
#define WIFI_PASS      "12345678"

//CẤU HÌNH PHẦN CỨNG
#define GPS_UART_RX_PIN 16
#define GPS_UART_PORT   UART_NUM_2

//CẤU HÌNH LOGIC GPS
#define MAX_POINTS 50       
#define RADIUS_ENTER 10.0   // Vào 10m -> Bật
#define RADIUS_EXIT  15.0   // Ra 15m -> Tắt (Hysteresis)

// Cấu trúc của một toạ độ
typedef struct {
    double lat;
    double lon;
    int track_id;
    char name[32]; 
} poi_t;
static char current_name[32] = "";
static poi_t points[MAX_POINTS];
static int point_count = 0;
static const char *TAG = "GPS_TOUR";
static gps_time_t current_time;
static gps_date_t current_date;
//1. FILE VÀ CONFIG
esp_err_t init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    return esp_vfs_spiffs_register(&conf);
}

// Đọc file config.txt
void load_config() {
    FILE *f = fopen("/spiffs/config.txt", "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Chua co file config! Tao file mau...");
        f = fopen("/spiffs/config.txt", "w");
        fprintf(f, "21.005037620872148,105.8453652430323,1,Cong Tran Dai Nghia\n");
        fprintf(f, "21.004430824594806,105.84421471858742,2,Thu Vien TQB\n");
        fprintf(f, "21.006409872632364, 105.8423703905079,3,Hoi truong C2\n");
        fprintf(f, "21.00556751075386, 105.84519167303262,4,Toa nha C7\n");
        fprintf(f, "20.993472647051668,105.86411242339778,3,Test \n");
        fclose(f);
        return; 
    }

    point_count = 0;
    char line[128];
    while (fgets(line, sizeof(line), f) && point_count < MAX_POINTS) {
        poi_t *p = &points[point_count];
        char *token = strtok(line, ","); if(!token) continue; p->lat = atof(token);
        token = strtok(NULL, ","); if(!token) continue; p->lon = atof(token);
        token = strtok(NULL, ","); if(!token) continue; p->track_id = atoi(token);
       memset(p->name, 0, sizeof(p->name));  // ✅ Reset name

token = strtok(NULL, "\n"); 
if(token) {
    strncpy(p->name, token, 31);
    p->name[31] = '\0';  // ✅ Đảm bảo null-terminated
}

        ESP_LOGI(TAG, "Loaded #%d: %s (Track %d)", point_count, p->name, p->track_id);
        point_count++;
    }
    fclose(f);
    ESP_LOGI(TAG, "Tong so diem: %d", point_count);
}

//2. WEB
static esp_err_t config_get_handler(httpd_req_t *req) {
    // 1. Gửi Header + Script 
    httpd_resp_sendstr_chunk(req, "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
                                  "<style>body{font-family:sans-serif;padding:20px} textarea{width:100%;height:300px;font-family:monospace;font-size:14px;padding:10px} .btn{background:#28a745;color:white;padding:15px;border:none;width:100%;font-size:18px;cursor:pointer;margin-top:10px} .btn-danger{background:#dc3545;margin-top:5px;padding:10px;font-size:14px}</style>"
                                  "<script>"
                                  "function saveData() {"
                                  "  var txt = document.getElementById('cfg').value;"
                                  "  fetch('/save', {method: 'POST', body: txt})"
                                  "  .then(response => {"
                                  "     if(response.ok) { alert('DA LUU THANH CONG!'); window.location.href='/'; }"
                                  "     else { alert('Loi khi luu!'); }"
                                  "  });"
                                  "}"
                                  "function resetLog(){if(confirm('Ban chac chan muon xoa log?')){fetch('/reset_log',{method:'POST'}).then(r=>r.ok?alert('DA XOA LOG!'):alert('Loi!'));}}"
                                  "</script>"
                                  "</head>");
                                  
    httpd_resp_sendstr_chunk(req, "<body><h2>CAU HINH DIEM DEN</h2>");
    
    // 2. Ô nhập liệu 
    httpd_resp_sendstr_chunk(req, "<textarea id='cfg'>");
    
    // Đọc file config hiện tại vào
    FILE *f = fopen("/spiffs/config.txt", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) httpd_resp_sendstr_chunk(req, line);
        fclose(f);
    }
    
    // 3. Nút bấm gọi hàm Javascript
    httpd_resp_sendstr_chunk(req, "</textarea><br>");
    httpd_resp_sendstr_chunk(req, "<button onclick='saveData()' class='btn'>LUU CAU HINH (FIXED)</button>");
    httpd_resp_sendstr_chunk(req, "<button onclick='resetLog()' class='btn btn-danger'>XOA LOG CU</button>");
    httpd_resp_sendstr_chunk(req, "<br><br><a href='/log'>&laquo; Quay ve xem Log</a></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
 
static esp_err_t config_save_handler(httpd_req_t *req) {
    char buf[512];
    int ret, remaining = req->content_len;
 
    FILE *f = fopen("/spiffs/config.txt", "w"); 
    if (!f) { httpd_resp_send_500(req); return ESP_FAIL; }
 
    while (remaining > 0) {
        int to_read = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        if ((ret = httpd_req_recv(req, buf, to_read)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            fclose(f); return ESP_FAIL;
        }
        fwrite(buf, 1, ret, f);
        remaining -= ret;
    }
    fclose(f);
    load_config(); 
    httpd_resp_sendstr_chunk(req, "<h1>DA LUU THANH CONG!</h1><a href='/log'>Xem Nhat Ky Ngay</a>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
 
// Trang xem Log
static esp_err_t log_get_handler(httpd_req_t *req) {
    httpd_resp_sendstr_chunk(req, "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
                                  "<style>body{font-family:sans-serif;padding:20px} .btn-reset{background:#dc3545;color:white;padding:12px;border:none;width:100%;font-size:16px;cursor:pointer;margin-top:10px}</style>"
                                  "<script>function resetLog(){if(confirm('Ban chac chan muon xoa log?')){fetch('/reset_log',{method:'POST'}).then(r=>r.ok?window.location.href='/log':alert('Loi!'));}}</script>"
                                  "</head><body><h2>NHAT KY</h2><pre>");
    FILE *f = fopen("/spiffs/log.txt", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) httpd_resp_sendstr_chunk(req, line);
        fclose(f);
    }
    httpd_resp_sendstr_chunk(req, "</pre><br>");
    httpd_resp_sendstr_chunk(req, "<button onclick='resetLog()' class='btn-reset'>XOA LOG CU</button>");
    httpd_resp_sendstr_chunk(req, "<br><br><a href='/'>Ve trang cau hinh</a></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
 static esp_err_t reset_log_handler(httpd_req_t *req)
{
    // Xóa file log cũ
    remove("/spiffs/log.txt");

    // Tạo file log mới rỗng
    FILE *f = fopen("/spiffs/log.txt", "w");
    if (f) {
        fclose(f);
    }

    ESP_LOGI(TAG, "Da xoa log cu");

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}
void start_webserver() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = config_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get);
        httpd_uri_t uri_post = { .uri = "/save", .method = HTTP_POST, .handler = config_save_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_post);
        httpd_uri_t uri_log = { .uri = "/log", .method = HTTP_GET, .handler = log_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_log);
    }
    httpd_uri_t uri_reset = {
    .uri = "/reset_log",
    .method = HTTP_POST,
    .handler = reset_log_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(server, &uri_reset);
}

//3. WIFI
void wifi_init_softap() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

//4. GPS LOGIC
// Hàm ghi log vào file
#define LOG_FILE_MAX_SIZE 10000  // ~10KB

void write_log(const char *msg)
{
    FILE *f = fopen("/spiffs/log.txt", "r");
    int lines = 0;

    if (f) {
        char buf[64];
        while (fgets(buf, sizeof(buf), f)) lines++;
        fclose(f);
    }

    if (lines > 100) {
        remove("/spiffs/log.txt");
    }

    f = fopen("/spiffs/log.txt", "a");
    if (f) {

        fprintf(f,
                "[%02d/%02d/%04d %02d:%02d:%02d] %s\n",
                current_date.day,
                current_date.month,
                current_date.year,
                current_time.hour + 7, // UTC -> VN
                current_time.minute,
                current_time.second,
                msg);

        fclose(f);
    }
}
    

double haversine_distance(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0);
    return 6371000.0 * 2 * atan2(sqrt(a), sqrt(1 - a));
}

static void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    gps_t *gps = NULL;
    static int current_track = 0; // 0 = Không phát gì
    
    switch (event_id) {
    case GPS_UPDATE:
        gps = (gps_t *)event_data;
        current_time = gps->tim;
        current_date = gps->date;
        int found_track = 0; 
        double min_dist = 99999.0;
        char *loc_name = "---";

        for (int i = 0; i < point_count; i++) {
            double dist = haversine_distance(points[i].lat, points[i].lon, gps->latitude, gps->longitude);
            if (dist < min_dist) min_dist = dist;
            
            // Hysteresis Logic
            double threshold = (current_track == points[i].track_id) ? RADIUS_EXIT : RADIUS_ENTER;

            if (dist <= threshold) {
                found_track = points[i].track_id;
                loc_name = points[i].name;
                break; 
            }
        }
        
        // Log ra Serial Monitor
        ESP_LOGI(TAG, "Lat:%.5f Lon:%.5f | MinDist:%.1fm | Track:%d", 
                 gps->latitude, gps->longitude, min_dist, current_track);
        
        // Xử lý Audio
        if (found_track != current_track) {
            char log_msg[64];
            if (found_track != 0) {
                ESP_LOGI(TAG, "--> VAO VUNG: %s (Bai %d)", loc_name, found_track);
                dfplayer_play_track(found_track);
                
                // Ghi vào Log Web
                strcpy(current_name, loc_name);

        snprintf(log_msg, sizeof(log_msg),
         "VAO: %s (%.5f, %.5f)",
         current_name,
         gps->latitude,
         gps->longitude);
        write_log(log_msg);
            } else {
                ESP_LOGI(TAG, "--> RA KHOI VUNG");
                snprintf(log_msg, sizeof(log_msg),
                "RA: %s (%.5f, %.5f)",
                current_name,
                gps->latitude,
                gps->longitude);

                write_log(log_msg);

                current_name[0] = '\0';
                dfplayer_pause();
            }
            current_track = found_track;
        }
        break;
        
    case GPS_UNKNOWN:
        break;
    }
}

void app_main(void)
{
    // 1. Khởi tạo NVS & SPIFFS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }
    
    if (init_spiffs() == ESP_OK) {
        load_config(); // Load tọa độ
    } else {
        ESP_LOGE(TAG, "Loi SPIFFS!");
    }

    // 2. Khởi tạo Wifi Web
    wifi_init_softap();
    start_webserver();

    // 3. Khởi tạo Audio
    dfplayer_init();
    // 4. Khởi tạo GPS
    nmea_parser_config_t config = {
        .uart = { .uart_port = UART_NUM_2, .rx_pin = GPS_UART_RX_PIN, .baud_rate = 9600, 
                  .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, 
                  .stop_bits = UART_STOP_BITS_1, .event_queue_size = 16 }
    };
    nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config);
    nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
}