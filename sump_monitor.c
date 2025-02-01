/*
Measure distance using an HC-SR04 sensor using event monitoring.

This iteration will eschew the contextless operations which seem to
produce inconsistent results.

Starting point will be some of the code from
https://github.com/HankB/GPIOD_Debian_Raspberry_Pi/blob/main/line_IO/read_write.c

Build:
    gcc -Wall -o sump_monitor sump_monitor.c  -l gpiod
*/

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <gpiod.h>
#include <stdint.h>
#include <string.h>

static int send_pulse(struct gpiod_line *);
static void sleep_us(unsigned long microseconds);
static float median(float values[], int count);
static int report_depth(float depth);

typedef enum
{
    read_line,
    write_line,
    monitor_line,
} line_function;

static struct gpiod_line *trigger_line;
static struct gpiod_line *echo_line;

struct gpiod_line *init_GPIO(struct gpiod_chip *chip,
                             const char *name,
                             const char *context,
                             line_function function, int flags);

static const char *cnsmr = "HC-SR04";                 // declare a consumer to use for calls
static const char *GPIO_chip_name = "/dev/gpiochip0"; // name of trigger GPIO (output)
static const char *trigger_name = "GPIO23";           // name of trigger GPIO (output)
static const char *echo_name = "GPIO24";              // name of echo GPIO input

static struct timespec start = {0.0};  // start of echo pulse
static struct timespec finish = {0.0}; // end of echo pulse

#define debug_lvl 0 // control chattiness

int main(int argc, char **argv)
{
    // need to open the chip first
    struct gpiod_chip *chip = gpiod_chip_open(GPIO_chip_name);
    if (chip == NULL)
    {
        perror("gpiod_chip_open()");
        return 1;
    }
    // acquire & configure GPIO 23 (trigger) for output
    // struct gpiod_line *trigger_line;
    trigger_line = init_GPIO(chip, trigger_name, cnsmr, write_line, 0);
    if (0 == trigger_line)
    {
        perror("               init_GPIO(trigger_line)");
        gpiod_chip_close(chip);
        return 1;
    }
    // acquire & configure GPIO 24 (echo) for input

    // struct gpiod_line *echo_line;

    echo_line = init_GPIO(chip, echo_name, cnsmr, monitor_line, 0);
    if (0 == echo_line)
    {
        perror("init_GPIO(echo_name)");
        gpiod_line_release(trigger_line);
        gpiod_chip_close(chip);
        return 1;
    }

    int readings_needed = 5;
    float readings[readings_needed];

    int reading_count = 0;
    bool need_pulse = true;

    while (reading_count < readings_needed)
    {
        if (need_pulse)
        {
            sleep_us(60 * 1000);               // delay 60 microseconda per recommendation - try 100 ms
            int rc = send_pulse(trigger_line); // send the trigger pulse
            if (0 != rc)
            {
                perror("send_pulse(trigger_line)");
                gpiod_line_release(trigger_line);
                gpiod_line_release(echo_line);
                gpiod_chip_close(chip);
                return -1;
            }
            need_pulse = false;
        }

        const struct timespec timeout = {0L, 1000000L}; // 1000000ns, 0s
        int rc = gpiod_line_event_wait(echo_line, &timeout);
        if (rc < 0)
        {
            perror("gpiod_line_event_wait(echo_line)");
            gpiod_line_release(trigger_line);
            gpiod_line_release(echo_line);
            gpiod_chip_close(chip);
            return -1;
        }
        else if (rc == 0)
        {
            need_pulse = true;
        }
        else
        {
            struct gpiod_line_event event;
            rc = gpiod_line_event_read(echo_line, &event);
            if (rc < 0)
            {
                perror("gpiod_line_event_read(gpio_11, &event)");
            }
            else
            {
                switch (event.event_type)
                {
                case GPIOD_LINE_EVENT_RISING_EDGE:
                {
                    start = event.ts;
                    break;
                }
                case GPIOD_LINE_EVENT_FALLING_EDGE:
                {
                    if (start.tv_sec != 0) // if we didn't miss the start of the pulse
                    {
                        finish = event.ts;
                        float pulse_width = ((float)(finish.tv_nsec - start.tv_nsec) / 1000000000) + (finish.tv_sec - start.tv_sec);
                        readings[reading_count] = pulse_width * 1100 * 12 / 2.0; // distance in inches based on 1100 fps in air
                        //readings[reading_count] = pulse_width * 34300 / 2.0; // distance in cm based on 343 m/s in air
                        if (debug_lvl > 0)
                            printf("%f, %d, %f\n", pulse_width, reading_count, readings[reading_count]);
                        start.tv_sec = 0; // zero our for next reading
                        reading_count++;
                    }
                    need_pulse = true;
                    break;
                }
                default:
                {
                    fprintf(stderr, "\t\t\tUnknown event.event_type %d\n", event.event_type);
                    break;
                }
                }
            }
        }
    }

    float median_distance = median(readings, readings_needed);
    if (debug_lvl > 0)
        printf("median %f\n", median_distance);

    report_depth(median_distance);
    gpiod_line_release(trigger_line);
    gpiod_line_release(echo_line);
    gpiod_chip_close(chip);
}

struct gpiod_line *init_GPIO(struct gpiod_chip *chip,
                             const char *name,
                             const char *context,
                             line_function func, int flags)
{
    struct gpiod_line *line;

    line = gpiod_chip_find_line(chip, name);
    if (line == NULL)
    {
        return 0;
    }

    if (func == read_line || func == write_line)
    {
        const struct gpiod_line_request_config config =
            {context,
             (func == write_line) ? GPIOD_LINE_REQUEST_DIRECTION_OUTPUT : GPIOD_LINE_REQUEST_DIRECTION_INPUT,
             flags};

        int rc = gpiod_line_request(line,
                                    &config,
                                    0);
        if (0 != rc)
        {
            return 0;
        }
    }
    else if (func == monitor_line)
    {
        // register to read events
        int rc = gpiod_line_request_both_edges_events(line, cnsmr);
        if (0 > rc)
        {
            return 0;
        }
    }
    return line;
}

static int send_pulse(struct gpiod_line *line)
{
    int rc = gpiod_line_set_value(line, 1);
    if (rc < 0)
    {
        perror("               gpiod_line_set_value(line,1)");
        return 1;
    }

    sleep_us(10); // delay 10 microseconds

    rc = gpiod_line_set_value(line, 0);
    if (rc < 0)
    {
        perror("               gpiod_line_get_value(line,0)");
        return 1;
    }
    return 0;
}

static void sleep_us(unsigned long microseconds)
{
    struct timespec ts;
    ts.tv_sec = microseconds / 1000000ul;           // whole seconds
    ts.tv_nsec = (microseconds % 1000000ul) * 1000; // remainder, in nanoseconds
    nanosleep(&ts, NULL);
}

// code to find the median value
// https://stackoverflow.com/questions/1961173/median-function-in-c-math-library

static int compare(const void *a, const void *b)
{
    return (*(float *)a - *(float *)b);
}

static float median(float values[], int count)
{
    qsort(values, count, sizeof(float), compare);
    return values[count / 2];
}

// format results as JSON and output
static int report_depth(float depth)
{
    time_t ts = time(0);

    printf( "{\"t\":%ld, \"depth\":%.1f, \"sensor\":\"HC-SR04\"}",
             ts, depth);
    return 0;
}
