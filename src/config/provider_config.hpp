#pragma once

#include <string>
#include <vector>

namespace anolis_provider_ezo {

enum class EzoDeviceType {
    Ph,
    Orp,
    Ec,
    Do,
    Rtd,
    Hum,
};

struct DeviceSpec {
    std::string id;
    EzoDeviceType type = EzoDeviceType::Ph;
    std::string label;
    int address = 0;
};

struct ProviderConfig {
    std::string config_file_path;
    std::string provider_name = "anolis-provider-ezo";
    std::string bus_path;
    int query_delay_us = 300000;
    int timeout_ms = 300;
    int retry_count = 2;
    std::vector<DeviceSpec> devices;
};

ProviderConfig load_config(const std::string &path);
EzoDeviceType parse_device_type(const std::string &value);
std::string to_string(EzoDeviceType type);
std::string format_i2c_address(int address);
std::string summarize_config(const ProviderConfig &config);

} // namespace anolis_provider_ezo
