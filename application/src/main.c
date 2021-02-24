
#include <stdio.h>
#include <zephyr.h>
#include <sys/printk.h>
#include "secure_services.h"
#include <logging/log.h>

#include <stdlib.h>
#if defined(CONFIG_BSD_LIB)
#include <modem/lte_lc.h>
#include <modem/bsdlib.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include <modem/modem_info.h>
#include <bsd.h>
#endif
#include <net/aws_iot.h>
#include <power/reboot.h>
#include <date_time.h>
#include <cJSON.h>
#include <cJSON_os.h>
#include "measures.h"
#include <date_time.h>

#include <drivers/gpio.h>

#define RED_LED_PIN DT_GPIO_PIN(DT_NODELABEL(red_led), gpios)
#define GREEN_LED_PIN DT_GPIO_PIN(DT_NODELABEL(green_led), gpios)
#define BLUE_LED_PIN DT_GPIO_PIN(DT_NODELABEL(blue_led), gpios)

#define BUTTON_NODE DT_NODELABEL(button0)
#define BUTTON_GPIO_LABEL DT_GPIO_LABEL(BUTTON_NODE, gpios)
#define BUTTON_GPIO_PIN DT_GPIO_PIN(BUTTON_NODE, gpios)
#define BUTTON_GPIO_FLAGS GPIO_INPUT | DT_GPIO_FLAGS(BUTTON_NODE, gpios)

#define LED_ON 0
#define LED_OFF !LED_ON
static const struct device *gpio_dev;
static struct gpio_callback gpio_cb;
bool button_pressed = false;

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

char custom_topic[75] = "";
uint64_t get_uid();

uint32_t randint(uint32_t a, uint32_t b);

BUILD_ASSERT(!IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT),
             "This sample does not support LTE auto-init and connect");

#define APP_TOPICS_COUNT CONFIG_AWS_IOT_APP_SUBSCRIPTION_LIST_COUNT
#define DATE_TIME_TIMEOUT_S 15

static struct k_delayed_work connect_work;

static struct k_delayed_work send_measure_work;

K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(date_time_obtained, 0, 1);

void publish(char *topic, char *payload)
{
    int err;
    struct aws_iot_data tx_data = {
        .qos = MQTT_QOS_1_AT_LEAST_ONCE,
        .topic.type = AWS_IOT_SHADOW_TOPIC_UNKNOWN,
        .topic.str = topic,
        .topic.len = strlen(topic),
        .ptr = payload,
        .len = strlen(payload)};

    // dont send if client is not live
    LOG_INF("publish: %s/%s", tx_data.topic.str, tx_data.ptr);
    err = aws_iot_send(&tx_data);

    if (err)
    {
        //   if (err != -EINPROGRESS)
        //   {
        LOG_ERR("mqtt_publish, error: %d", err);
        //   }
    }
}

size_t pad16(uint8_t *buf, size_t lenIn)
{
    if (lenIn % 16)
    {
        for (int i = 0; i < 16 - (lenIn % 16); i++)
        {
            buf[lenIn + i] = 0;
        }
        return lenIn + (16 - (lenIn % 16));
    }
    else
    {
        return lenIn;
    }
}

static void connect_work_fn(struct k_work *work)
{
    int err;

    err = aws_iot_connect(NULL);
    if (err)
    {
        LOG_ERR("aws_iot_connect, error: %d", err);
    }

    LOG_WRN("Next connection retry in %d seconds",
            CONFIG_CONNECTION_RETRY_TIMEOUT_SECONDS);

    k_delayed_work_submit(&connect_work,
                          K_SECONDS(CONFIG_CONNECTION_RETRY_TIMEOUT_SECONDS));
}
void deserialize(uint8_t *inBuffer, size_t inStrLen, uint8_t *outBuffer)
{
    for (int i = 0; i < inStrLen / 2; i++)
    {
        uint8_t tmp = 0;
        uint8_t byte;
        byte = inBuffer[(2 * i)];
        if (byte >= '0' && byte <= '9')
            byte = byte - '0';
        else if (byte >= 'a' && byte <= 'f')
            byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <= 'F')
            byte = byte - 'A' + 10;
        tmp |= byte << 4;

        byte = inBuffer[(2 * i) + 1];
        if (byte >= '0' && byte <= '9')
            byte = byte - '0';
        else if (byte >= 'a' && byte <= 'f')
            byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <= 'F')
            byte = byte - 'A' + 10;
        tmp |= byte & 0xF;
        outBuffer[i] = tmp;
    }
}

static void print_received_data(const char *buf, const char *topic,
                                size_t topic_len)
{
    cJSON *root_obj = NULL;

    root_obj = cJSON_Parse(buf);
    if (root_obj == NULL)
    {
        LOG_ERR("cJSON Parse failure");
        return;
    }
    uint32_t n_payloads = cJSON_GetArraySize(root_obj);
    LOG_DBG("received %d payloads", n_payloads);

    measure_t measures[10];

    for (int i = 0; i < n_payloads; i++)
    {
        //uint8_t buf[256];
        cJSON *n = cJSON_GetArrayItem(root_obj, i);
        if (n == NULL)
        {
            LOG_ERR("Failed to get item at index %d", i);
        }
        cJSON *entry = cJSON_GetObjectItemCaseSensitive(n, "entry");
        if (entry == NULL)
        {
            LOG_ERR("received misformed json");
        }
        if (cJSON_IsString(entry) && (entry->valuestring != NULL))
        {
            //LOG_DBG("treating %s", entry->valuestring);
            uint8_t buf[128];
            size_t len = strlen(entry->valuestring);
            deserialize(entry->valuestring, len, buf);
            uint8_t plain[128];
            decrypt(buf, plain, len / 2);
            unpack(plain, &measures[i]);
            LOG_INF("decrypted ");
            printMUID(&measures[i]);
        }
    }
    // shuffle measures
    shuffle(measures, n_payloads);

    for (int i = 0; i < n_payloads; i++)
    {
        char *message = measure_as_json(&measures[i]);
        if (message)
        {
            publish("measures", message);
            LOG_WRN("str: %s", message);
            cJSON_free(message);
        }
        else
        {
            LOG_ERR("failed to print json unformatted");
        }
    }
    cJSON_Delete(root_obj);
    gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_OFF);
}

void aws_iot_event_handler(const struct aws_iot_evt *const evt)
{
    switch (evt->type)
    {
    case AWS_IOT_EVT_CONNECTING:
        LOG_INF("AWS_IOT_EVT_CONNECTING");
        if (k_delayed_work_pending(&send_measure_work))
        {
            k_delayed_work_cancel(&send_measure_work);
        }
        break;
    case AWS_IOT_EVT_CONNECTED:
        LOG_INF("AWS_IOT_EVT_CONNECTED");

        if (evt->data.persistent_session)
        {
            LOG_INF("Persistent session enabled");
        }

#if defined(CONFIG_BSD_LIBRARY)
        int err = lte_lc_psm_req(true);
        if (err)
        {
            LOG_ERR("Requesting PSM failed, error: %d", err);
        }
#endif
        break;
    case AWS_IOT_EVT_READY:
        LOG_INF("AWS_IOT_EVT_READY");
        gpio_pin_set(gpio_dev, RED_LED_PIN, LED_OFF);
        k_delayed_work_submit(&send_measure_work, K_MSEC(5000));

        break;
    case AWS_IOT_EVT_DISCONNECTED:
        gpio_pin_set(gpio_dev, RED_LED_PIN, LED_ON);
        LOG_INF("AWS_IOT_EVT_DISCONNECTED");
        if (k_delayed_work_pending(&send_measure_work))
        {
            k_delayed_work_cancel(&send_measure_work);
        }

        if (k_delayed_work_pending(&connect_work))
        {
            break;
        }

        k_delayed_work_submit(&connect_work, K_NO_WAIT);
        break;
    case AWS_IOT_EVT_DATA_RECEIVED:
        LOG_INF("AWS_IOT_EVT_DATA_RECEIVED");
        print_received_data(evt->data.msg.ptr, evt->data.msg.topic.str,
                            evt->data.msg.topic.len);
        break;

    case AWS_IOT_EVT_ERROR:
        LOG_ERR("AWS_IOT_EVT_ERROR, %d", evt->data.err);
        break;
    default:
        LOG_WRN("Unknown AWS IoT event type: %d", evt->type);
        break;
    }
}

static void work_init(void)
{
    //k_delayed_work_init(&shadow_update_work, shadow_update_work_fn);
    k_delayed_work_init(&connect_work, connect_work_fn);
    //k_delayed_work_init(&shadow_update_version_work,
    //                    shadow_update_version_work_fn);
}

#if defined(CONFIG_BSD_LIBRARY)
static void lte_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type)
    {
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
            (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
        {
            break;
        }

        LOG_INF("Network registration status: %s",
                evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");

        k_sem_give(&lte_connected);
        break;
    case LTE_LC_EVT_PSM_UPDATE:
        LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
                evt->psm_cfg.tau, evt->psm_cfg.active_time);
        break;
    case LTE_LC_EVT_EDRX_UPDATE:
    {
        char log_buf[60];
        ssize_t len;

        len = snprintf(log_buf, sizeof(log_buf),
                       "eDRX parameter update: eDRX: %f, PTW: %f",
                       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
        if (len > 0)
        {
            LOG_INF("%s", log_buf);
        }
        break;
    }
    case LTE_LC_EVT_RRC_UPDATE:
        LOG_INF("RRC mode: %s",
                evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
        break;
    case LTE_LC_EVT_CELL_UPDATE:
        LOG_INF("LTE cell changed: Cell ID: %d, Tracking area: %d",
                evt->cell.id, evt->cell.tac);
        break;
    default:
        break;
    }
}

static void modem_configure(void)
{
    int err;

    if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT))
    {
        /* Do nothing, modem is already configured and LTE connected. */
    }
    else
    {
        err = lte_lc_init_and_connect_async(lte_handler);
        if (err)
        {
            LOG_ERR("Modem could not be configured, error: %d",
                    err);
            return;
        }
    }
}

static void at_configure(void)
{
    int err;

    err = at_notif_init();
    __ASSERT(err == 0, "AT Notify could not be initialized.");
    err = at_cmd_init();
    __ASSERT(err == 0, "AT CMD could not be established.");
}
#endif

static int app_topics_subscribe(void)
{
    int err;
    uint64_t uid = get_uid();
    sprintf(custom_topic, "getPool%08" PRIx64, uid);
    LOG_INF("subscribing to %s", custom_topic);
    const struct aws_iot_topic_data topics_list[APP_TOPICS_COUNT] = {
        [0].str = custom_topic,
        [0].len = strlen(custom_topic)};

    err = aws_iot_subscription_topics_add(topics_list,
                                          ARRAY_SIZE(topics_list));
    if (err)
    {
        LOG_ERR("aws_iot_subscription_topics_add, error: %d", err);
    }

    return err;
}
static void date_time_event_handler(const struct date_time_evt *evt)
{
    switch (evt->type)
    {
    case DATE_TIME_OBTAINED_MODEM:
        LOG_INF("DATE_TIME_OBTAINED_MODEM");
        break;
    case DATE_TIME_OBTAINED_NTP:
        LOG_INF("DATE_TIME_OBTAINED_NTP");
        break;
    case DATE_TIME_OBTAINED_EXT:
        LOG_INF("DATE_TIME_OBTAINED_EXT");
        break;
    case DATE_TIME_NOT_OBTAINED:
        LOG_WRN("DATE_TIME_NOT_OBTAINED");
        break;
    default:
        break;
    }

    /** Do not depend on obtained time, continue upon any event from the
 *  date time library.
 */
    k_sem_give(&date_time_obtained);
}

uint32_t randint(uint32_t a, uint32_t b)
{
    uint64_t rand[18];
    size_t oLen = 0;
    spm_request_random_number((uint8_t *)rand, sizeof(rand), &oLen);
    return (rand[0] % (b - a)) + a;
}
uint64_t get_uid()
{
    // get Device UID
    uint64_t uid = 0;
    spm_request_read(&uid, 0x00FF0000 + 0x204, sizeof(uid));
    return uid;
}
void send_measure()
{
    gpio_pin_set(gpio_dev, BLUE_LED_PIN, LED_ON);
    int err;
    int16_t temperature_data;

    // generate measure
    measure_t m;
    memset(&m, 0, sizeof(m));
    GenerateMUID(&m, get_uid());
    // add GPS location

    // dummy values
    err = modem_info_short_get(MODEM_INFO_TEMP, &temperature_data);
    m.data = temperature_data;

    // encrypt measure
    uint8_t buf[128];
    uint8_t encrypted[128];
    size_t len_packed = pack(buf, &m);
    size_t len_padded = pad16(buf, len_packed);
    uint8_t hex_buf[256];

    encrypt(buf, encrypted, len_padded);

    for (int i = 0; i < len_padded; i++)
    {
        sprintf(&hex_buf[i * 2], "%02X", encrypted[i]);
    }

    // send payload
    printMUID(&m);

    publish("addToPool", hex_buf);
    char *message = measure_as_json(&m);
    if (message)
    {
        publish("cheater", message);
        cJSON_free(message);
    }
    else
    {
        LOG_ERR("failed to allocate memory for json message");
    }

    // with 10% chance, ask for payload pool
    int chance = randint(0, 10000);
    LOG_INF("chance = %d", chance);
    if (chance < 1500)
    {
        LOG_WRN("asking for pool");
        {
            uint64_t uid = get_uid();
            sprintf(buf, "%08" PRIx64, uid);
            publish("requestPool", buf);
            gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_ON);
        }
    }
    k_delayed_work_submit(&send_measure_work, K_MSEC(10000 + randint(0, 1000)));
    gpio_pin_set(gpio_dev, BLUE_LED_PIN, LED_OFF);
}
void init_leds(void)
{
    gpio_pin_configure(gpio_dev, RED_LED_PIN, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, GREEN_LED_PIN, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio_dev, BLUE_LED_PIN, GPIO_OUTPUT_HIGH);
}
void button_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    button_pressed = true;
}
bool init_button(void)
{
    int ret = gpio_pin_configure(gpio_dev, BUTTON_GPIO_PIN, BUTTON_GPIO_FLAGS);
    if (ret != 0)
    {
        printk("Error %d: failed to configure %s pin %d\n",
               ret, BUTTON_GPIO_LABEL, BUTTON_GPIO_PIN);

        return false;
    }

    ret = gpio_pin_interrupt_configure(gpio_dev,
                                       BUTTON_GPIO_PIN,
                                       GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0)
    {
        printk("Error %d: failed to configure interrupt on %s pin %d\n",
               ret, BUTTON_GPIO_LABEL, BUTTON_GPIO_PIN);

        return false;
    }

    gpio_init_callback(&gpio_cb, button_pressed_callback, BIT(BUTTON_GPIO_PIN));
    gpio_add_callback(gpio_dev, &gpio_cb);

    return true;
}

void main(void)
{
    gpio_dev = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));

    if (!gpio_dev)
    {
        LOG_ERR("Error getting GPIO device binding\r");
    }
    if (!init_button())
    {
        LOG_ERR("failed to init button");
    }
    init_leds();
    gpio_pin_set(gpio_dev, RED_LED_PIN, LED_ON);
    gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_ON);
    gpio_pin_set(gpio_dev, BLUE_LED_PIN, LED_OFF);

    struct aws_iot_config iot_config;
    int err;

    LOG_WRN("MSE TM Privacy impl demo started, version: %s", CONFIG_APP_VERSION);

    cJSON_Init();

#if defined(CONFIG_BSD_LIBRARY)
    err = bsdlib_init();
    at_configure();
#endif
    err = aws_iot_init(&iot_config, aws_iot_event_handler);
    if (err)
    {
        LOG_ERR("AWS IoT library could not be initialized, error: %d",
                err);
    }

    /** Subscribe to customizable non-shadow specific topics
 *  to AWS IoT backend.
 */

    work_init();

    k_delayed_work_init(&send_measure_work, send_measure);
#if defined(CONFIG_BSD_LIBRARY)
    err = modem_info_init();
    if (err)
    {
        LOG_ERR("Failed initializing modem info module, error: %d",
                err);
    }
    modem_configure();

    k_sem_take(&lte_connected, K_FOREVER);
#endif

    date_time_update_async(date_time_event_handler);

    err = k_sem_take(&date_time_obtained, K_SECONDS(DATE_TIME_TIMEOUT_S));
    if (err)
    {
        LOG_ERR("Date time, no callback event within %d seconds",
                DATE_TIME_TIMEOUT_S);
    }

    err = aws_iot_connect(&iot_config);
    if (err)
    {
        LOG_ERR("aws_iot_connect failed: %d", err);
    }
    err = app_topics_subscribe();
    if (err)
    {
        LOG_ERR("Adding application specific topics failed, error: %d",
                err);
    }
    //k_delayed_work_submit(&send_measure_work, K_MSEC(5000));
    gpio_pin_set(gpio_dev, RED_LED_PIN, LED_OFF);
    gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_OFF);
    while (1)
    {
        k_sleep(K_MSEC(10));
        if (button_pressed)
        {
            button_pressed = false;
            uint8_t buf[64];
            uint64_t uid = get_uid();
            sprintf(buf, "%08" PRIx64, uid);
            publish("requestPool", buf);
        }
    }
}