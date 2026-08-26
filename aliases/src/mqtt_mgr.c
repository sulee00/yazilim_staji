#include "mqtt_mgr.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/random/random.h>
#include <string.h>

LOG_MODULE_REGISTER(mqtt_mgr, LOG_LEVEL_INF);

static struct mqtt_client client_ctx;
static struct sockaddr_storage broker;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static bool connected = false;
static bool connecting = false;

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result == 0) {
            LOG_INF("MQTT baglantisi basarili.");
            connected = true;
        } else {
            LOG_ERR("MQTT Connack hatasi: %d", evt->result);
            connected = false;
        }
        connecting = false;
        break;
    case MQTT_EVT_DISCONNECT:
        connected = false;
        connecting = false;
        break;
    default:
        break;
    }
}

int mqtt_mgr_init(const mqtt_mgr_config_t *cfg)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(cfg->broker_port);
    zsock_inet_pton(AF_INET, cfg->broker_ip, &broker4->sin_addr);

    mqtt_client_init(&client_ctx);
    client_ctx.broker = &broker;
    client_ctx.evt_cb = mqtt_evt_handler;
    client_ctx.client_id.utf8 = (uint8_t *)cfg->client_id;
    client_ctx.client_id.size = strlen(cfg->client_id);
    client_ctx.password = NULL;
    client_ctx.user_name = NULL;
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;
    client_ctx.rx_buf = rx_buffer;
    client_ctx.rx_buf_size = sizeof(rx_buffer);
    client_ctx.tx_buf = tx_buffer;
    client_ctx.tx_buf_size = sizeof(tx_buffer);
    client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

    return 0;
}

int mqtt_mgr_connect(void)
{
    if (connected || connecting) return 0;
    connecting = true;
    return mqtt_connect(&client_ctx);
}

void mqtt_mgr_process(void)
{
    if (connecting || connected) {
        mqtt_input(&client_ctx);
        mqtt_live(&client_ctx);
    }
}

int mqtt_mgr_publish(const char *topic, const char *payload)
{
    if (!connected) return -ENOTCONN;

    struct mqtt_publish_param param;
    param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)payload;
    param.message.payload.len = strlen(payload);
    param.message_id = sys_rand32_get();
    param.dup_flag = 0;
    param.retain_flag = 0;

    return mqtt_publish(&client_ctx, &param);
}

bool mqtt_mgr_is_connected(void)
{
    return connected;
}