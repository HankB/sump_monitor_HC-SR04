// following the example from
// https://github.com/HankB/GPIOD_Debian_Raspberry_Pi/blob/main/60-Capstone_C%2B%2B/follow_input.cpp

/*
Build
g++ -Wno-psabi -Wall -o sump_monitor sump_monitor.cpp -lgpiodcxx
*/

#include <iostream>
using namespace std;
#include <gpiod.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

namespace
{
    const ::filesystem::path chip_path("/dev/gpiochip0");
    const ::gpiod::line::offset trigger_line_offset = 24;
    const ::gpiod::line::offset echo_line_offset = 23;
    auto timeout = ::chrono::nanoseconds((long long unsigned)5 * 1000000000); // timeout in nano-seconds
    const char *consumer = "HC-SR04";

    constexpr int debug_lvl = 0; // control chattiness 0-3
    chrono::_V2::system_clock::duration pulse_send_ts;
    chrono::_V2::system_clock::duration wait_start_ts;
    chrono::_V2::system_clock::duration wait_complete_ts;

    gpiod::line::value before_pulse;
    gpiod::line::value during_pulse;
    gpiod::line::value after_pulse;
    gpiod::line::value before_wait;
    gpiod::line::value after_wait;

    int send_pulse(gpiod::line_request &pulse, gpiod::line::offset pulse_offset,
                   gpiod::line_request &echo, gpiod::line::offset echo_offset)
    {
        before_pulse = echo.get_value(echo_offset);
        pulse.set_value(pulse_offset, ::gpiod::line::value::ACTIVE);
        during_pulse = echo.get_value(echo_offset);

        // delay 10 microseconds
        this_thread::sleep_for(std::chrono::microseconds(10));
        pulse.set_value(pulse_offset, ::gpiod::line::value::INACTIVE);
        // pulse_send_ts = micros();
        pulse_send_ts = chrono::high_resolution_clock::now().time_since_epoch();
        after_pulse = echo.get_value(echo_offset);

        return 0;
    }

    const char *edge_event_type_str(const ::gpiod::edge_event &event)
    {
        switch (event.type())
        {
        case ::gpiod::edge_event::event_type::RISING_EDGE:
            return "Rising";
        case ::gpiod::edge_event::event_type::FALLING_EDGE:
            return "Falling";
        default:
            return "Unknown";
        }
    }
    std::vector<float> distance_readings;

} /* namespace */

int main(int argc, char **argv)
{
    if (debug_lvl > 1)
        cout << "GPIOD version " << gpiod::api_version() << endl;

    // chip object

    gpiod::chip chip("/dev/gpiochip0");
    if (!chip)
    {
        cerr << "chip not constructed" << endl;
        exit(-1);
    }
    else if (debug_lvl > 1)
    {

        cout << "chip is constructed and useable" << endl;
    }
    if (debug_lvl > 1)
    {
        gpiod::chip_info info = chip.get_info();
        cout << "name:" << info.name() << " label:" << info.label() << endl;
    }

    gpiod::edge_event_buffer events;
    if (debug_lvl > 1)
    {
        cout << "empty buffer holds " << events.num_events() << " events"
             << " and has a capacity of " << events.capacity() << endl;
    }
    // input processing from the GPIOD example follow_input.cpp
    // but probably don;t need the pullup and only interest is
    // the rising input.

    auto echo_request =
        chip.prepare_request()
            .set_consumer(consumer)
            .add_line_settings(
                echo_line_offset,
                ::gpiod::line_settings()
                    .set_direction(
                        ::gpiod::line::direction::INPUT)
                    //                    .set_bias(
                    //                        ::gpiod::line::bias::PULL_UP)
                    .set_edge_detection(
                        ::gpiod::line::edge::BOTH))
            .do_request();
    // output processing copied substantially from toggle_line_value.cpp

    auto output_request =
        ::gpiod::chip(chip_path)
            .prepare_request()
            .set_consumer(consumer)
            .add_line_settings(
                trigger_line_offset,
                ::gpiod::line_settings().set_direction(
                    ::gpiod::line::direction::OUTPUT))
            .do_request();

    // initialize the trigger to low
    output_request.set_value(trigger_line_offset, ::gpiod::line::value::INACTIVE);

    if (debug_lvl > 0)
        cerr << "pulse width, distance inches" << endl;

    size_t reading_count = 0;
    bool need_pulse = true;
    auto readings_needed = size_t{5};

    while (reading_count < readings_needed)
    {
        if (need_pulse)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(60)); // delay 60 microseconda per recommendation - try 100 ms
            send_pulse(output_request, trigger_line_offset, echo_request, echo_line_offset);

            need_pulse = false;
            if (debug_lvl > 1)
                cout << "\t\t\tpulse rdback (before, during, after) " << before_pulse << ", "
                     << during_pulse << ", " << after_pulse << endl;
        }
        wait_start_ts = chrono::high_resolution_clock::now().time_since_epoch();
        before_wait = echo_request.get_value(echo_line_offset);

        constexpr size_t reading_size = 10;
        gpiod::line::value readings[reading_size];
        constexpr size_t reading_delay = 100;

        auto timeout = ::chrono::milliseconds(100L);

        if (debug_lvl > 2)
        {
            for (size_t i = 0; i < reading_size; i++)
            {
                readings[i] = echo_request.get_value(echo_line_offset);
                this_thread::sleep_for(std::chrono::microseconds(reading_delay));
            }
        }
        auto have_events = echo_request.wait_edge_events(timeout);
        after_wait = echo_request.get_value(echo_line_offset);
        wait_complete_ts = chrono::high_resolution_clock::now().time_since_epoch();
        if (debug_lvl > 2)
        {
            cout << "\t\t\t";
            for (size_t i = 0; i < reading_size; i++)
            {
                cout << readings[i] << " ";
            }
            cout << endl;
        }
        if (debug_lvl > 1)
        {
            cout << "\t\t\twait rdback (before, after) " << before_wait << " " << after_wait << endl;
            cout << "\t\t\twait_edge_events() have_events:" << have_events << " start-send "
                 << wait_start_ts.count() - pulse_send_ts.count() << " wait-send "
                 << wait_complete_ts.count() - pulse_send_ts.count() << endl;
        }
        if (!have_events)
        {
            need_pulse = true;
        }
        else
        {
            static time_t start = 0;
            static time_t finish = 0;
            echo_request.read_edge_events(events);

            for (const auto &event : events)
            {

                if (debug_lvl > 1)
                    ::cout << event << ::endl;

                switch (event.type())
                {
                case ::gpiod::edge_event::event_type::RISING_EDGE:
                    start = event.timestamp_ns();
                    break;
                case ::gpiod::edge_event::event_type::FALLING_EDGE:
                    if (start != 0) // if we didn't miss the start of the pulse
                    {
                        finish = event.timestamp_ns();
                        auto raw = finish - start;
                        float pulse_width = ((float)(finish - start) / 1000000000);
                        float distance = pulse_width * 1100 * 12 / 2.0; // distance in inches based on 1100 fps in air
                        if (debug_lvl > 1)
                            cout << "readings: ";
                        cout << "                     " << (finish - start)
                             << ", " << raw
                             << ", " << pulse_width
                             << ", " << distance << endl;
                        distance_readings.push_back(distance);
                        cout << "                     pushing: " << (distance) << endl;

                        start = 0; // zero our indicator for next reading
                        reading_count++;
                        // this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    else
                    {
                        cout << "missed the start" << endl;
                    }
                    need_pulse = true;
                    break;
                default:
                    cerr << "\t\t\tUnknown event.event_type" << edge_event_type_str(event) << endl;
                    break;
                }
            }
        }
    }
    nth_element(distance_readings.begin(), distance_readings.begin() + readings_needed / 2, distance_readings.end());
    auto median_distance = distance_readings[readings_needed / 2];

    cout << " distance: " << median_distance
         << " of : " << distance_readings.size() << endl;
    for (const auto &reading : distance_readings)
    {
        std::cout << reading << " ";
    }
    cout << endl;
}
// (raw_readings[readings_needed / 2])