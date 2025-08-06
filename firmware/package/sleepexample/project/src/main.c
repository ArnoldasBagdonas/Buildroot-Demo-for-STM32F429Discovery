#include <sys/ioctl.h>
#include <linux/rtc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <fcntl.h>

static const uint64_t SLEEP_TIME_US = 5500000ULL;

typedef struct
{
    struct timespec mono; // clock_gettime(CLOCK_MONOTONIC, &mono)
    struct timespec real; // clock_gettime(CLOCK_REALTIME, &real)
    time_t time;          // time(&time)
} custom_time_t;

// Print elapsed time helper
void print_elapsed(custom_time_t start, custom_time_t end)
{
    double end_mono = (double)(end.mono.tv_sec) + (double)(end.mono.tv_nsec) / 1e9;
    double start_mono = (double)(start.mono.tv_sec) + (double)(start.mono.tv_nsec) / 1e9;

    double end_real = (double)(end.real.tv_sec) + (double)(end.real.tv_nsec) / 1e9;
    double start_real = (double)(start.real.tv_sec) + (double)(start.real.tv_nsec) / 1e9;

    printf("Elapsed (MONOTONIC): %.6f seconds\n", end_mono - start_mono);
    printf("Elapsed (REALTIME):  %.6f seconds\n", end_real - start_real);
    printf("Elapsed (time):      %.6f seconds\n", difftime(end.time, start.time));
}

// Print current time
void print_current_time()
{
    char buf[64];
    time_t now = time(NULL);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("Current time: %s\n", buf);
}

// Capture timestamps
void capture_timestamp(custom_time_t *ts)
{
    if (!ts)
        return;

    print_current_time();

    if (clock_gettime(CLOCK_MONOTONIC, &ts->mono) != 0)
        printf("clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));

    if (clock_gettime(CLOCK_REALTIME, &ts->real) != 0)
        printf("clock_gettime(CLOCK_REALTIME) failed: %s\n", strerror(errno));

    if (time(&ts->time) == ((time_t)-1))
        printf("time() failed: %s\n", strerror(errno));
}

void test_sleep()
{
    printf("=== Testing sleep() ===\n");
    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = sleep(SLEEP_TIME_US / 1000000);
    if (ret != 0)
        printf("sleep returned early with %d seconds left\n", ret);
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_usleep()
{
    printf("=== Testing usleep() ===\n");
    custom_time_t start, end;
    capture_timestamp(&start);
    if (usleep(SLEEP_TIME_US) != 0)
        printf("usleep failed: %s\n", strerror(errno));
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_nanosleep()
{
    printf("=== Testing nanosleep() ===\n");
    struct timespec ts;
    ts.tv_sec = (SLEEP_TIME_US) / 1000000;
    ts.tv_nsec = 1000 * ((SLEEP_TIME_US) % 1000000);

    custom_time_t start, end;
    capture_timestamp(&start);
    if (nanosleep(&ts, NULL) != 0)
        printf("nanosleep failed: %s\n", strerror(errno));
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_clock_nanosleep()
{
    printf("=== Testing clock_nanosleep() ===\n");
    struct timespec ts;
    ts.tv_sec = (SLEEP_TIME_US) / 1000000;
    ts.tv_nsec = 1000 * ((SLEEP_TIME_US) % 1000000);

    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    if (ret != 0)
        printf("clock_nanosleep failed: %s\n", strerror(ret));
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_select_delay()
{
    printf("=== Testing select() ===\n");
    struct timeval tv;
    tv.tv_sec = SLEEP_TIME_US / 1000000;
    tv.tv_usec = SLEEP_TIME_US % 1000000;

    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = select(0, NULL, NULL, NULL, &tv);
    if (ret < 0)
        printf("select failed: %s\n", strerror(errno));
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_poll()
{
    printf("=== Testing poll() ===\n");
    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = poll(NULL, 0, SLEEP_TIME_US / 1000);
    if (ret < 0)
        printf("poll failed: %s\n", strerror(errno));
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_epoll()
{
    printf("=== Testing epoll_wait() ===\n");
    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        printf("epoll_create1 failed: %s\n", strerror(errno));
        return;
    }
    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = epoll_wait(epfd, NULL, 0, SLEEP_TIME_US / 1000);
    if (ret < 0)
        printf("epoll_wait failed: %s\n", strerror(errno));
    capture_timestamp(&end);
    close(epfd);
    print_elapsed(start, end);
    printf("\n");
}

void test_pselect()
{
    printf("=== Testing pselect() ===\n");
    struct timespec timeout;
    timeout.tv_sec = (SLEEP_TIME_US) / 1000000;
    timeout.tv_nsec = 1000 * ((SLEEP_TIME_US) % 1000000);

    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = pselect(0, NULL, NULL, NULL, &timeout, NULL);
    if (ret < 0)
        printf("pselect failed: %s\n", strerror(errno));
    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

void test_pthread_cond_timedwait()
{
    printf("=== Testing pthread_cond_timedwait() ===\n");

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_condattr_t attr;
    pthread_cond_t cond;

    if (pthread_condattr_init(&attr) != 0)
        printf("pthread_condattr_init failed: %s\n", strerror(errno));

    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) != 0)
        printf("pthread_condattr_setclock failed: %s\n", strerror(errno));

    if (pthread_cond_init(&cond, &attr) != 0)
        printf("pthread_cond_init failed: %s\n", strerror(errno));

    pthread_condattr_destroy(&attr);

    pthread_mutex_lock(&mutex);

    struct timespec future;
    if (clock_gettime(CLOCK_MONOTONIC, &future) != 0)
        printf("clock_gettime failed: %s\n", strerror(errno));

    future.tv_sec += SLEEP_TIME_US / 1000000;
    future.tv_nsec += 1000 * (SLEEP_TIME_US % 1000000);
    if (future.tv_nsec >= 1000000000)
    {
        future.tv_sec++;
        future.tv_nsec -= 1000000000;
    }

    custom_time_t start, end;
    capture_timestamp(&start);
    int ret = pthread_cond_timedwait(&cond, &mutex, &future);
    if (ret != 0 && ret != ETIMEDOUT)
        printf("pthread_cond_timedwait failed: %s\n", strerror(ret));
    capture_timestamp(&end);

    pthread_mutex_unlock(&mutex);
    pthread_cond_destroy(&cond);

    print_elapsed(start, end);
    printf("\n");
}

void test_timerfd()
{
    printf("=== Testing timerfd_settime() ===\n");
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd == -1)
    {
        printf("timerfd_create failed: %s\n", strerror(errno));
        return;
    }

    struct itimerspec new_value;
    new_value.it_value.tv_sec = (SLEEP_TIME_US) / 1000000;
    new_value.it_value.tv_nsec = 1000 * ((SLEEP_TIME_US) % 1000000);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    custom_time_t start, end;
    capture_timestamp(&start);

    if (timerfd_settime(tfd, 0, &new_value, NULL) == -1)
    {
        printf("timerfd_settime failed: %s\n", strerror(errno));
        close(tfd);
        return;
    }

    uint64_t expirations;
    ssize_t s = read(tfd, &expirations, sizeof(expirations));
    if (s == -1)
        printf("read(timerfd) failed: %s\n", strerror(errno));
    else if (s != sizeof(expirations))
        printf("read(timerfd) returned unexpected size: %zd\n", s);

    capture_timestamp(&end);
    close(tfd);
    print_elapsed(start, end);
    printf("\n");
}

void test_busy_sleep(void)
{
    printf("=== Testing busy sleep with clock_gettime(CLOCK_MONOTONIC) ===\n");
    custom_time_t start, end;
    capture_timestamp(&start);
    struct timespec sleep_start, now;
    int ret = clock_gettime(CLOCK_MONOTONIC, &sleep_start);
    if (ret != 0)
        printf("clock_gettime failed: %s\n", strerror(errno));
    else
        printf("CLOCK_MONOTONIC start: %lld.%09ld\n",
               (long long)sleep_start.tv_sec, sleep_start.tv_nsec);

    do
    {
        ret = clock_gettime(CLOCK_MONOTONIC, &now);
        if (ret != 0)
        {
            printf("clock_gettime failed during busy loop: %s\n", strerror(errno));
            break;
        }
    } while ((now.tv_sec - sleep_start.tv_sec) < (SLEEP_TIME_US / 1000000));

    ret = clock_gettime(CLOCK_MONOTONIC, &now);
    if (ret != 0)
        printf("clock_gettime failed: %s\n", strerror(errno));
    else
        printf("CLOCK_MONOTONIC end: %lld.%09ld\n",
               (long long)now.tv_sec, now.tv_nsec);

    capture_timestamp(&end);
    print_elapsed(start, end);
    printf("\n");
}

int test_set_system_time(int year, int mon, int day, int hour, int min, int sec)
{
    printf("=== Testing set system time with clock_settime(CLOCK_REALTIME, &ts) ===\n");
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;

    time_t time_to_set = mktime(&t);
    if (time_to_set == -1)
    {
        perror("mktime");
        return -1;
    }

    struct timespec ts;
    ts.tv_sec = time_to_set;
    ts.tv_nsec = 0;

    if (clock_settime(CLOCK_REALTIME, &ts) == -1)
    {
        perror("clock_settime");
        return -1;
    }

    printf("System time set successfully\n");
    return 0;
}

int test_set_system_settimeofday(int year, int mon, int day, int hour, int min, int sec)
{
    printf("=== Testing set system time with settimeofday() ===\n");
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;

    time_t time_to_set = mktime(&t);
    if (time_to_set == -1)
    {
        perror("mktime");
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = time_to_set;
    tv.tv_usec = 0;

    if (settimeofday(&tv, NULL) == -1)
    {
        perror("settimeofday");
        return -1;
    }
    printf("System time set successfully\n");
    return 0;
}

void test_set_time_hwclock(void)
{
    printf("=== Testing set system time with hwclock() ===\n");
    int ret;
    ret = system("hwclock -w");
    if (ret != 0)
        printf("hwclock -w failed with return code %d\n", ret);
    else
        printf("hwclock -w succeeded\n");

    ret = system("hwclock -s");
    if (ret != 0)
        printf("hwclock -s failed with return code %d\n", ret);
    else
        printf("hwclock -s succeeded\n");
    printf("\n");
}

int main()
{
    printf("Starting sleep/time tests with detailed error logging\n\n");

    test_sleep();
    test_usleep();
    test_nanosleep();
    test_clock_nanosleep();
    test_select_delay();
    test_poll();
    test_epoll();
    test_pselect();
    test_pthread_cond_timedwait();
    test_timerfd();
    test_busy_sleep();

    // Set system time to a fixed date/time (example: 2020-01-01 12:00:00)
    test_set_system_time(2020, 1, 1, 12, 0, 0);
    test_set_system_settimeofday(2020, 1, 1, 12, 0, 0);
    test_set_time_hwclock();

    printf("Tests completed.\n");

    return 0;
}
