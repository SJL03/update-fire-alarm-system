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

//默认网络接口对象实例声明
NetworkInterface *net = NetworkInterface::get_default_instance();

//扩展存储实例
BlockDevice *bd = BlockDevice::get_default_instance();

#if COMPONENT_SD || COMPONENT_NUSD
FATFileSystem fs("fs");
#else
LittleFileSystem fs("fs");
#endif

//火灾预警系统传感器主要参数
#define SENSORS_POLL_INTERVAL 3.0
#define YANWU_GATE 70.0
#define TEMP_GATE  70.0

//传感器定义
DS1820   ds1820(D7); 
AnalogIn adc_yanwu(A1);
float temp = 0;

//服务器传感器资源
MbedCloudClientResource *res_yanwu;
MbedCloudClientResource *res_temp;

EventQueue eventQueue;

//存储终端设备信息
static const ConnectorClientEndpointInfo* endpointInfo;

//注册到服务器时候的回调函数
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("Registered to Pelion Device Management. Endpoint Name: %s\n", endpoint->internal_endpoint_name.c_str());
    endpointInfo = endpoint;
}

//传感器数据读取和计算
void sensors_update() {
    float yanwu = adc_yanwu.read()*100;
    int   result = 0; 
    float temp_first = 0;
    if (ds1820.begin()) {
        ds1820.startConversion();
        wait(1.0);
        result = ds1820.read(temp_first);
        if (result==0)
            temp=temp_first;
    }
    printf("yanwu :  %6.4f %%,  temp: %6.4f C\r\n", yanwu, temp);
    if( yanwu>YANWU_GATE || temp>TEMP_GATE )
        printf("Fire!!!Fire!!!Fire!!! \r\n");
    if (endpointInfo) {
        res_yanwu->set_value(yanwu);
        res_temp->set_value(temp);
    }
}

int main(void) {
    printf("\nStarting Fire Alarm System\n");

    //挂载SD卡
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

    //连接网络
    printf("Connecting to the network using the default network interface...\n");
    net = NetworkInterface::get_default_instance();
    nsapi_error_t net_status = NSAPI_ERROR_NO_CONNECTION;
    if( ! net->get_ip_address()){
        while ((net_status = net->connect()) != NSAPI_ERROR_OK) {
        
            printf("Unable to connect to network (%d). Retrying...\n", net_status);
        }
    }
    printf("Connected to the network successfully. IP address: %s\n", net->get_ip_address());

    //客户端对象初始化
    printf("Initializing Pelion Device Management Client...\n");
    SimpleMbedCloudClient client(net, bd, &fs);
    int client_status = client.init();
    if (client_status != 0) {
        printf("Pelion Client initialization failed (%d)\n", client_status);
        return -1;
    }
    
    //传感器资源初始化
    res_yanwu = client.create_resource("3303/0/5700", "Yanwu_ADC");
    res_yanwu->set_value(0);
    res_yanwu->methods(M2MMethod::GET);
    res_yanwu->observable(true);

    res_temp = client.create_resource("3316/0/5700", "Temp_ds1820");
    res_temp->set_value(0);
    res_temp->methods(M2MMethod::GET);
    res_temp->observable(true);

    //注册到Mbed Pelion 服务器
    printf("Initialized Pelion Device Management Client. Registering...\n");
    client.on_registered(&registered);
    client.register_and_connect();

    //根据传感器设置轮询时间更新数据
    Ticker timer;
    timer.attach(eventQueue.event(&sensors_update), SENSORS_POLL_INTERVAL);

    // 队列轮训
    eventQueue.dispatch_forever();
}

#endif
