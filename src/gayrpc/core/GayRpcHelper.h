#pragma once

#include <functional>
#include <memory>
#include <string>

#include <google/protobuf/util/json_util.h>
#include <gayrpc/core/meta.pb.h>
#include <gayrpc/core/GayRpcType.h>
#include <gayrpc/core/GayRpcError.h>

namespace gayrpc { namespace core {

    using namespace google::protobuf::util;
    // 构造用于RPC请求的Meta对象
    inline RpcMeta makeRequestRpcMeta(uint64_t sequenceID,
        ServiceIDType serviceID,
        ServiceFunctionMsgIDType msgID,
        RpcMeta_DataEncodingType type,
        bool expectResponse)
    {
        RpcMeta meta;
        meta.set_type(RpcMeta::REQUEST);
        meta.set_encoding(type);
        meta.set_service_id(serviceID);
        meta.mutable_request_info()->set_intmethod(msgID);
        meta.mutable_request_info()->set_sequence_id(sequenceID);
        meta.mutable_request_info()->set_expect_response(expectResponse);

        return meta;
    }

    // 解析Response然后(通过拦截器)调用回调
    template<typename Response, typename Hanele>
    inline void    parseResponseWrapper(const Hanele& handle,
        const RpcMeta& meta,
        const std::string& data,
        const UnaryServerInterceptor& inboundInterceptor)
    {
        Response response;
        switch (meta.encoding())
        {
        case RpcMeta::BINARY:
            if (!response.ParseFromString(data))
            {
                throw std::runtime_error(std::string("parse binary response failed, type of:") + typeid(Response).name());
            }
            break;
        case RpcMeta::JSON:
        {
            auto s = JsonStringToMessage(data, &response);
            if (!s.ok())
            {
                throw std::runtime_error(std::string("parse json response failed:") + s.error_message().as_string() + ", type of:" + typeid(Response).name());
            }
            break;
        }
        default:
            throw std::runtime_error(std::string("response by unsupported encoding:") + std::to_string(meta.encoding()) + ", type of:" + typeid(Response).name());
        }

        gayrpc::core::RpcError error;
        if (meta.response_info().failed())
        {
            error = gayrpc::core::RpcError(meta.response_info().failed(),
                meta.response_info().error_code(),
                meta.response_info().reason());
        }

        inboundInterceptor(
            meta,
            response,
            [&response, &error, &handle](const RpcMeta&, const google::protobuf::Message&) {
            handle(response, error);
        });
    }

    // 解析Request然后(通过拦截器)调用服务处理函数
    template<typename RequestType>
    inline void parseRequestWrapper(RequestType& request,
        const RpcMeta& meta,
        const std::string& data,
        const UnaryServerInterceptor& inboundInterceptor,
        const UnaryHandler& handler)
    {
        switch (meta.encoding())
        {
        case RpcMeta::BINARY:
            if (!request.ParseFromString(data))
            {
                throw std::runtime_error(std::string("parse binary request failed, type of:") + typeid(RequestType).name());
            }
            break;
        case RpcMeta::JSON:
        {
            auto s = JsonStringToMessage(data, &request);
            if (!s.ok())
            {
                throw std::runtime_error(std::string("parse json request failed:") + s.error_message().as_string() + ", type of:" + typeid(RequestType).name());
            }
        }
        break;
        default:
            throw std::runtime_error(std::string("request by unsupported encoding:") + std::to_string(meta.encoding()) + ", type of:" + typeid(RequestType).name());
        }

        inboundInterceptor(meta, request, handler);
    }

} }