#pragma once

#include <memory>

#include <gayrpc/core/GayRpcTypeHandler.h>
#include <gayrpc/core/GayRpcType.h>

namespace gayrpc { namespace core {

    class ServiceContext final
    {
    public:
        ServiceContext(RpcTypeHandleManager::PTR typeHandleManager, UnaryServerInterceptor inInterceptor, UnaryServerInterceptor outInterceptor)
            :
            mTypeHandleManager(typeHandleManager),
            mInInterceptor(inInterceptor),
            mOutInterceptor(outInterceptor)
        {}

        const RpcTypeHandleManager::PTR&    getTypeHandleManager() const
        {
            return mTypeHandleManager;
        }

        const UnaryServerInterceptor&       getInInterceptor() const
        {
            return mInInterceptor;
        }

        const UnaryServerInterceptor&       getOutInterceptor() const
        {
            return mOutInterceptor;
        }

    private:
        const RpcTypeHandleManager::PTR     mTypeHandleManager;
        const UnaryServerInterceptor        mInInterceptor;
        const UnaryServerInterceptor        mOutInterceptor;
    };

    class BaseService : public std::enable_shared_from_this<BaseService>
    {
    public:
        BaseService(ServiceContext context)
            :
            mContext(context)
        {
        }
        virtual ~BaseService() = default;

        virtual void onClose() {}

        const ServiceContext&               getServiceContext() const
        {
            return mContext;
        }

    private:
        const ServiceContext                mContext;
    };

} }