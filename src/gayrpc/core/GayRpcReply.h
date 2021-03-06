#pragma once

#include <atomic>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

#include <gayrpc/core/meta.pb.h>
#include <gayrpc/core/GayRpcType.h>

namespace gayrpc { namespace core {

    class BaseReply
    {
    public:
        BaseReply(RpcMeta meta, UnaryServerInterceptor outboundInterceptor)
            :
            mRequestMeta(std::move(meta)),
            mOutboundInterceptor(std::move(outboundInterceptor))
        {
        }

        virtual ~BaseReply() = default;

    protected:
        void    reply(const google::protobuf::Message& response)
        {
            if (mReplyFlag.test_and_set())
            {
                throw std::runtime_error("already reply");
            }

            if (!mRequestMeta.request_info().expect_response())
            {
                return;
            }

            RpcMeta meta;
            meta.set_type(RpcMeta::RESPONSE);
            meta.set_service_id(mRequestMeta.service_id());
            meta.mutable_response_info()->set_sequence_id(mRequestMeta.request_info().sequence_id());
            meta.mutable_response_info()->set_failed(false);
            meta.mutable_response_info()->set_timeout(false);

            mOutboundInterceptor(meta, response, [](const RpcMeta&, const google::protobuf::Message&) {
            });
        }

        template<typename Response>
        void    error(int32_t errorCode, const std::string& reason)
        {
            if (mReplyFlag.test_and_set())
            {
                throw std::runtime_error("already reply");
            }

            if (!mRequestMeta.request_info().expect_response())
            {
                return;
            }

            RpcMeta meta;
            meta.set_type(RpcMeta::RESPONSE);
            meta.set_encoding(RpcMeta_DataEncodingType_BINARY);
            meta.set_service_id(mRequestMeta.service_id());
            meta.mutable_response_info()->set_sequence_id(mRequestMeta.request_info().sequence_id());
            meta.mutable_response_info()->set_failed(true);
            meta.mutable_response_info()->set_error_code(errorCode);
            meta.mutable_response_info()->set_reason(reason);
            meta.mutable_response_info()->set_timeout(false);

            Response response;
            mOutboundInterceptor(meta, response, [](const RpcMeta&, const google::protobuf::Message&) {
            });
        }

    private:
        std::atomic_flag            mReplyFlag = ATOMIC_FLAG_INIT;
        RpcMeta                     mRequestMeta;
        UnaryServerInterceptor      mOutboundInterceptor;
    };

    template<typename T>
    class TemplateReply : public BaseReply
    {
    public:
        typedef std::shared_ptr<TemplateReply<T>> PTR;

        TemplateReply(RpcMeta meta,
            UnaryServerInterceptor outboundInterceptor)
            :
            BaseReply(std::move(meta), std::move(outboundInterceptor))
        {
        }

        void    reply(const T& response)
        {
            BaseReply::reply(response);
        }

        void    error(int32_t errorCode, const std::string& reason)
        {
            BaseReply::error<T>(errorCode, reason);
        }
    };

} }