/*
Build
g++ -Wall -o gpio_mqtt_publish proj_gpio.cpp  proj_mqtt.cpp \
    -lgpiodcxx -lpaho-mqttpp3 -lpaho-mqtt3as -lpthread
*/

#include <iostream>
using namespace std;
#include <gpiod.hpp>
#include <unistd.h>
#include <limits.h> // For HOST_NAME_MAX
#include <cstring>
#include <chrono>

#include "proj_mqtt.h"

namespace
{

    /* Example configuration - customize to suit your situation. */
    const ::filesystem::path chip_path("/dev/gpiochip0");
    const ::gpiod::line::offset input_line_offset = 23;
    const ::gpiod::line::offset output_line_offset = 24;
    auto timeout = ::chrono::seconds(900); // timeout in seconds
    const char *consumer = "C++ follower";
} /* namespace */

int main(int argc, char **argv)
{
    if (7 != argc)
    {
        cerr << "Usage: "
             << argv[0]
             << " broker_uri location measurement device gpio_in gpio_out"
             << endl;
        return -1;
    }

    /*
    unpack positional input parms
    gpio_mqtt_publish broker_uri location measurement device gpio_in gpio_out
    */
    const ::string broker(argv[1]);
    const ::string location(argv[2]);
    const ::string measurement(argv[3]);
    const ::string device(argv[4]);
    const ::gpiod::line::offset gpio_in = stoi(argv[5]);
    const ::gpiod::line::offset gpio_out = stoi(argv[6]);

    // build topic
    // "HA/{hostnmame}/{location}/{measurement}"
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        // Ensure null termination in case of truncation, although gethostname usually handles this
        hostname[sizeof(hostname) - 1] = '\0';
    }
    else
    {
        perror("gethostname");
        return -1;
    }

    const ::string topic("HA/" + string(hostname) + "/" + location + "/" + measurement);
    // chip object

    gpiod::chip chip("/dev/gpiochip0");
    if (!chip)
    {
        cout << "chip not constructed" << endl;
        exit(-1);
    }
    gpiod::chip_info info = chip.get_info();

    gpiod::edge_event_buffer events;
    // input processing from the GPIOD example watch_line_rising.cpp
    // except we want both rising and falling events and a timeout.

    auto input_request =
        chip.prepare_request()
            .set_consumer(consumer)
            .add_line_settings(
                gpio_in,
                ::gpiod::line_settings()
                    .set_direction(
                        ::gpiod::line::direction::INPUT)
                    .set_bias(
                        ::gpiod::line::bias::PULL_UP)
                    .set_edge_detection(
                        ::gpiod::line::edge::BOTH))
            .do_request();

    // output processing copied substantially from toggle_line_value.cpp

    auto output_request =
        ::gpiod::chip(chip_path)
            .prepare_request()
            .set_consumer(consumer)
            .add_line_settings(
                gpio_out,
                ::gpiod::line_settings().set_direction(
                    ::gpiod::line::direction::OUTPUT))
            .do_request();

    // initialize the output to match the input
    output_request.set_value(gpio_out,
                             input_request.get_value(gpio_in));

    bool previous_state = false;
    
    while (true)
    {

        /* Blocks until at least one event is available. */
        if (input_request.wait_edge_events(timeout)) { 
            input_request.read_edge_events(events);
    
            for (const auto &event : events)
            {
                ostringstream payload;
                previous_state = (event.type() == 
                    ::gpiod::edge_event::event_type::RISING_EDGE);
                output_request.set_value(gpio_out, ( previous_state
                        ? gpiod::line::value::ACTIVE 
                        : gpiod::line::value::INACTIVE));
                // build payload
                // like: {"t":1765945224, "state":{1|0}, "device":"{device}", "gpio":{n}
                payload << "{\"t\":" << time(0)
                << ", \"state\":" << ((event.type()== ::gpiod::edge_event::event_type::RISING_EDGE)?1:0)
                << ", \"device\":\"" << device
                << "\", \"gpio\":" << gpio_in << "}";
                if (publish_msg(broker, topic, payload.str()))
                    cout << "msg not published" << endl;
            }
        } else
        {
            ostringstream payload;
            payload << "{\"t\":" << time(0)
            << ", \"state\":" << (previous_state?1:0)
            << ", \"device\":\"" << device
            << "\", \"gpio\":" << gpio_in << "}";
            if (publish_msg(broker, topic, payload.str()))
                cout << "msg not published" << endl;
        }
    }

    chip.close();
}