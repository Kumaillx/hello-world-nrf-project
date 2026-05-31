#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>

/* Register Single Master Log Module */
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

#define FW_VERSION "1.0.0"
#define STACK_SIZE 1024
#define PRIORITY   7

/* Safe Thread Synchronizations */
K_MUTEX_DEFINE(storage_mutex);
K_MSGQ_DEFINE(sensor_msgq, sizeof(uint32_t), 10, 4);

static uint32_t boot_counter = 0;

/* Static BLE Advertising Data Structure */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, (sizeof(CONFIG_BT_DEVICE_NAME) - 1)),
};

/* --- Thread 1: Logger & Storage Monitor Context --- */
void logger_thread_entry(void *p1, void *p2, void *p3)
{
    uint32_t received_sensor_val;
    
    k_mutex_lock(&storage_mutex, K_FOREVER);
    boot_counter++;
    LOG_INF("========================================");
    LOG_INF("Application Boot Success");
    LOG_INF("Firmware Version: %s", FW_VERSION);
    LOG_INF("Bootloader Status: MCUboot Active");
    LOG_INF("Boot Counter safely incremented: %d", boot_counter);
    LOG_INF("========================================");
    k_mutex_unlock(&storage_mutex);

    while (1) {
        if (k_msgq_get(&sensor_msgq, &received_sensor_val, K_MSEC(500)) == 0) {
            LOG_INF("[Logger Thread] Processing Sensor Metric: %d", received_sensor_val);
        }
        k_sleep(K_MSEC(100));
    }
}
K_THREAD_DEFINE(logger_tid, STACK_SIZE, logger_thread_entry, NULL, NULL, NULL, PRIORITY, 0, 0);

/* --- Thread 2: Sensor Mocking Context --- */
void sensor_thread_entry(void *p1, void *p2, void *p3)
{
    uint32_t dummy_metric = 100;
    while (1) {
        dummy_metric++;
        k_msgq_put(&sensor_msgq, &dummy_metric, K_NO_WAIT);
        k_sleep(K_MSEC(2000)); 
    }
}
K_THREAD_DEFINE(sensor_tid, STACK_SIZE, sensor_thread_entry, NULL, NULL, NULL, PRIORITY, 0, 0);

/* --- Thread 3: Main/BLE System Initialization Context --- */
int main(void)
{
    int err;

    /* Initialize Bluetooth Subsystem */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    /* Start Wireless Broadcasting */
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth Advertising successfully started as '%s'", CONFIG_BT_DEVICE_NAME);
    
    return 0;
}