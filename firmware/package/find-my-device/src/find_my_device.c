#define _GNU_SOURCE

#include "device_info.h"
#include "mdns_codec.h"
#include "oauth_service.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_INTERFACE "eth0"
#define DEFAULT_MODEL "FINDER-R01"
#define DEFAULT_NAME "My Finder"
#define DEFAULT_STATE_FILE "/var/lib/find-my-device/state"
#define DEFAULT_BUTTON_CHIP "/dev/gpiochip0"
#define DEFAULT_LED_CHIP "/dev/gpiochip6"
#define DEFAULT_BUTTON_LINE 0U  /* PA0, on-board blue USER button */
#define DEFAULT_LED_LINE 13U    /* PG13, on-board green LED */
#define DEFAULT_HTTP_PORT 8080U
#define HTTP_BUFFER_BYTES 4096U
#define MDNS_BUFFER_BYTES 768U
#define MDNS_PORT 5353U
#define MDNS_GROUP "224.0.0.251"
#define STATE_MAGIC UINT32_C(0x464d4433) /* FMD3 */
#define STATE_VERSION 1U
#define LED_FLASH_MS UINT64_C(200)
#define LED_PATTERN_PAUSE_MS UINT64_C(1200)
#define SERVICE_TICK_NS 50000000L

typedef struct
{
    const char *interface_name;
    const char *model;
    const char *state_path;
    const char *button_chip;
    const char *led_chip;
    const char *address_override;
    uint16_t port;
    unsigned int button_line;
    unsigned int led_line;
    bool button_active_low;
    bool gpio_enabled;
    bool prepare_interface;
    bool device_id_override_set;
    uint64_t device_id_override;
} Options;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    char device_name[64];
    OAuthPersistentState oauth;
} PersistentState;

typedef struct
{
    int button_fd;
    int led_fd;
    bool button_last;
    unsigned int button_stable_count;
    unsigned int flash_toggles;
    bool flash_level;
    uint64_t flash_next_ms;
    unsigned int confirmation_flashes;
    unsigned int confirmation_flashes_completed;
    bool confirmation_level;
    uint64_t confirmation_next_ms;
    bool link_up;
} PlatformControls;

typedef struct
{
    Options options;
    PersistentState persistent;
    OAuthService oauth;
    DeviceInfo info;
    MdnsServiceConfig mdns;
    char device_id[17];
    char device_name[64];
    char instance_name[128];
    char host_name[96];
    struct in_addr address;
    int http_socket;
    int mdns_socket;
    int service_timer_fd;
    int websocket_socket;
    uint32_t websocket_counter;
    uint64_t websocket_next_ms;
    uint64_t start_ms;
    bool announce_now;
    PlatformControls controls;
} Application;

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t confirm_requested;
static volatile sig_atomic_t address_reload_requested;

static void handle_signal(int signal_number)
{
    if (signal_number == SIGUSR1)
        confirm_requested = 1;
    else if (signal_number == SIGHUP)
        address_reload_requested = 1;
    else if (signal_number == SIGINT || signal_number == SIGTERM)
        stop_requested = 1;
}

static int create_service_timer(void)
{
    struct itimerspec timer;
    int descriptor = timerfd_create(CLOCK_MONOTONIC, 0);
    if (descriptor < 0)
        return -1;

    memset(&timer, 0, sizeof(timer));
    timer.it_interval.tv_nsec = SERVICE_TICK_NS;
    timer.it_value = timer.it_interval;
    if (timerfd_settime(descriptor, 0, &timer, NULL) != 0)
    {
        close(descriptor);
        return -1;
    }
    return descriptor;
}

static uint64_t monotonic_ms(void)
{
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
        return 0U;
    return (uint64_t)value.tv_sec * UINT64_C(1000) + (uint64_t)value.tv_nsec / UINT64_C(1000000);
}

static uint64_t now_seconds(const Application *application)
{
    return (monotonic_ms() - application->start_ms) / UINT64_C(1000);
}

static bool parse_u16(const char *text, uint16_t *value)
{
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0U || parsed > 65535U)
        return false;
    *value = (uint16_t)parsed;
    return true;
}

static bool parse_uint(const char *text, unsigned int *value)
{
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX)
        return false;
    *value = (unsigned int)parsed;
    return true;
}

static bool parse_device_id(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;
    if (text == NULL || strlen(text) != 16U)
        return false;
    errno = 0;
    parsed = strtoull(text, &end, 16);
    if (errno != 0 || end == text || *end != '\0')
        return false;
    *value = (uint64_t)parsed;
    return true;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --interface NAME       network interface (default eth0)\n"
            "  --port PORT            HTTP port (default 8080)\n"
            "  --state PATH           persistent state file\n"
            "  --name NAME            initial friendly name\n"
            "  --model MODEL          product model\n"
            "  --address IPV4         address override for host tests\n"
            "  --device-id HEX16      identity override for host tests\n"
            "  --button-chip PATH     GPIO chip for physical confirmation\n"
            "  --button-line OFFSET   GPIO line (default PA0)\n"
            "  --button-active-low    button is pressed at logic low\n"
            "  --led-chip PATH        GPIO chip for active-high status LED\n"
            "  --led-line OFFSET      GPIO line (default PG13)\n"
            "  --no-gpio              use SIGUSR1 confirmation only\n"
            "  --prepare-interface    set the stable MAC and exit\n",
            program);
}

static bool parse_options(int argc, char **argv, Options *options, const char **initial_name)
{
    int index;
    memset(options, 0, sizeof(*options));
    options->interface_name = DEFAULT_INTERFACE;
    options->model = DEFAULT_MODEL;
    options->state_path = DEFAULT_STATE_FILE;
    options->button_chip = DEFAULT_BUTTON_CHIP;
    options->led_chip = DEFAULT_LED_CHIP;
    options->port = DEFAULT_HTTP_PORT;
    options->button_line = DEFAULT_BUTTON_LINE;
    options->led_line = DEFAULT_LED_LINE;
    options->gpio_enabled = true;
    *initial_name = DEFAULT_NAME;

    for (index = 1; index < argc; index++)
    {
        const char *option = argv[index];
        const char *argument = index + 1 < argc ? argv[index + 1] : NULL;
        if (strcmp(option, "--no-gpio") == 0)
            options->gpio_enabled = false;
        else if (strcmp(option, "--button-active-low") == 0)
            options->button_active_low = true;
        else if (strcmp(option, "--prepare-interface") == 0)
            options->prepare_interface = true;
        else if (strcmp(option, "--help") == 0)
        {
            usage(argv[0]);
            exit(0);
        }
        else if (argument == NULL)
            return false;
        else if (strcmp(option, "--interface") == 0)
        {
            options->interface_name = argument;
            index++;
        }
        else if (strcmp(option, "--port") == 0)
        {
            if (!parse_u16(argument, &options->port))
                return false;
            index++;
        }
        else if (strcmp(option, "--state") == 0)
        {
            options->state_path = argument;
            index++;
        }
        else if (strcmp(option, "--name") == 0)
        {
            *initial_name = argument;
            index++;
        }
        else if (strcmp(option, "--model") == 0)
        {
            options->model = argument;
            index++;
        }
        else if (strcmp(option, "--address") == 0)
        {
            options->address_override = argument;
            index++;
        }
        else if (strcmp(option, "--device-id") == 0)
        {
            if (!parse_device_id(argument, &options->device_id_override))
                return false;
            options->device_id_override_set = true;
            index++;
        }
        else if (strcmp(option, "--button-chip") == 0)
        {
            options->button_chip = argument;
            index++;
        }
        else if (strcmp(option, "--led-chip") == 0)
        {
            options->led_chip = argument;
            index++;
        }
        else if (strcmp(option, "--button-line") == 0)
        {
            if (!parse_uint(argument, &options->button_line))
                return false;
            index++;
        }
        else if (strcmp(option, "--led-line") == 0)
        {
            if (!parse_uint(argument, &options->led_line))
                return false;
            index++;
        }
        else
            return false;
    }
    return strlen(options->interface_name) < IFNAMSIZ && strlen(options->model) > 0U &&
           strlen(options->model) <= 32U && strlen(*initial_name) > 0U &&
           strlen(*initial_name) < 64U;
}

static uint64_t fnv1a64(const uint8_t *data, size_t length)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < length; index++)
    {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool read_uid_file(const char *path, uint8_t uid[12])
{
    const char *offset_text = getenv("FMD_UID_OFFSET");
    unsigned long offset = 0x210UL;
    char *end = NULL;
    int descriptor;
    ssize_t count;
    if (offset_text != NULL && *offset_text != '\0')
    {
        errno = 0;
        offset = strtoul(offset_text, &end, 0);
        if (errno != 0 || end == offset_text || *end != '\0')
            return false;
    }
    descriptor = open(path, O_RDONLY);
    if (descriptor < 0)
        return false;
    if (lseek(descriptor, (off_t)offset, SEEK_SET) < 0)
    {
        close(descriptor);
        return false;
    }
    count = read(descriptor, uid, 12U);
    close(descriptor);
    return count == 12;
}

static bool read_stm32_uid(uint8_t uid[12])
{
    const char *override = getenv("FMD_UID_PATH");
    const char *root = "/sys/bus/nvmem/devices";
    const char provider_prefix[] = "stm32-romem";
    DIR *directory;
    struct dirent *entry;
    char path[384];
    bool found = false;
    if (override != NULL && *override != '\0')
        return read_uid_file(override, uid);
    directory = opendir(root);
    if (directory == NULL)
        return false;
    while ((entry = readdir(directory)) != NULL)
    {
        int length;
        if (strncmp(entry->d_name, provider_prefix, sizeof(provider_prefix) - 1U) != 0)
            continue;
        length = snprintf(path, sizeof(path), "%s/%s/nvmem", root, entry->d_name);
        if (length <= 0 || (size_t)length >= sizeof(path))
            continue;
        if (read_uid_file(path, uid))
        {
            found = true;
            break;
        }
    }
    closedir(directory);
    return found;
}

static bool get_interface_mac(const char *interface_name, uint8_t mac[6])
{
    struct ifreq request;
    int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptor < 0)
        return false;
    memset(&request, 0, sizeof(request));
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", interface_name);
    if (ioctl(descriptor, SIOCGIFHWADDR, &request) != 0)
    {
        close(descriptor);
        return false;
    }
    memcpy(mac, request.ifr_hwaddr.sa_data, 6U);
    close(descriptor);
    return (mac[0] & 1U) == 0U;
}

static bool get_device_id(const Options *options, uint64_t *device_id)
{
    uint8_t uid[12];
    uint8_t mac[6];
    size_t index;
    if (options->device_id_override_set)
    {
        *device_id = options->device_id_override;
        return true;
    }
    if (read_stm32_uid(uid))
    {
        *device_id = fnv1a64(uid, sizeof(uid));
        return *device_id != 0U;
    }
    if (!get_interface_mac(options->interface_name, mac))
        return false;
    *device_id = 0U;
    for (index = 0U; index < sizeof(mac); index++)
        *device_id = (*device_id << 8U) | mac[index];
    return *device_id != 0U;
}

static void derive_mac(uint64_t device_id, uint8_t mac[6])
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < sizeof(device_id); index++)
    {
        hash ^= (uint8_t)(device_id >> (index * 8U));
        hash *= UINT64_C(1099511628211);
    }
    hash = (hash ^ (hash >> 44U)) & ((UINT64_C(1) << 44U) - 1U);
    mac[0] = (uint8_t)(((hash >> 40U) << 4U) | 0x02U);
    mac[1] = (uint8_t)(hash >> 32U);
    mac[2] = (uint8_t)(hash >> 24U);
    mac[3] = (uint8_t)(hash >> 16U);
    mac[4] = (uint8_t)(hash >> 8U);
    mac[5] = (uint8_t)hash;
}

static bool set_interface_mac(const char *interface_name, const uint8_t mac[6])
{
    struct ifreq request;
    short original_flags;
    int descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptor < 0)
        return false;
    memset(&request, 0, sizeof(request));
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", interface_name);
    if (ioctl(descriptor, SIOCGIFFLAGS, &request) != 0)
    {
        close(descriptor);
        return false;
    }
    original_flags = request.ifr_flags;
    request.ifr_flags &= (short)~IFF_UP;
    if (ioctl(descriptor, SIOCSIFFLAGS, &request) != 0)
    {
        close(descriptor);
        return false;
    }
    memset(&request.ifr_hwaddr, 0, sizeof(request.ifr_hwaddr));
    request.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(request.ifr_hwaddr.sa_data, mac, 6U);
    if (ioctl(descriptor, SIOCSIFHWADDR, &request) != 0)
    {
        close(descriptor);
        return false;
    }
    request.ifr_flags = original_flags;
    (void)ioctl(descriptor, SIOCSIFFLAGS, &request);
    close(descriptor);
    return true;
}

static bool get_interface_address(const Options *options, struct in_addr *address)
{
    struct ifreq request;
    int descriptor;
    if (options->address_override != NULL)
        return inet_pton(AF_INET, options->address_override, address) == 1;
    descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (descriptor < 0)
        return false;
    memset(&request, 0, sizeof(request));
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", options->interface_name);
    if (ioctl(descriptor, SIOCGIFADDR, &request) != 0)
    {
        close(descriptor);
        return false;
    }
    *address = ((struct sockaddr_in *)&request.ifr_addr)->sin_addr;
    close(descriptor);
    return true;
}

static bool save_persistent_state(Application *application)
{
    char temporary[512];
    FILE *file;
    int length;
    bool ok;
    application->persistent.magic = STATE_MAGIC;
    application->persistent.version = STATE_VERSION;
    application->persistent.size = (uint16_t)sizeof(application->persistent);
    snprintf(application->persistent.device_name, sizeof(application->persistent.device_name), "%s",
             application->device_name);
    length = snprintf(temporary, sizeof(temporary), "%s.tmp", application->options.state_path);
    if (length <= 0 || (size_t)length >= sizeof(temporary))
        return false;
    file = fopen(temporary, "wb");
    if (file == NULL)
        return false;
    ok = fwrite(&application->persistent, sizeof(application->persistent), 1U, file) == 1U;
    if (ok)
        ok = fflush(file) == 0;
    if (fclose(file) != 0)
        ok = false;
    if (ok)
        ok = rename(temporary, application->options.state_path) == 0;
    if (!ok)
        (void)unlink(temporary);
    return ok;
}

static void load_persistent_state(Application *application, const char *initial_name)
{
    FILE *file;
    PersistentState state;
    memset(&application->persistent, 0, sizeof(application->persistent));
    snprintf(application->device_name, sizeof(application->device_name), "%s", initial_name);
    file = fopen(application->options.state_path, "rb");
    if (file == NULL)
        return;
    if (fread(&state, sizeof(state), 1U, file) == 1U && state.magic == STATE_MAGIC &&
        state.version == STATE_VERSION && state.size == sizeof(state) &&
        memchr(state.device_name, '\0', sizeof(state.device_name)) != NULL &&
        state.device_name[0] != '\0')
    {
        application->persistent = state;
        snprintf(application->device_name, sizeof(application->device_name), "%s",
                 state.device_name);
    }
    fclose(file);
}

static bool oauth_load(void *context, OAuthPersistentState *state)
{
    Application *application = context;
    if (application->persistent.magic != STATE_MAGIC)
        return false;
    *state = application->persistent.oauth;
    return true;
}

static bool oauth_save(void *context, const OAuthPersistentState *state)
{
    Application *application = context;
    application->persistent.oauth = *state;
    return save_persistent_state(application);
}

static bool random_u32(uint32_t *value)
{
    int descriptor;
    ssize_t count;
    if (value == NULL)
        return false;
    descriptor = open("/dev/urandom", O_RDONLY);
    if (descriptor >= 0)
    {
        count = read(descriptor, value, sizeof(*value));
        close(descriptor);
        if (count == (ssize_t)sizeof(*value))
            return true;
    }
    *value = (uint32_t)monotonic_ms() ^ (uint32_t)getpid();
    *value ^= *value << 13;
    *value ^= *value >> 17;
    *value ^= *value << 5;
    return true;
}

static int request_gpio_line(const char *path, unsigned int offset, uint64_t flags,
                             const char *consumer)
{
    struct gpio_v2_line_request request;
    int chip;
    memset(&request, 0, sizeof(request));
    chip = open(path, O_RDONLY);
    if (chip < 0)
        return -1;
    request.offsets[0] = offset;
    request.num_lines = 1U;
    request.config.flags = flags;
    snprintf(request.consumer, sizeof(request.consumer), "%s", consumer);
    if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request) != 0)
    {
        close(chip);
        return -1;
    }
    close(chip);
    return request.fd;
}

static bool gpio_read(int descriptor, bool *high)
{
    struct gpio_v2_line_values values = {.mask = 1U};
    if (ioctl(descriptor, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) != 0)
        return false;
    *high = (values.bits & 1U) != 0U;
    return true;
}

static bool gpio_write(int descriptor, bool high)
{
    struct gpio_v2_line_values values = {.mask = 1U, .bits = high ? 1U : 0U};
    return ioctl(descriptor, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) == 0;
}

static void controls_init(Application *application)
{
    PlatformControls *controls = &application->controls;
    controls->button_fd = -1;
    controls->led_fd = -1;
    if (!application->options.gpio_enabled)
        return;
    controls->button_fd = request_gpio_line(
        application->options.button_chip, application->options.button_line,
        GPIO_V2_LINE_FLAG_INPUT |
            (application->options.button_active_low ? GPIO_V2_LINE_FLAG_BIAS_PULL_UP
                                                    : GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN),
        "find-my-device-button");
    if (controls->button_fd < 0)
        fprintf(stderr, "find_my_device_button_unavailable chip=%s line=%u error=%s\n",
                application->options.button_chip, application->options.button_line,
                strerror(errno));
    controls->led_fd =
        request_gpio_line(application->options.led_chip, application->options.led_line,
                          GPIO_V2_LINE_FLAG_OUTPUT, "find-my-device-led");
    if (controls->led_fd < 0)
        fprintf(stderr, "find_my_device_led_unavailable chip=%s line=%u error=%s\n",
                application->options.led_chip, application->options.led_line, strerror(errno));
    else
        (void)gpio_write(controls->led_fd, false);
}

static bool interface_link_up(const char *interface_name)
{
    char path[128];
    char value = '0';
    int descriptor;
    int length = snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", interface_name);
    if (length <= 0 || (size_t)length >= sizeof(path))
        return false;
    descriptor = open(path, O_RDONLY);
    if (descriptor < 0)
        return false;
    if (read(descriptor, &value, 1U) != 1)
        value = '0';
    close(descriptor);
    return value == '1';
}

static void trigger_flashes(Application *application, unsigned int count)
{
    PlatformControls *controls = &application->controls;
    if (count == 0U)
        return;
    controls->flash_toggles = count * 2U;
    controls->flash_level =
        controls->confirmation_flashes != 0U ? controls->confirmation_level : controls->link_up;
    application->controls.flash_next_ms = monotonic_ms();
}

static void set_confirmation_pattern(Application *application, unsigned int flashes)
{
    PlatformControls *controls = &application->controls;
    controls->confirmation_flashes = flashes;
    controls->confirmation_flashes_completed = 0U;
    controls->confirmation_level = flashes != 0U ? true : controls->link_up;
    controls->confirmation_next_ms = monotonic_ms() + LED_FLASH_MS;
    controls->flash_toggles = 0U;
    if (controls->led_fd >= 0)
        (void)gpio_write(controls->led_fd, controls->confirmation_level);
    printf("find_my_device_confirmation_pattern=%s\n",
           flashes == 1U ? "one-flash" : flashes == 2U ? "two-flash" : "idle");
    fflush(stdout);
}

static void confirm_physical(Application *application)
{
    if (!application->oauth.confirmation_waiting)
        return;
    oauth_confirm_physical(&application->oauth);
    if (application->oauth.physical_confirmed)
    {
        set_confirmation_pattern(application, 2U);
        fputs("find_my_device_physical_confirmation=accepted\n", stdout);
        fflush(stdout);
    }
}

static void controls_service(Application *application)
{
    PlatformControls *controls = &application->controls;
    uint64_t now = monotonic_ms();
    bool button_high;
    bool button_pressed;
    bool link_up = interface_link_up(application->options.interface_name);
    if (link_up != controls->link_up)
    {
        controls->link_up = link_up;
        printf("find_my_device_link=%s\n", link_up ? "up" : "down");
    }
    if (controls->button_fd >= 0 && gpio_read(controls->button_fd, &button_high))
    {
        button_pressed = application->options.button_active_low ? !button_high : button_high;
        if (button_pressed == controls->button_last)
            controls->button_stable_count++;
        else
        {
            controls->button_last = button_pressed;
            controls->button_stable_count = 1U;
        }
        if (button_pressed && controls->button_stable_count == 2U)
            confirm_physical(application);
    }
    if (controls->led_fd < 0)
        return;
    if (controls->flash_toggles != 0U)
    {
        if (now >= controls->flash_next_ms)
        {
            controls->flash_level = !controls->flash_level;
            controls->flash_toggles--;
            controls->flash_next_ms = now + LED_FLASH_MS;
            (void)gpio_write(controls->led_fd, controls->flash_level);
        }
        return;
    }
    if (controls->confirmation_flashes != 0U)
    {
        if (now >= controls->confirmation_next_ms)
        {
            if (controls->confirmation_level)
            {
                controls->confirmation_level = false;
                controls->confirmation_flashes_completed++;
                controls->confirmation_next_ms =
                    now + (controls->confirmation_flashes_completed >=
                                   controls->confirmation_flashes
                               ? LED_PATTERN_PAUSE_MS
                               : LED_FLASH_MS);
            }
            else
            {
                if (controls->confirmation_flashes_completed >=
                    controls->confirmation_flashes)
                    controls->confirmation_flashes_completed = 0U;
                controls->confirmation_level = true;
                controls->confirmation_next_ms = now + LED_FLASH_MS;
            }
            (void)gpio_write(controls->led_fd, controls->confirmation_level);
        }
        return;
    }
    (void)gpio_write(controls->led_fd,
                     controls->link_up ? true : (now / 500U) % 2U != 0U);
}

static bool send_all(int descriptor, const void *data_value, size_t length)
{
    const uint8_t *data = data_value;
    while (length != 0U)
    {
        ssize_t sent = send(descriptor, data, length, MSG_NOSIGNAL);
        if (sent <= 0)
            return false;
        data += sent;
        length -= (size_t)sent;
    }
    return true;
}

static ssize_t receive_request(int descriptor, char *buffer, size_t capacity)
{
    size_t total = 0U;
    size_t required = 0U;
    while (total + 1U < capacity)
    {
        ssize_t count = recv(descriptor, buffer + total, capacity - total - 1U, 0);
        char *headers_end;
        if (count <= 0)
            return count;
        total += (size_t)count;
        buffer[total] = '\0';
        headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end != NULL)
        {
            const char *cursor = buffer;
            unsigned long body_bytes = 0U;
            while ((cursor = strstr(cursor, "\r\n")) != NULL && cursor < headers_end)
            {
                cursor += 2;
                if (strncasecmp(cursor, "Content-Length:", 15U) == 0)
                    body_bytes = strtoul(cursor + 15U, NULL, 10);
            }
            required = (size_t)(headers_end + 4 - buffer) + (size_t)body_bytes;
            if (required >= capacity)
                return -1;
            if (total >= required)
                return (ssize_t)total;
        }
    }
    return -1;
}

static bool copy_bearer(const char *headers, char *output, size_t output_size)
{
    const char *line = headers;
    while (line != NULL && *line != '\0')
    {
        const char *end = strstr(line, "\r\n");
        size_t length = end == NULL ? strlen(line) : (size_t)(end - line);
        static const char prefix[] = "Authorization: Bearer ";
        if (length > sizeof(prefix) - 1U && strncasecmp(line, prefix, sizeof(prefix) - 1U) == 0)
        {
            size_t token_length = length - (sizeof(prefix) - 1U);
            if (token_length == 0U || token_length >= output_size)
                return false;
            memcpy(output, line + sizeof(prefix) - 1U, token_length);
            output[token_length] = '\0';
            return true;
        }
        line = end == NULL ? NULL : end + 2;
    }
    return false;
}

static bool copy_json_string(const char *json, const char *key, char *output, size_t output_size)
{
    char pattern[48];
    const char *value;
    size_t length = 0U;
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) <= 0)
        return false;
    value = strstr(json, pattern);
    if (value == NULL)
        return false;
    value += strlen(pattern);
    while (*value == ' ' || *value == '\t')
        value++;
    if (*value++ != ':')
        return false;
    while (*value == ' ' || *value == '\t')
        value++;
    if (*value++ != '\"')
        return false;
    while (value[length] != '\0' && value[length] != '\"')
    {
        unsigned char character = (unsigned char)value[length];
        if (character < 0x20U || character == '\\' || length + 1U >= output_size)
            return false;
        output[length] = value[length];
        length++;
    }
    if (value[length] != '\"' || length == 0U)
        return false;
    output[length] = '\0';
    return true;
}

static const char *status_text(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 303:
        return "See Other";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 500:
        return "Internal Server Error";
    default:
        return "Error";
    }
}

static bool send_http(int descriptor, int status, const char *content_type, const char *location,
                      const char *body, size_t body_length)
{
    char header[512];
    int length = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\n%s%s%s"
        "Content-Length: %u\r\nConnection: close\r\n\r\n",
        status, status_text(status), content_type,
        location == NULL || *location == '\0' ? "" : "Location: ", location == NULL ? "" : location,
        location == NULL || *location == '\0' ? "" : "\r\n", (unsigned int)body_length);
    if (length <= 0 || (size_t)length >= sizeof(header))
        return false;
    return send_all(descriptor, header, (size_t)length) &&
           (body_length == 0U || send_all(descriptor, body, body_length));
}

typedef struct
{
    uint32_t state[5];
    uint64_t bits;
    uint8_t block[64];
    size_t used;
} Sha1;
static uint32_t rol(uint32_t value, unsigned int count)
{
    return (value << count) | (value >> (32U - count));
}
static void sha1_block(Sha1 *context, const uint8_t block[64])
{
    uint32_t words[80], a, b, c, d, e, f, k, temporary;
    size_t index;
    for (index = 0U; index < 16U; index++)
        words[index] = ((uint32_t)block[index * 4U] << 24U) |
                       ((uint32_t)block[index * 4U + 1U] << 16U) |
                       ((uint32_t)block[index * 4U + 2U] << 8U) | block[index * 4U + 3U];
    for (index = 16U; index < 80U; index++)
        words[index] = rol(
            words[index - 3U] ^ words[index - 8U] ^ words[index - 14U] ^ words[index - 16U], 1U);
    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    for (index = 0U; index < 80U; index++)
    {
        if (index < 20U)
        {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        }
        else if (index < 40U)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        }
        else if (index < 60U)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }
        temporary = rol(a, 5U) + f + e + k + words[index];
        e = d;
        d = c;
        c = rol(b, 30U);
        b = a;
        a = temporary;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
}
static void sha1_update(Sha1 *context, const uint8_t *data, size_t length)
{
    context->bits += (uint64_t)length * 8U;
    while (length != 0U)
    {
        size_t count = 64U - context->used;
        if (count > length)
            count = length;
        memcpy(context->block + context->used, data, count);
        context->used += count;
        data += count;
        length -= count;
        if (context->used == 64U)
        {
            sha1_block(context, context->block);
            context->used = 0U;
        }
    }
}
static void sha1_digest(const uint8_t *data, size_t length, uint8_t digest[20])
{
    Sha1 context = {{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U}, 0U, {0}, 0U};
    uint8_t padding[72] = {0x80U};
    uint64_t bits;
    size_t padding_length, index;
    sha1_update(&context, data, length);
    bits = context.bits;
    padding_length = context.used < 56U ? 56U - context.used : 120U - context.used;
    sha1_update(&context, padding, padding_length);
    for (index = 0U; index < 8U; index++)
        padding[index] = (uint8_t)(bits >> (56U - index * 8U));
    sha1_update(&context, padding, 8U);
    for (index = 0U; index < 5U; index++)
    {
        digest[index * 4U] = (uint8_t)(context.state[index] >> 24U);
        digest[index * 4U + 1U] = (uint8_t)(context.state[index] >> 16U);
        digest[index * 4U + 2U] = (uint8_t)(context.state[index] >> 8U);
        digest[index * 4U + 3U] = (uint8_t)context.state[index];
    }
}
static void base64_encode(const uint8_t *input, size_t length, char *output)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t in = 0U, out = 0U;
    while (in < length)
    {
        uint32_t value = (uint32_t)input[in++] << 16U;
        bool second = in < length, third;
        if (second)
            value |= (uint32_t)input[in++] << 8U;
        third = in < length;
        if (third)
            value |= input[in++];
        output[out++] = alphabet[(value >> 18U) & 63U];
        output[out++] = alphabet[(value >> 12U) & 63U];
        output[out++] = second ? alphabet[(value >> 6U) & 63U] : '=';
        output[out++] = third ? alphabet[value & 63U] : '=';
    }
    output[out] = '\0';
}

static bool websocket_send_text(int descriptor, const char *text)
{
    uint8_t header[4] = {0x81U, 0U, 0U, 0U};
    size_t length = strlen(text), header_length = 2U;
    if (length > 65535U)
        return false;
    if (length < 126U)
        header[1] = (uint8_t)length;
    else
    {
        header[1] = 126U;
        header[2] = (uint8_t)(length >> 8U);
        header[3] = (uint8_t)length;
        header_length = 4U;
    }
    return send_all(descriptor, header, header_length) && send_all(descriptor, text, length);
}

static bool receive_exact(int descriptor, uint8_t *output, size_t length)
{
    while (length != 0U)
    {
        ssize_t count = recv(descriptor, output, length, 0);
        if (count <= 0)
            return false;
        output += count;
        length -= (size_t)count;
    }
    return true;
}

static bool websocket_receive_text(int descriptor, char *output, size_t output_size)
{
    uint8_t header[2], extended[2], mask[4], encoded[512];
    size_t length, index;
    if (!receive_exact(descriptor, header, 2U) || (header[0] & 0x80U) == 0U ||
        (header[0] & 0x0fU) != 1U || (header[1] & 0x80U) == 0U)
        return false;
    length = header[1] & 0x7fU;
    if (length == 126U)
    {
        if (!receive_exact(descriptor, extended, 2U))
            return false;
        length = ((size_t)extended[0] << 8U) | extended[1];
    }
    else if (length == 127U)
        return false;
    if (length >= output_size || length > sizeof(encoded) || !receive_exact(descriptor, mask, 4U) ||
        !receive_exact(descriptor, encoded, length))
        return false;
    for (index = 0U; index < length; index++)
        output[index] = (char)(encoded[index] ^ mask[index & 3U]);
    output[length] = '\0';
    return true;
}

static bool websocket_upgrade(Application *application, int client, const char *headers)
{
    const char *key = strcasestr(headers, "Sec-WebSocket-Key: ");
    const char *end;
    static const char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[96], accept[32], message[512], token[32], response[256];
    uint8_t digest[20];
    size_t key_length;
    int length;
    if (key == NULL)
        return false;
    key += 19U;
    end = strstr(key, "\r\n");
    if (end == NULL)
        return false;
    key_length = (size_t)(end - key);
    if (key_length == 0U || key_length + sizeof(magic) > sizeof(combined))
        return false;
    memcpy(combined, key, key_length);
    memcpy(combined + key_length, magic, sizeof(magic) - 1U);
    sha1_digest((const uint8_t *)combined, key_length + sizeof(magic) - 1U, digest);
    base64_encode(digest, 20U, accept);
    length = snprintf(response, sizeof(response),
                      "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: "
                      "Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",
                      accept);
    if (length <= 0 || (size_t)length >= sizeof(response) ||
        !send_all(client, response, (size_t)length) ||
        !websocket_send_text(client, "{\"type\":\"auth_required\"}"))
        return false;
    if (!websocket_receive_text(client, message, sizeof(message)) ||
        !copy_json_string(message, "access_token", token, sizeof(token)) ||
        !oauth_validate_access(&application->oauth, token, now_seconds(application)))
    {
        (void)websocket_send_text(
            client, "{\"type\":\"auth_invalid\",\"message\":\"Invalid access token\"}");
        return false;
    }
    if (!websocket_send_text(client, "{\"type\":\"auth_ok\"}") ||
        !websocket_receive_text(client, message, sizeof(message)) ||
        strstr(message, "\"type\":\"subscribe_status\"") == NULL ||
        !websocket_send_text(client, "{\"id\":1,\"type\":\"result\",\"success\":true}"))
        return false;
    if (application->websocket_socket >= 0)
        close(application->websocket_socket);
    application->websocket_socket = client;
    application->websocket_counter = 0U;
    application->websocket_next_ms = monotonic_ms();
    puts("find_my_device_websocket=ready");
    return true;
}

static unsigned long free_memory_bytes(void)
{
    struct sysinfo info;
    if (sysinfo(&info) != 0)
        return 1UL;
    return info.freeram * info.mem_unit;
}

static void service_websocket(Application *application)
{
    char event[320];
    int length;
    if (application->websocket_socket < 0 || monotonic_ms() < application->websocket_next_ms)
        return;
    length = snprintf(
        event, sizeof(event),
        "{\"type\":\"event\",\"event\":{\"event_type\":\"server_status\",\"data\":{\"uptime_"
        "seconds\":%u,\"counter\":%u,\"free_heap_bytes\":%lu,\"cpu_utilization_percent\":0}}}",
        (unsigned int)now_seconds(application), (unsigned int)++application->websocket_counter,
        free_memory_bytes());
    application->websocket_next_ms = monotonic_ms() + 2000U;
    if (length <= 0 || (size_t)length >= sizeof(event) ||
        !websocket_send_text(application->websocket_socket, event))
    {
        close(application->websocket_socket);
        application->websocket_socket = -1;
    }
}

static bool valid_device_name(const char *name)
{
    size_t index, length = strlen(name);
    if (length == 0U || length >= 64U)
        return false;
    for (index = 0U; index < length; index++)
        if ((unsigned char)name[index] < 0x20U || name[index] == '\"' || name[index] == '\\')
            return false;
    return true;
}

static void client_timeouts(int descriptor)
{
    struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};
    (void)setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static bool serve_client(Application *application, int client)
{
    char request[HTTP_BUFFER_BYTES], response[HTTP_BUFFER_BYTES], token[32];
    char *target, *line_end, *query = NULL, *headers, *body;
    ssize_t received;
    size_t body_length = 0U;
    int status = 200;
    client_timeouts(client);
    received = receive_request(client, request, sizeof(request));
    if (received <= 0)
        return false;
    request[received] = '\0';
    target = strchr(request, ' ');
    if (target == NULL || (line_end = strstr(target + 1, " HTTP/1.")) == NULL)
        return false;
    target++;
    *line_end = '\0';
    query = strchr(target, '?');
    if (query != NULL)
        *query++ = '\0';
    headers = strstr(line_end + 1, "\r\n");
    headers = headers == NULL ? line_end + 1 : headers + 2;
    body = strstr(headers, "\r\n\r\n");
    if (body != NULL)
        body += 4;
    if (strncmp(request, "GET ", 4U) == 0 && strcmp(target, "/api/info") == 0)
        body_length = device_info_build_json(&application->info, response, sizeof(response));
    else if (strncmp(request, "GET ", 4U) == 0 && strcmp(target, "/api/device-info") == 0)
    {
        if (!copy_bearer(headers, token, sizeof(token)) ||
            !oauth_validate_access(&application->oauth, token, now_seconds(application)))
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"A valid bearer token is required\"}}");
            status = 401;
        }
        else
            body_length = device_info_build_json(&application->info, response, sizeof(response));
        if (status == 401)
            body_length = strlen(response);
    }
    else if (strncmp(request, "GET ", 4U) == 0 && strcmp(target, "/auth/authorize") == 0)
    {
        OAuthResponse oauth_response;
        oauth_authorize(&application->oauth, query, application->device_name, &oauth_response);
        if (oauth_response.status == 200)
            set_confirmation_pattern(application, 1U);
        send_http(client, oauth_response.status, oauth_response.content_type,
                  oauth_response.location, oauth_response.body, oauth_response.body_length);
        return false;
    }
    else if (strncmp(request, "GET ", 4U) == 0 && strcmp(target, "/auth/confirmation/status") == 0)
    {
        OAuthResponse oauth_response;
        oauth_confirmation_status(&application->oauth, &oauth_response);
        send_http(client, oauth_response.status, oauth_response.content_type, NULL,
                  oauth_response.body, oauth_response.body_length);
        return false;
    }
    else if (strncmp(request, "POST ", 5U) == 0 &&
             (strcmp(target, "/auth/decision") == 0 || strcmp(target, "/auth/token") == 0) &&
             body != NULL)
    {
        OAuthResponse oauth_response;
        size_t form_length = (size_t)received - (size_t)(body - request);
        if (strcmp(target, "/auth/decision") == 0)
        {
            oauth_decide(&application->oauth, body, form_length, now_seconds(application),
                         &oauth_response);
            if (!application->oauth.confirmation_waiting)
                set_confirmation_pattern(application, 0U);
        }
        else
            oauth_token(&application->oauth, body, form_length, now_seconds(application),
                        &oauth_response);
        send_http(client, oauth_response.status, oauth_response.content_type,
                  oauth_response.location, oauth_response.body, oauth_response.body_length);
        return false;
    }
    else if (strncmp(request, "GET ", 4U) == 0 && strcmp(target, "/api/ws") == 0)
        return websocket_upgrade(application, client, headers);
    else if (strncmp(request, "POST ", 5U) == 0 && strcmp(target, "/api/led/flashes") == 0 &&
             body != NULL)
    {
        const char *count_value = strstr(body, "\"flash_count\"");
        char *count_end = NULL;
        unsigned long count = 0U;
        if (count_value != NULL)
            count_value = strchr(count_value, ':');
        if (count_value != NULL)
            count = strtoul(count_value + 1, &count_end, 10);
        if (!copy_bearer(headers, token, sizeof(token)) ||
            !oauth_validate_access(&application->oauth, token, now_seconds(application)))
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"A valid bearer token is required\"}}");
            status = 401;
        }
        else if (count_end == NULL || count < 1U || count > 3U)
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"flash_count must be 1, 2, or 3\"}}");
            status = 400;
        }
        else
        {
            trigger_flashes(application, (unsigned int)count);
            snprintf(response, sizeof(response), "{\"success\":true}");
        }
        body_length = strlen(response);
    }
    else if (strncmp(request, "PUT ", 4U) == 0 && strcmp(target, "/api/device-name") == 0 &&
             body != NULL)
    {
        char name[64];
        if (!copy_bearer(headers, token, sizeof(token)) ||
            !oauth_validate_access(&application->oauth, token, now_seconds(application)))
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"A valid bearer token is required\"}}");
            status = 401;
        }
        else if (!copy_json_string(body, "device_name", name, sizeof(name)) ||
                 !valid_device_name(name))
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"device_name must contain 1 to 63 characters\"}}");
            status = 400;
        }
        else
        {
            snprintf(application->device_name, sizeof(application->device_name), "%s", name);
            if (!save_persistent_state(application))
            {
                snprintf(response, sizeof(response),
                         "{\"error\":{\"message\":\"device name persistence failed\"}}");
                status = 500;
            }
            else
            {
                snprintf(response, sizeof(response), "{\"success\":true}");
                application->announce_now = true;
            }
        }
        body_length = strlen(response);
    }
    else if (strncmp(request, "POST ", 5U) == 0 &&
             strcmp(target, "/api/mobile/registrations") == 0 && body != NULL)
    {
        char mobile_id[96], ip[INET_ADDRSTRLEN];
        if (!copy_bearer(headers, token, sizeof(token)) ||
            !oauth_validate_access(&application->oauth, token, now_seconds(application)))
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"A valid bearer token is required\"}}");
            status = 401;
        }
        else if (!copy_json_string(body, "device_id", mobile_id, sizeof(mobile_id)))
        {
            snprintf(response, sizeof(response),
                     "{\"error\":{\"message\":\"A device_id JSON string is required\"}}");
            status = 400;
        }
        else
        {
            inet_ntop(AF_INET, &application->address, ip, sizeof(ip));
            snprintf(
                response, sizeof(response),
                "{\"registration_id\":\"mobile-registration-001\",\"device_id\":\"%s\",\"websocket_"
                "url\":\"ws://%s:%u/api/ws\",\"registered_at\":\"1970-01-01T00:00:%02uZ\"}",
                mobile_id, ip, application->options.port,
                (unsigned int)(now_seconds(application) % 60U));
        }
        body_length = strlen(response);
    }
    else
    {
        send_http(client, 404, "application/json", NULL, "", 0U);
        return false;
    }
    if (body_length == 0U)
    {
        send_http(client, 500, "application/json", NULL, "", 0U);
        return false;
    }
    send_http(client, status, "application/json", NULL, response, body_length);
    return false;
}

static int create_http_socket(uint16_t port)
{
    struct sockaddr_in address;
    int descriptor, one = 1;
    descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0)
        return -1;
    (void)setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(descriptor, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(descriptor, 5) != 0)
    {
        close(descriptor);
        return -1;
    }
    return descriptor;
}

static int create_mdns_socket(const struct in_addr *interface_address)
{
    struct sockaddr_in bind_address;
    struct ip_mreq membership;
    int descriptor, one = 1, ttl = 255;
    descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor < 0)
        return -1;
    (void)setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&bind_address, 0, sizeof(bind_address));
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(MDNS_PORT);
    bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(descriptor, (struct sockaddr *)&bind_address, sizeof(bind_address)) != 0)
    {
        close(descriptor);
        return -1;
    }
    membership.imr_multiaddr.s_addr = inet_addr(MDNS_GROUP);
    membership.imr_interface = *interface_address;
    if (setsockopt(descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) !=
            0 ||
        setsockopt(descriptor, IPPROTO_IP, IP_MULTICAST_IF, interface_address,
                   sizeof(*interface_address)) != 0)
    {
        close(descriptor);
        return -1;
    }
    (void)setsockopt(descriptor, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    return descriptor;
}

static void mdns_send(Application *application, const uint8_t *packet, size_t length)
{
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(MDNS_PORT);
    destination.sin_addr.s_addr = inet_addr(MDNS_GROUP);
    if (sendto(application->mdns_socket, packet, length, 0, (struct sockaddr *)&destination,
               sizeof(destination)) != (ssize_t)length)
        perror("mdns sendto");
}

static void mdns_announce(Application *application, uint32_t ttl)
{
    uint8_t packet[MDNS_BUFFER_BYTES];
    size_t length = mdns_build_announcement(&application->mdns, ttl, packet, sizeof(packet));
    if (length != 0U)
        mdns_send(application, packet, length);
}

static void mdns_receive(Application *application)
{
    uint8_t input[MDNS_BUFFER_BYTES], output[MDNS_BUFFER_BYTES];
    ssize_t received = recv(application->mdns_socket, input, sizeof(input), 0);
    if (received > 0)
    {
        size_t length = mdns_build_response(&application->mdns, input, (size_t)received, output,
                                            sizeof(output));
        if (length != 0U)
            mdns_send(application, output, length);
    }
}

static void reload_interface_address(Application *application)
{
    struct in_addr address;
    char printable[INET_ADDRSTRLEN];
    int descriptor;

    if (!get_interface_address(&application->options, &address) ||
        address.s_addr == application->address.s_addr)
        return;
    descriptor = create_mdns_socket(&address);
    if (descriptor < 0)
    {
        perror("find-my-device mDNS address reload");
        return;
    }
    mdns_announce(application, 0U);
    close(application->mdns_socket);
    application->mdns_socket = descriptor;
    application->address = address;
    memcpy(application->mdns.ipv4, &address.s_addr, sizeof(application->mdns.ipv4));
    application->announce_now = true;
    if (inet_ntop(AF_INET, &address, printable, sizeof(printable)) != NULL)
        printf("find_my_device_ipv4=%s source=dhcp-update\n", printable);
    fflush(stdout);
}

static int application_run(Application *application)
{
    uint64_t next_announcement = 0U;
    unsigned int initial_announcements = 0U;
    controls_init(application);
    application->http_socket = create_http_socket(application->options.port);
    application->mdns_socket = create_mdns_socket(&application->address);
    application->service_timer_fd = create_service_timer();
    application->websocket_socket = -1;
    if (application->http_socket < 0 || application->mdns_socket < 0 ||
        application->service_timer_fd < 0)
    {
        perror("find-my-device sockets");
        if (application->http_socket >= 0)
            close(application->http_socket);
        if (application->mdns_socket >= 0)
            close(application->mdns_socket);
        if (application->service_timer_fd >= 0)
            close(application->service_timer_fd);
        return 1;
    }
    printf("device_info_ready port=%u device_id=%s\n", application->options.port,
           application->device_id);
    printf("mdns_ready service=%s host=%s\n", application->instance_name, application->host_name);
    fflush(stdout);
    while (!stop_requested)
    {
        struct pollfd descriptors[4];
        nfds_t count = 3U;
        int result;
        uint64_t now = monotonic_ms();
        descriptors[0].fd = application->http_socket;
        descriptors[0].events = POLLIN;
        descriptors[0].revents = 0;
        descriptors[1].fd = application->mdns_socket;
        descriptors[1].events = POLLIN;
        descriptors[1].revents = 0;
        descriptors[2].fd = application->service_timer_fd;
        descriptors[2].events = POLLIN;
        descriptors[2].revents = 0;
        if (application->websocket_socket >= 0)
        {
            descriptors[3].fd = application->websocket_socket;
            descriptors[3].events = POLLERR | POLLHUP;
            descriptors[3].revents = 0;
            count = 4U;
        }
        result = poll(descriptors, count, 50);
        if (result < 0 && errno != EINTR)
        {
            perror("poll");
            break;
        }
        if (descriptors[0].revents & POLLIN)
        {
            int client = accept(application->http_socket, NULL, NULL);
            if (client >= 0)
            {
                bool keep = serve_client(application, client);
                if (!keep)
                    close(client);
            }
        }
        if (descriptors[1].revents & POLLIN)
            mdns_receive(application);
        if (descriptors[2].revents & POLLIN)
        {
            uint64_t expirations;
            (void)read(application->service_timer_fd, &expirations, sizeof(expirations));
        }
        if (count == 4U && (descriptors[3].revents & (POLLERR | POLLHUP)))
        {
            close(application->websocket_socket);
            application->websocket_socket = -1;
        }
        if (confirm_requested)
        {
            confirm_requested = 0;
            confirm_physical(application);
        }
        if (address_reload_requested)
        {
            address_reload_requested = 0;
            reload_interface_address(application);
        }
        controls_service(application);
        service_websocket(application);
        now = monotonic_ms();
        if (application->announce_now || now >= next_announcement)
        {
            mdns_announce(application, 120U);
            application->announce_now = false;
            if (initial_announcements++ < 1U)
                next_announcement = now + 1000U;
            else
                next_announcement = now + 120000U;
        }
    }
    mdns_announce(application, 0U);
    usleep(100000U);
    mdns_announce(application, 0U);
    if (application->websocket_socket >= 0)
        close(application->websocket_socket);
    close(application->http_socket);
    close(application->mdns_socket);
    close(application->service_timer_fd);
    if (application->controls.button_fd >= 0)
        close(application->controls.button_fd);
    if (application->controls.led_fd >= 0)
    {
        (void)gpio_write(application->controls.led_fd, false);
        close(application->controls.led_fd);
    }
    return 0;
}

int main(int argc, char **argv)
{
    Application application;
    const char *initial_name;
    uint64_t device_id;
    uint8_t mac[6];
    memset(&application, 0, sizeof(application));
    application.http_socket = -1;
    application.mdns_socket = -1;
    application.service_timer_fd = -1;
    application.websocket_socket = -1;
    if (!parse_options(argc, argv, &application.options, &initial_name))
    {
        usage(argv[0]);
        return 2;
    }
    if (!get_device_id(&application.options, &device_id))
    {
        fprintf(stderr, "Cannot derive the STM32 device identity.\n");
        return 2;
    }
    derive_mac(device_id, mac);
    if (application.options.prepare_interface)
    {
        if (!set_interface_mac(application.options.interface_name, mac))
        {
            fprintf(stderr, "Cannot set %s MAC address: %s\n", application.options.interface_name,
                    strerror(errno));
            return 1;
        }
        printf("find_my_device_mac=%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3],
               mac[4], mac[5]);
        return 0;
    }
    if (!get_interface_address(&application.options, &application.address))
    {
        fprintf(stderr, "Interface %s has no IPv4 address.\n", application.options.interface_name);
        return 2;
    }
    application.start_ms = monotonic_ms();
    snprintf(application.device_id, sizeof(application.device_id), "%016llx",
             (unsigned long long)device_id);
    load_persistent_state(&application, initial_name);
    oauth_service_init(&application.oauth, random_u32);
    (void)oauth_service_restore(&application.oauth, oauth_load, oauth_save, &application, 0U);
    if (!mdns_build_service_names(application.options.model, "stm32f429-linux",
                                  application.device_id, application.instance_name,
                                  sizeof(application.instance_name), application.host_name,
                                  sizeof(application.host_name)))
    {
        fprintf(stderr, "Cannot build mDNS names.\n");
        return 2;
    }
    application.info.device_id = application.device_id;
    application.info.device_name = application.device_name;
    application.info.device_model = application.options.model;
    application.mdns.service_type = MDNS_SERVICE_TYPE;
    application.mdns.instance_name = application.instance_name;
    application.mdns.host_name = application.host_name;
    application.mdns.device_id = application.device_id;
    application.mdns.name = application.device_name;
    application.mdns.model = application.options.model;
    memcpy(application.mdns.ipv4, &application.address.s_addr, 4U);
    application.mdns.port = application.options.port;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    return application_run(&application);
}
