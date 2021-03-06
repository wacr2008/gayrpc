#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

#include <brynet/net/TCPService.h>
#include <brynet/net/Connector.h>
#include <brynet/utils/WaitGroup.h>

#include <gayrpc/utils/UtilsWrapper.h>
#include "./pb/benchmark_service.gayrpc.h"

using namespace brynet;
using namespace brynet::net;
using namespace dodo::benchmark;

typedef std::vector<std::chrono::nanoseconds> LATENTY_TYPE;
typedef std::shared_ptr<LATENTY_TYPE> LATENCY_PTR;

class BenchmarkClient : public std::enable_shared_from_this<BenchmarkClient>
{
public:
    BenchmarkClient(EchoServerClient::PTR client,
        brynet::utils::WaitGroup::PTR wg,
        int maxNum,
        LATENCY_PTR latency,
        std::string payload)
        :
        maxRequestNum(maxNum),
        mClient(std::move(client)),
        mWg(std::move(wg)),
        mPayload(std::move(payload)),
        mLatency(std::move(latency))
    {
        mCurrentNum = 0;
    }

    void sendRequest()
    {
        // 发送RPC请求
        EchoRequest request;
        request.set_message(mPayload);

        mRequestTime = std::chrono::steady_clock::now();
        mClient->Echo(request, std::bind(&BenchmarkClient::onEchoResponse, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    }

private:
    void    onEchoResponse(const EchoResponse& response,
        const gayrpc::core::RpcError& error)
    {
        mCurrentNum++;
        mLatency->push_back((std::chrono::steady_clock::now() - mRequestTime));

        if (error.failed())
        {
            std::cout << "reason" << error.reason() << std::endl;
            return;
        }

        if (mCurrentNum < maxRequestNum)
        {
            sendRequest();
        }
        else
        {
            mWg->done();
        }
    }

private:
    const int                                       maxRequestNum;
    const EchoServerClient::PTR                     mClient;
    const utils::WaitGroup::PTR                     mWg;
    const std::string                               mPayload;

    int                                             mCurrentNum;
    LATENCY_PTR                                     mLatency;
    std::chrono::steady_clock::time_point           mRequestTime;
};

std::atomic<int64_t> connectionCounter(0);

static void onConnection(dodo::benchmark::EchoServerClient::PTR client,
    const utils::WaitGroup::PTR& wg,
    int maxRequestNum,
    LATENCY_PTR latency,
    std::string payload)
{
    connectionCounter++;
    std::cout << "connection counter is:" << connectionCounter << std::endl;
    auto b = std::make_shared<BenchmarkClient>(client, wg, maxRequestNum, latency, payload);
    b->sendRequest();
}

static void outputLatency(int realyTotalRequestNum,
    const std::vector<LATENCY_PTR>& latencyArray,
    std::chrono::steady_clock::time_point startTime)
{
    auto nowTime = std::chrono::steady_clock::now();

    std::chrono::nanoseconds totalLatenty = std::chrono::nanoseconds::zero();
    LATENTY_TYPE tmp1;

    for (auto& v : latencyArray)
    {
        for (auto& latency : *v)
        {
            totalLatenty += latency;
            tmp1.push_back(latency);
        }
    }
    std::sort(tmp1.begin(), tmp1.end());

    auto costTime = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - startTime);

    std::cout << "connection num:"
        << connectionCounter
        << std::endl;

    std::cout << "took "
        << costTime.count()
        << "ms, for "
        << realyTotalRequestNum
        << " requests"
        << std::endl;

    std::cout << "throughput  (TPS):"
        << (realyTotalRequestNum / (std::chrono::duration_cast<std::chrono::seconds>(costTime)).count())
        << std::endl;

    std::cout << "mean:"
        << (std::chrono::duration_cast<std::chrono::milliseconds>(totalLatenty).count() / realyTotalRequestNum)
        << " ms ,"
        << (totalLatenty.count() / realyTotalRequestNum)
        << " ns"
        << std::endl;

    if (tmp1.empty())
    {
        std::cout << "latenty is empty" << std::endl;
        return;
    }

    std::cout << "median:"
        << (std::chrono::duration_cast<std::chrono::milliseconds>(tmp1[tmp1.size() / 2]).count())
        << " ms ,"
        << (tmp1[tmp1.size() / 2].count())
        << " ns"
        << std::endl;

    std::cout << "max:"
        << (std::chrono::duration_cast<std::chrono::milliseconds>(tmp1[tmp1.size() - 1]).count())
        << " ms ,"
        << (tmp1[tmp1.size() - 1].count())
        << " ns"
        << std::endl;

    std::cout << "min:"
        << (std::chrono::duration_cast<std::chrono::milliseconds>(tmp1[0]).count())
        << " ms ,"
        << (tmp1[0].count())
        << " ns"
        << std::endl;

    auto p99Index = tmp1.size() * 99 / 100;
    std::chrono::nanoseconds p99Total = std::chrono::nanoseconds::zero();
    for (size_t i = 0; i < p99Index; i++)
    {
        p99Total += tmp1[i];
    }
    std::cout << "p99:"
        << (std::chrono::duration_cast<std::chrono::milliseconds>(p99Total).count() / p99Index)
        << " ms ,"
        << (p99Total.count() / p99Index)
        << " ns"
        << std::endl;
}

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: <host> <port> <client num> <total request num> <payload size>\n");
        exit(-1);
    }

    auto server = TcpService::Create();
    server->startWorkerThread(std::thread::hardware_concurrency());

    auto connector = AsyncConnector::Create();
    connector->startWorkerThread();
    auto clientNum = std::stoi(argv[3]);
    auto maxRequestNumEveryClient = std::stoi(argv[4]) / clientNum;
    auto realyTotalRequestNum = maxRequestNumEveryClient * clientNum;
    auto payload = std::string(std::stoi(argv[5]), 'a');

    auto wg = utils::WaitGroup::Create();

    std::vector<LATENCY_PTR> latencyArray;

    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < clientNum; i++)
    {
        wg->add();

        try
        {
            auto latency = std::make_shared<LATENTY_TYPE>();
            latencyArray.push_back(latency);

            gayrpc::utils::AsyncCreateRpcClient< EchoServerClient>(server, connector,
                argv[1], std::stoi(argv[2]), std::chrono::seconds(10),
                nullptr, nullptr, nullptr, [=](dodo::benchmark::EchoServerClient::PTR client) {
                    onConnection(client, wg, maxRequestNumEveryClient, latency, payload);
                }, []() {
                    std::cout << "connect failed" << std::endl;
                }, 1024*1024, std::chrono::seconds(10));
        }
        catch (std::runtime_error& e)
        {
            std::cout << "error:" << e.what() << std::endl;
        }
    }
    
    wg->wait(std::chrono::seconds(100));

    outputLatency(realyTotalRequestNum, latencyArray, startTime);

    return 0;
}
