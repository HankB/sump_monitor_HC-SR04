/*
Simple example to publish to broker.

Work here will lean heavily on
https://github.com/eclipse-paho/paho.mqtt.cpp/blob/master/examples/topic_publish.cpp

Build:
g++ -Wall -o publish publish.cpp -lpaho-mqttpp3 -lpaho-mqtt3as -lpthread
*/
#include <iostream>
#include <atomic>
#include <unistd.h>

#include "mqtt/async_client.h"

#include "proj_mqtt.h"

using namespace std;

namespace
{

	const string CLIENT_ID{"MQTT_publish"};
	const int QOS = 0;

	//const auto TIMEOUT = std::chrono::seconds(10);

}

int publish_msg(const string &serverURI, const string &topic, const string &payload)
{
	static mqtt::async_client cli(serverURI, "");

	try
	{

		if (!cli.is_connected())
		{
			cli.connect()->wait();
		}

		mqtt::topic top(cli, topic, QOS);
		mqtt::token_ptr tok;

		tok = top.publish(payload);
		tok->wait(); // Just wait for the last one to complete.
		cli.disconnect()->wait();
	}
	catch (const mqtt::exception &exc)
	{
		cerr << exc << endl;
		return 1;
	}

	return 0;
}