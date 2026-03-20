#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_nvs.h"
#include "task_settings.h"
#include "wifi_app.h"
#include "main.h"

// Queue handle used to manipulate the main queue of events.
QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t eventID)
{
  app_event_t msg;
  msg.event_id = eventID;
  return xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
}

static void main_task(void *pvParameters)
{
  (void)pvParameters;

  app_queue_handle = xQueueCreate(10, sizeof(app_event_t));
  if (app_queue_handle == NULL)
  {
    printf("Failed to create app queue\n");
    vTaskDelete(NULL);
  }

  wifi_app_start();

  app_send_message(WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS);

  while (1)
  {
    app_event_t app_event;
    if (xQueueReceive(app_queue_handle, &app_event, pdMS_TO_TICKS(1000)) == pdPASS)
    {
      printf("main_task received event id: %lu\n", (unsigned long)app_event.event_id);
      switch (app_event.event_id)
      {
      case WIFI_APP_MSG_STA_CONNECTED_GOT_IP:
        ESP_ERROR_CHECK(app_nvs_save_sta_creds());
        break;

      case WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS:
        ESP_ERROR_CHECK(app_nvs_load_sta_creds());
        break;

      default:
        break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void)
{
  xTaskCreatePinnedToCore(main_task,
                          "main_task",
                          MAIN_TASK_STACK_SIZE,
                          NULL,
                          MAIN_TASK_PRIORITY,
                          NULL,
                          MAIN_TASK_CORE_ID);
}