#ifndef __security_entrance_tizen_app_H__
#define __security_entrance_tizen_app_H__

#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "security-entrance-tizen-app"

#define MV_RESULT_COUNT_MAX 30
#define MV_RESULT_LENGTH_MAX (MV_RESULT_COUNT_MAX * 4) //4(x, y, w, h) * COUNT
#define IMAGE_INFO_MAX ((8 * MV_RESULT_LENGTH_MAX) + 4)

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define CAMERA_IMAGE_QUALITY 100 //1~100
#define CAMERA_PREVIEW_INTERVAL_MIN 50

#endif /* __smart-security-entrance_H__ */
