#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "JNIpart"
#define LOGV(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define LOGD(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define LOGI(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#include <time.h> // clock_gettime

static inline int64_t getTimeMs() {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static inline int getTimeInterval(int64_t startTime) {
  return int(getTimeMs() - startTime);
}
#endif

#ifdef _MSC_VER
#define LOGV(...)                                                              \
  ((void)printf(__VA_ARGS__), printf("\n"))
#define LOGD(...)                                                              \
  ((void)printf(__VA_ARGS__), printf("\n"))
#define LOGI(...)                                                              \
  ((void)printf(__VA_ARGS__), printf("\n"))
#define LOGE(...)                                                              \
  ((void)printf(__VA_ARGS__), printf("\n"))

#include <windows.h> // QueryPerformanceCounter

static inline int64_t getTimeMs()
{
	int64_t cnt;
	QueryPerformanceCounter((LARGE_INTEGER *)&cnt);
	return cnt;
}

static inline int getTimeInterval(int64_t cntStart)
{
	int64_t cntEnd, cntFreq;
	QueryPerformanceCounter((LARGE_INTEGER *)&cntEnd);
	QueryPerformanceFrequency((LARGE_INTEGER *)&cntFreq);	// counts per second
	int intervalMs = int((cntEnd - cntStart) / double(cntFreq) * 1000.0);
	return intervalMs;
}
#endif