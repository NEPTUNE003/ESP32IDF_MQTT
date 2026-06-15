#include <inttypes.h>

#include "onenet_demo.h"
#include "als_service.h"
#include "imu_service.h"
#include "led_task.h"
#include "menu.h"
#include "lcd_task.h"

static const char *TAG = "MQTT_EXAMPLE";

/* WiFi 事件组 */
static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;

/* MQTT 客户端句柄 */
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/* ======================== WiFi 连接 ======================== */

/**
 * @brief WiFi 事件处理
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "WiFi retry %d/%d", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化并连接 WiFi
 */
static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi connecting to SSID: %s", WIFI_SSID);

    /* 等待 WiFi 连接成功或失败 */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed");
    }
}

/* ======================== MQTT 相关 ======================== */

/**
 * @brief       错误日志
 * @param       message     :错误消息
 * @param       error_code  :错误码
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * @brief 从 JSON 字符串中提取 "KEY":数值 中的整数值
 * @param json JSON字符串，例如 {"SCREEN":1}
 * @param key  要查找的key名，例如 "SCREEN"
 * @return 找到则返回数值，否则返回 -1
 */
static int extract_value_int(const char *json, const char *key)
{
    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "\"%s\":", key);
    const char *p = strstr(json, key_buf);
    if (!p) return -1;
    p += strlen(key_buf);
    // 跳过空白
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    // 处理负号
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    int val = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return val * sign;
}

/**
 * @brief       上报设备属性
 * @param       client      :MQTT客户端句柄
 * @param       params      :JSON格式的属性参数
 */
void report_device_property(esp_mqtt_client_handle_t client, const char *params)
{
    static uint32_t s_msg_seq = 0;
    s_msg_seq++;

    char message_id[12];
    snprintf(message_id, sizeof(message_id), "%" PRIu32, s_msg_seq);

    char onenet_msg[400];
    snprintf(onenet_msg, sizeof(onenet_msg), ONE_NET_BODY_FORMAT, message_id, params);

    int msg_id = esp_mqtt_client_publish(
        client,
        TOPIC_PROP_POST,
        onenet_msg,
        strlen(onenet_msg),
        1,  /* QoS 1 */
        0   /* Retain */
    );

    ESP_LOGI(TAG, "Report property: %s (msg_id=%d)", onenet_msg, msg_id);
}

/**
 * @brief 动态上报任务 - 定期上传感测数据
 */
static void dynamic_report_task(void *pvParameters)
{
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvParameters;

    while (1)
    {
        /* 采集各传感器数据 */
        uint16_t als_val = als_get_value();
        float pitch = imu_service_get_pitch();
        float roll = imu_service_get_roll();
        led_mode_t current_led_mode = led_mode_get();

        /* 构造动态JSON，上报所有感测数据 */
        char params[512];
        snprintf(params, sizeof(params),
            "{"
            "\"ALS\":{\"value\":%d},"
            "\"LED_MODE\":{\"value\":%d},"
            "\"PITCH\":{\"value\":%.1f},"
            "\"ROLL\":{\"value\":%.1f}"
            "}",
            als_val,
            (int)current_led_mode,
            pitch,
            roll);

        /* 调用上报函数 */
        report_device_property(client, params);

        /* 每5秒上报一次 */
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/**
 * @brief 处理 OneNET 属性设置请求（thing.service.property.set）
 * @param client MQTT客户端句柄
 * @param data_buf 收到的 JSON payload
 */
static void handle_property_set(esp_mqtt_client_handle_t client, const char *data_buf)
{
    ESP_LOGI(TAG, "Received property set request: %s", data_buf);

    /* 提取请求中的 id，回复时原样返回 */
    char req_id[64] = "1";
    const char *id_start = strstr(data_buf, "\"id\":\"");
    if (id_start) {
        id_start += 6; // 跳过 "id":"
        const char *id_end = strchr(id_start, '\"');
        if (id_end) {
            size_t len = id_end - id_start;
            if (len < sizeof(req_id)) {
                memcpy(req_id, id_start, len);
                req_id[len] = '\0';
            }
        }
    }

    /* ===== ★ 先发回复，防止 OneNET 超时 ===== */
    char reply[512];
    snprintf(reply, sizeof(reply),
        "{\"id\":\"%s\",\"version\":\"1.0\",\"code\":200,\"method\":\"thing.service.property.set\",\"data\":{}}",
        req_id);
    esp_mqtt_client_publish(client, TOPIC_PROP_SET_REPLY, reply, strlen(reply), 1, 0);
    ESP_LOGI(TAG, "Sent reply first: %s", reply);

    /* ===== 再执行操作（这些可能较慢，但这时 OneNET 已收到回复） ===== */

    /* LED_MODE 控制（非阻塞，通过队列发给 menu_task 执行 LCD 操作） */
    int led_val = extract_value_int(data_buf, "LED_MODE");
    if (led_val >= 0 && led_val < 5) {
        menu_set_led((led_mode_t)led_val);
        ESP_LOGI(TAG, "Cloud set LED to %d (queued)", led_val);
    }

    /* SCREEN 切换（非阻塞，通过队列发给 menu_task 执行 LCD 操作） */
    int screen_val = extract_value_int(data_buf, "SCREEN");
    if (screen_val >= 0 && screen_val <= 4) {
        menu_switch_screen((screen_type_t)screen_val);
        ESP_LOGI(TAG, "Cloud switch to screen %d (queued)", screen_val);
    }
}

/**
 * @brief MQTT事件处理函数
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:  /* MQTT连接成功 */
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        /* 连接成功后创建定时上报任务 */
        xTaskCreatePinnedToCore(
            dynamic_report_task,    /* 任务函数 */
            "dynamic_report",       /* 任务名 */
            4096,                   /* 栈大小 */
            client,                 /* 传递client句柄 */
            5,                      /* 优先级 */
            NULL,                   /* 任务句柄 */
            1                       /* 核心1 */
        );

        /* 订阅OneNET属性设置主题 */
        esp_mqtt_client_subscribe(client, TOPIC_PROP_SET, 0);
        ESP_LOGI(TAG, "Subscribed to property set topic");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        /* 处理属性设置请求 */
        if (strncmp(event->topic, TOPIC_PROP_SET, event->topic_len) == 0)
        {
            char data_buf[256];
            snprintf(data_buf, sizeof(data_buf), "%.*s", event->data_len, event->data);
            handle_property_set(client, data_buf);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle && event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * @brief MQTT 启动任务（在独立任务中运行，不阻塞 app_main）
 */
static void mqtt_start_task(void *pvParameters)
{
    /* 先连接 WiFi（会阻塞直到连接成功或失败） */
    wifi_init_sta();

    /* 配置MQTT参数 */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .broker.address.hostname = MQTT_HOST,
        .broker.address.port = MQTT_PORT,
        .credentials.client_id = DEVICE_ID,
        .credentials.username = PRODUCT_ID,
        .credentials.authentication.password = DEVICE_TOKEN,
    };

    /* 设置日志等级（INFO 级别足够，VERBOSE 会严重拖慢系统） */
    esp_log_level_set("mqtt_client", ESP_LOG_INFO);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_INFO);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_INFO);
    esp_log_level_set("TRANSPORT", ESP_LOG_INFO);
    esp_log_level_set("outbox", ESP_LOG_INFO);

    /* 创建MQTT客户端实例 */
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* 注册MQTT事件处理函数 */
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, s_mqtt_client);
    /* 启动MQTT连接 */
    esp_mqtt_client_start(s_mqtt_client);

    vTaskDelete(NULL);
}

/**
 * @brief       启动OneNET MQTT连接（非阻塞，创建独立任务处理）
 * @param       无
 */
void onenet_mqtt_start(void)
{
    xTaskCreatePinnedToCore(
        mqtt_start_task,
        "mqtt_start",
        8192,
        NULL,
        1,
        NULL,
        1
    );
    ESP_LOGI(TAG, "MQTT start task created (non-blocking)");
}
