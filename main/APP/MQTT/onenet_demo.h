#ifndef __ONENET_DEMO_H
#define __ONENET_DEMO_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

/* ========== WiFi 配置 ========== */
#define WIFI_SSID       "Ne"            /* WiFi 名称（你的手机热点） */
#define WIFI_PASS       "1111122222"    /* WiFi 密码 */
#define WIFI_MAX_RETRY  5               /* 最大重连次数 */

/* ========== OneNET 配置参数 ========== */
#define PRODUCT_ID      "csf356xA3b"                            /* 产品ID */
#define DEVICE_ID       "ATest"                                 /* 设备名称 */
#define DEVICE_TOKEN    "version=2018-10-31&res=products%2Fcsf356xA3b%2Fdevices%2FATest&et=1880976613&method=md5&sign=q8A0UO3EN7bzLulNU0C21Q%3D%3D"  /* 生成的token */
#define MQTT_HOST       "mqtts.heclouds.com"                    /* 固定域名 */
#define MQTT_PORT       1883                                    /* 固定端口号 */

/* ========== OneNET MQTT 主题 ========== */
#define TOPIC_PROP_POST         "$sys/" PRODUCT_ID "/" DEVICE_ID "/thing/property/post"
#define TOPIC_PROP_SET          "$sys/" PRODUCT_ID "/" DEVICE_ID "/thing/property/set"
#define TOPIC_PROP_POST_REPLY   "$sys/" PRODUCT_ID "/" DEVICE_ID "/thing/property/post_reply"
#define TOPIC_PROP_SET_REPLY    "$sys/" PRODUCT_ID "/" DEVICE_ID "/thing/property/set_reply"

/* 上报数据JSON模板 */
#define ONE_NET_BODY_FORMAT     "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":%s}"

/* WiFi 事件标志组 */
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

/* ========== 函数声明 ========== */
void onenet_mqtt_start(void);

#endif /* __ONENET_DEMO_H */