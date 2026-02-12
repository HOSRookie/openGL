#pragma once

#include <hilog/log.h>

#ifndef GLEX_LOG_DOMAIN
#define GLEX_LOG_DOMAIN 0xFF01
#endif

#ifndef GLEX_LOG_TAG
#define GLEX_LOG_TAG "GLEX"
#endif

#define GLEX_LOGI(fmt, ...) OH_LOG_Print(LOG_APP, LOG_INFO, GLEX_LOG_DOMAIN, GLEX_LOG_TAG, fmt, ##__VA_ARGS__)
#define GLEX_LOGW(fmt, ...) OH_LOG_Print(LOG_APP, LOG_WARN, GLEX_LOG_DOMAIN, GLEX_LOG_TAG, fmt, ##__VA_ARGS__)
#define GLEX_LOGE(fmt, ...) OH_LOG_Print(LOG_APP, LOG_ERROR, GLEX_LOG_DOMAIN, GLEX_LOG_TAG, fmt, ##__VA_ARGS__)
#define GLEX_LOGD(fmt, ...) OH_LOG_Print(LOG_APP, LOG_DEBUG, GLEX_LOG_DOMAIN, GLEX_LOG_TAG, fmt, ##__VA_ARGS__)
