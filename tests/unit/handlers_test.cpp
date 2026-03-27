#include <gtest/gtest.h>

#include "config/provider_config.hpp"
#include "core/handlers.hpp"
#include "core/runtime_state.hpp"
#include "protocol.pb.h"

namespace {

anolis_provider_ezo::ProviderConfig make_stub_config() {
    anolis_provider_ezo::ProviderConfig config;
    config.provider_name = "ezo-lab";
    config.bus_path = "mock://unit-test-i2c";
    config.query_delay_us = 300000;
    config.timeout_ms = 300;
    config.retry_count = 2;
    config.devices = {
        anolis_provider_ezo::DeviceSpec{"ph0", anolis_provider_ezo::EzoDeviceType::Ph, "Tank pH", 0x63},
        anolis_provider_ezo::DeviceSpec{"do0", anolis_provider_ezo::EzoDeviceType::Do, "Tank DO", 0x61},
    };
    return config;
}

TEST(HandlersTest, HelloReturnsPhaseThreeMetadata) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::HelloRequest request;
    request.set_protocol_version("v1");
    request.set_client_name("test-client");

    anolis::deviceprovider::v1::Response response;
    anolis_provider_ezo::handlers::handle_hello(request, response);

    EXPECT_EQ(response.status().code(), anolis::deviceprovider::v1::Status::CODE_OK);
    EXPECT_EQ(response.hello().provider_name(), "anolis-provider-ezo");
    EXPECT_EQ(response.hello().metadata().at("phase"), "3");
    EXPECT_EQ(response.hello().metadata().at("vertical_slice"), "ph");
}

TEST(HandlersTest, HelloRejectsWrongProtocolVersion) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::HelloRequest request;
    request.set_protocol_version("v0");

    anolis::deviceprovider::v1::Response response;
    anolis_provider_ezo::handlers::handle_hello(request, response);

    EXPECT_EQ(response.status().code(), anolis::deviceprovider::v1::Status::CODE_FAILED_PRECONDITION);
}

TEST(HandlersTest, WaitReadyAndGetHealthReflectActiveAndExcludedDevices) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::Response wait_response;
    anolis_provider_ezo::handlers::handle_wait_ready(
        anolis::deviceprovider::v1::WaitReadyRequest{},
        wait_response);
    EXPECT_EQ(wait_response.status().code(), anolis::deviceprovider::v1::Status::CODE_OK);
    EXPECT_EQ(wait_response.wait_ready().diagnostics().at("ready"), "true");
    EXPECT_EQ(wait_response.wait_ready().diagnostics().at("active_device_count"), "1");
    EXPECT_EQ(wait_response.wait_ready().diagnostics().at("excluded_device_count"), "1");

    anolis::deviceprovider::v1::Response health_response;
    anolis_provider_ezo::handlers::handle_get_health(
        anolis::deviceprovider::v1::GetHealthRequest{},
        health_response);
    EXPECT_EQ(health_response.status().code(), anolis::deviceprovider::v1::Status::CODE_OK);
    EXPECT_EQ(health_response.get_health().devices_size(), 2);
}

TEST(HandlersTest, ListDevicesReturnsOnlyActivePhaseThreeInventory) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::ListDevicesRequest request;
    request.set_include_health(true);

    anolis::deviceprovider::v1::Response response;
    anolis_provider_ezo::handlers::handle_list_devices(request, response);

    EXPECT_EQ(response.status().code(), anolis::deviceprovider::v1::Status::CODE_OK);
    ASSERT_EQ(response.list_devices().devices_size(), 1);
    EXPECT_EQ(response.list_devices().devices(0).device_id(), "ph0");
    EXPECT_EQ(response.list_devices().devices(0).type_id(), "sensor.ezo.ph");
    EXPECT_GE(response.list_devices().device_health_size(), 1);
}

TEST(HandlersTest, DescribeDeviceReturnsCapabilitiesForPhDevice) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::DescribeDeviceRequest request;
    request.set_device_id("ph0");

    anolis::deviceprovider::v1::Response response;
    anolis_provider_ezo::handlers::handle_describe_device(request, response);

    EXPECT_EQ(response.status().code(), anolis::deviceprovider::v1::Status::CODE_OK);
    EXPECT_EQ(response.describe_device().device().device_id(), "ph0");
    ASSERT_EQ(response.describe_device().capabilities().signals_size(), 1);
    EXPECT_EQ(response.describe_device().capabilities().signals(0).signal_id(), "ph.value");
}

TEST(HandlersTest, ReadSignalsReturnsPhValue) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::ReadSignalsRequest request;
    request.set_device_id("ph0");
    request.add_signal_ids("ph.value");

    anolis::deviceprovider::v1::Response response;
    anolis_provider_ezo::handlers::handle_read_signals(request, response);

    EXPECT_EQ(response.status().code(), anolis::deviceprovider::v1::Status::CODE_OK);
    EXPECT_EQ(response.read_signals().device_id(), "ph0");
    ASSERT_EQ(response.read_signals().values_size(), 1);
    EXPECT_EQ(response.read_signals().values(0).signal_id(), "ph.value");
    EXPECT_EQ(response.read_signals().values(0).value().type(),
              anolis::deviceprovider::v1::VALUE_TYPE_DOUBLE);
}

TEST(HandlersTest, ReadSignalsRejectsUnknownSignalId) {
    anolis_provider_ezo::runtime::reset();
    anolis_provider_ezo::runtime::initialize(make_stub_config());

    anolis::deviceprovider::v1::ReadSignalsRequest request;
    request.set_device_id("ph0");
    request.add_signal_ids("ph.bad");

    anolis::deviceprovider::v1::Response response;
    anolis_provider_ezo::handlers::handle_read_signals(request, response);

    EXPECT_EQ(response.status().code(), anolis::deviceprovider::v1::Status::CODE_NOT_FOUND);
}

} // namespace
