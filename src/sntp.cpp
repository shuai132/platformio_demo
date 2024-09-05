#include "sntp.h"

#include "esp_sntp.h"
#include "log.h"

static void time_sync_notification_cb(struct timeval *tv) {
  (void)(tv);
  LOGD("time_sync_notification_cb");
}

void sntp_obtain_time() {
  // wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 10;
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
    LOGI("Waiting for system time to be set... (%d/%d)", retry, retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  time(&now);
  localtime_r(&now, &timeinfo);
}

void sntp_env_init() {
  LOGD("sntp_env_init");
  // Set timezone to China Standard Time
  setenv("TZ", "CST-8", 1);
  tzset();

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "ntp.aliyun.com");
  sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  sntp_init();
}
