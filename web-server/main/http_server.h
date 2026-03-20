#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t http_server_start(void);
bool http_server_is_running(void);

#endif // HTTP_SERVER_H
