#include "InterOp.h"
#include "njp/Network.h"
#include <kls/coroutine/Future.h>
#include <kls/coroutine/Operation.h>

using namespace kls;
using namespace njp;
using namespace kls::coroutine;

namespace {
    class InterOpClient {
    public:
        class Request {
        public:
            Request(
                    InterOpClient *client,
                    const char *method, const char *resource, const char *data,
                    njp_phttp_response_callback callback, void *callback_data
            ) :
                    m_client(client), m_payload(temp_json::parse(data)), m_method(method), m_resource(resource),
                    m_callback(callback), m_callback_data(callback_data) { request(); }

            ValueAsync<> request() {
                try {
                    auto result = co_await m_client->m_client->exec(m_method, m_resource, std::move(m_payload));
                    auto data = result.dump();
                    m_callback(200, data.c_str(), m_callback_data);
                }
                catch (ClientRemoteError &err) { m_callback(err.code(), nullptr, m_callback_data); }
                catch (...) { m_callback(500, nullptr, m_callback_data); }
                delete this;
            }
        private:
            InterOpClient *m_client;
            temp_json m_payload;
            temp::string m_method, m_resource;
            njp_phttp_response_callback m_callback;
            void *m_callback_data;
        };

        InterOpClient(const char *address, int port) :
                m_client(run_blocking([=]() -> ValueAsync<std::shared_ptr<IClient>> {
                    co_return co_await IClient::create({io::Address::CreateIPv4({address}).value(), port});
                })) {}

        ~InterOpClient() { run_blocking([this]() -> ValueAsync<> { co_await m_client->close(); }); }
    private:
        std::shared_ptr<IClient> m_client{};
    };

    class InterOpServer {
    public:
        using Future = FlexFuture<std::pair<int, temp::string>>;

        InterOpServer(const char *address, int port) :
                m_server(IServer::create({io::Address::CreateIPv4({address}).value(), port}, 128)) {}

        ~InterOpServer() { run_blocking([this]() -> ValueAsync<> { co_await m_server->close(); }); }

        struct Request {
            Future::PromiseHandle promise;
            void release(int code, const char *content) const {
                promise->set(std::pair<int, temp::string>(code, content));
            }
        };

        void handler(
                const char *method, const char *resource,
                njp_phttp_request_callback callback, void *callback_data
        ) {
            m_server->handles(method, resource, [=](temp_json request) -> ValueAsync<temp_json> {
                Request helper{};
                auto future = Future{[&](auto h) { helper.promise = h; }};
                auto json_string = request.dump();
                callback(reinterpret_cast<handle>(&request), json_string.c_str(), callback_data);
                auto &&[code, response] = co_await future;
                if (code != 200) throw ServerUserCodeError(code, response);
                co_return temp_json::parse(response);
            });
        }
    private:
        std::shared_ptr<IServer> m_server;
    };
}

handle njp_phttp_connect(const char *address, int port) {
    return reinterpret_cast<handle>(new InterOpClient(address, port));
}

void njp_phttp_start_request(
        handle server,
        const char *method, const char *resource, const char *data,
        njp_phttp_response_callback callback, void *callback_data
) {
    auto client = reinterpret_cast<InterOpClient *>(server);
    new InterOpClient::Request(client, method, resource, data, callback, callback_data);
}

void njp_phttp_disconnect(handle client) { delete reinterpret_cast<InterOpClient *>(client); }

handle njp_phttp_start_serve(const char *address, int port) {
    return reinterpret_cast<handle>(new InterOpServer(address, port));
}

void njp_phttp_handle_response(
        handle server, const char *method, const char *resource,
        njp_phttp_request_callback callback, void *callback_data
) {
    reinterpret_cast<InterOpServer *>(server)->handler(method, resource, callback, callback_data);
}

void njp_phttp_done_response(handle response, int code, const char *content) {
    reinterpret_cast<InterOpServer::Request*>(response)->release(code, content);
}

void njp_phttp_stop_serve(handle server) { delete reinterpret_cast<InterOpServer *>(server); }