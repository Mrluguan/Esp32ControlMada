//内部固件程序版本号
const int firmwareVersion = 12;
//当前时区偏移量
const int timezone_offset = 8;
//检查更新地址
const char *check_version_url = "https://iot.dotsourceit.com/IoTDevice/GetLatestVersion";
//下载更新地址
const char *download_latest_firmware_url = "https://iot.dotsourceit.com/IoTDevice/DownloadLatestOTAFirmwareBin";
//节能模式HttpPing地址
const char *http_ping_url = "https://iot.dotsourceit.com/IoTDevice/Ping";
//获取当前网络时间戳地址
const char *get_current_timespan_url = "https://iot.dotsourceit.com/IoTDevice/Time";
//节能模式
const bool PowerSaveMode = true;
//主要服务器地址
const char *mqtt_server = "iot.dotsourceit.com";
//MQTT服务器端口
const int mqtt_port = 24515;
//Mqtt设备发布主题前缀
const char *mqtt_device_pub_topic_prefix = "dots_iot_client_pub/";
//Mqtt设备订阅主题前缀
const char *mqtt_device_sub_topic_prefix = "dots_iot_client_sub/";

//公司代号
const char *CompanyNo = "DY";
//产品类型
const char *DeviceCategory = "EID";
//产品型号
const char *DeviceModelNo = "MP072";

const int status_report_interval = 60000;

/*const int POWER_EN = 15;
const int LED_RED_PIN = 5;
const int LED_GREEN_PIN = 17;
const int LED_BLUE_PIN = 16;*/

const int POWER_EN = 17;
const int LED_RED_PIN = 17;
const int LED_GREEN_PIN = 17;
const int LED_BLUE_PIN = 17;

const int KEY = 4;