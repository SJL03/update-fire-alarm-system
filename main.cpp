// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------
#ifndef MBED_TEST_MODE

#include "mbed.h"
#include "simple-mbed-cloud-client.h"
#include "DS1820.h"
#include "FATFileSystem.h"
#include "LittleFileSystem.h"

// Default network interface object. Don't forget to change the WiFi SSID/password in mbed_app.json if you're using WiFi.
NetworkInterface *net = NetworkInterface::get_default_instance();

// Default block device available on the target board
BlockDevice *bd = BlockDevice::get_default_instance();

#if COMPONENT_SD || COMPONENT_NUSD
// Use FATFileSystem for SD card type blockdevices
FATFileSystem fs("fs");
#else
// Use LittleFileSystem for non-SD block devices to enable wear leveling and other functions
LittleFileSystem fs("fs");
#endif

// Default User button for GET example
InterruptIn button(BUTTON1);

#define SENSORS_POLL_INTERVAL 3.0
#define YANWU_GATE 70.0
#define TEMP_GATE  70.0

//自定义输入引脚
DS1820      ds1820(D7); 
AnalogIn adc_yanwu(A1);
float temp = 0;

MbedCloudClientResource *res_yanwu;
MbedCloudClientResource *res_temp;

// Declaring pointers for access to Pelion Device Management Client resources outside of main()
MbedCloudClientResource *res_button;

// An event queue is a very useful structure to debounce information between contexts (e.g. ISR and normal threads)
// This is great because things such as network operations are illegal in ISR, so updating a resource in a button's fall() function is not allowed
EventQueue eventQueue;

// When the device is registered, this variable will be used to access various useful information, like device ID etc.
static const ConnectorClientEndpointInfo* endpointInfo;

/**
 * Button handler
 * This function will be triggered either by a physical button press or by a ticker every 5 seconds (see below)
 */
void button_press() {
    int v = res_button->get_value_int() + 100;
    res_button->set_value(v);
    printf("Button clicked %d times\n", v);
}

/**
 * Notification callback handler
 * @param resource The resource that triggered the callback
 * @param status The delivery status of the notification
 */
void button_callback(MbedCloudClientResource *resource, const NoticationDeliveryStatus status) {
    printf("Button notification, status %s (%d)\n", MbedCloudClientResource::delivery_status_to_string(status), status);
}

/**
 * Registration callback handler
 * @param endpoint Information about the registered endpoint such as the name (so you can find it back in portal)
 * When the device is registered, this variable will be used to access various useful information, like device ID etc.
 */
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("Registered to Pelion Device Management. Endpoint Name: %s\n", endpoint->internal_endpoint_name.c_str());
    endpointInfo = endpoint;
}

void sensors_update() {
    float yanwu = adc_yanwu.read()*100;
    int   result = 0; 
    float temp_first = 0;
    if (ds1820.begin()) {
        ds1820.startConversion();   // start temperature conversion from analog to digital
        wait(1.0);                  // let DS1820 complete the temperature conversion
        result = ds1820.read(temp_first); // read temperature from DS1820 and perform cyclic redundancy check (CRC)
        if (result==0)
            temp=temp_first;
    }
    printf("yanwu :  %6.4f ,  temp: %6.4f C\r\n", yanwu, temp);
    if(yanwu>YANWU_GATE||temp>TEMP_GATE)
        printf("Fire!!!Fire!!!Fire!!! \r\n");
    if (endpointInfo) {
        res_yanwu->set_value(yanwu);
        res_temp->set_value(temp);
    }
}

int main(void) {
    printf("\nStarting Simple Pelion Device Management Client example\n");

    int storage_status = fs.mount(bd);
    if (storage_status != 0) {
        printf("Storage mounting failed.\n");
    }

    if (storage_status) {
        printf("Formatting the storage...\n");
        int storage_status = StorageHelper::format(&fs, bd);
        if (storage_status != 0) {
            printf("ERROR: Failed to reformat the storage (%d).\n", storage_status);
        }
    } 

    // Connect to the Internet (DHCP is expected to be on)
    printf("Connecting to the network using the default network interface...\n");
    net = NetworkInterface::get_default_instance();
    nsapi_error_t net_status = NSAPI_ERROR_NO_CONNECTION;
    if( ! net->get_ip_address()){
        while ((net_status = net->connect()) != NSAPI_ERROR_OK) {
        
            printf("Unable to connect to network (%d). Retrying...\n", net_status);
        }
    }

    printf("Connected to the network successfully. IP address: %s\n", net->get_ip_address());

    printf("Initializing Pelion Device Management Client...\n");

    // SimpleMbedCloudClient handles registering over LwM2M to Pelion Device Management
    SimpleMbedCloudClient client(net, bd, &fs);
    int client_status = client.init();
    if (client_status != 0) {
        printf("Pelion Client initialization failed (%d)\n", client_status);
        return -1;
    }

    // Creating resources, which can be written or read from the cloud
    res_button = client.create_resource("3200/0/5501", "Button Count");
    res_button->set_value(0);
    res_button->methods(M2MMethod::GET);
    res_button->observable(true);
    res_button->attach_notification_callback(button_callback);
    
    // Sensor resources
    res_yanwu = client.create_resource("3303/0/5700", "Yanwu_ADC");
    res_yanwu->set_value(0);
    res_yanwu->methods(M2MMethod::GET);
    res_yanwu->observable(true);

    res_temp = client.create_resource("3316/0/5700", "Temp_ds1820");
    res_temp->set_value(0);
    res_temp->methods(M2MMethod::GET);
    res_temp->observable(true);

    printf("Initialized Pelion Device Management Client. Registering...\n");

    // Callback that fires when registering is complete
    client.on_registered(&registered);

    // Register with Pelion DM
    client.register_and_connect();

    // The button fires on an interrupt context, but debounces it to the eventqueue, so it's safe to do network operations
    button.fall(eventQueue.event(&button_press));
    printf("Press the user button to increment the LwM2M resource value...\n");

    Ticker timer;
    timer.attach(eventQueue.event(&sensors_update), SENSORS_POLL_INTERVAL);

    // You can easily run the eventQueue in a separate thread if required
    eventQueue.dispatch_forever();
}

#endif /* MBED_TEST_MODE */
