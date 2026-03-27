#pragma once

#include <string>

#include "protocol.pb.h"

namespace anolis_provider_ezo::handlers {

using CallRequest = anolis::deviceprovider::v1::CallRequest;
using DescribeDeviceRequest = anolis::deviceprovider::v1::DescribeDeviceRequest;
using GetHealthRequest = anolis::deviceprovider::v1::GetHealthRequest;
using HelloRequest = anolis::deviceprovider::v1::HelloRequest;
using ListDevicesRequest = anolis::deviceprovider::v1::ListDevicesRequest;
using ReadSignalsRequest = anolis::deviceprovider::v1::ReadSignalsRequest;
using Response = anolis::deviceprovider::v1::Response;
using WaitReadyRequest = anolis::deviceprovider::v1::WaitReadyRequest;

void handle_hello(const HelloRequest &request, Response &response);
void handle_wait_ready(const WaitReadyRequest &request, Response &response);
void handle_list_devices(const ListDevicesRequest &request, Response &response);
void handle_describe_device(const DescribeDeviceRequest &request, Response &response);
void handle_read_signals(const ReadSignalsRequest &request, Response &response);
void handle_call(const CallRequest &request, Response &response);
void handle_get_health(const GetHealthRequest &request, Response &response);
void handle_unimplemented(Response &response, const std::string &message = "operation not implemented");

} // namespace anolis_provider_ezo::handlers
