// A small Poco::Net HTTP server, shaped like POCO's own HTTPTimeServer sample:
// a Poco::Util::ServerApplication that owns a Poco::Net::HTTPServer, with one
// HTTPRequestHandler per route chosen by an HTTPRequestHandlerFactory.
//
// Listens on 0.0.0.0:$PORT (read at runtime, default 8080) and serves at the
// root path:
//   GET /        -> a plain-text greeting
//   GET /health  -> {"status":"ok"}, the fleet's health check
//   GET /api/time -> {"time":"<ISO-8601 now>"}
//   anything else -> 404

#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Environment.h>
#include <Poco/JSON/Object.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/NumberParser.h>
#include <Poco/URI.h>
#include <Poco/Util/ServerApplication.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;

namespace {

void sendJSON(HTTPServerResponse& response, const Poco::JSON::Object& body,
              HTTPResponse::HTTPStatus status = HTTPResponse::HTTP_OK)
{
    response.setStatus(status);
    response.setContentType("application/json");
    body.stringify(response.send());
}

class RootHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest&, HTTPServerResponse& response) override
    {
        response.setContentType("text/plain");
        response.send() << "Hello from the POCO template!\n";
    }
};

class HealthHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest&, HTTPServerResponse& response) override
    {
        Poco::JSON::Object body;
        body.set("status", "ok");
        sendJSON(response, body);
    }
};

class TimeHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest&, HTTPServerResponse& response) override
    {
        Poco::JSON::Object body;
        body.set("time", Poco::DateTimeFormatter::format(Poco::DateTime(), Poco::DateTimeFormat::ISO8601_FORMAT));
        sendJSON(response, body);
    }
};

class NotFoundHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest&, HTTPServerResponse& response) override
    {
        Poco::JSON::Object body;
        body.set("error", "not found");
        sendJSON(response, body, HTTPResponse::HTTP_NOT_FOUND);
    }
};

// KEEP THIS: one log line per request (status + duration) is what the workspace Monitoring tab charts.
// Wraps every route's handler and, once it has answered, writes one JSON line to stdout, e.g.
//   {"durationMs":0.123,"method":"GET","path":"/health","status":200,"ua":"curl/8.5.0"}
// Never the query string, headers other than the user agent, or bodies.
class RequestLogHandler : public HTTPRequestHandler {
public:
    explicit RequestLogHandler(HTTPRequestHandler* inner) : inner_(inner) {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
    {
        const auto start = std::chrono::steady_clock::now();
        try {
            inner_->handleRequest(request, response);
        } catch (...) {
            log(request, response.sent() ? response.getStatus() : HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, start);
            throw;
        }
        log(request, response.getStatus(), start);
    }

private:
    static void log(const HTTPServerRequest& request, int status, std::chrono::steady_clock::time_point start)
    {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        Poco::JSON::Object line;
        line.set("status", status);
        line.set("durationMs", static_cast<double>(static_cast<long long>(ms * 1000.0 + 0.5)) / 1000.0);
        line.set("method", request.getMethod());
        line.set("path", Poco::URI(request.getURI()).getPath());
        line.set("ua", request.get("User-Agent", ""));
        std::ostringstream out;
        line.stringify(out);
        out << '\n';
        const std::string s = out.str();
        std::fwrite(s.data(), 1, s.size(), stdout);  // one write per line: no interleaving across threads
        std::fflush(stdout);
    }

    std::unique_ptr<HTTPRequestHandler> inner_;
};

class RequestHandlerFactory : public HTTPRequestHandlerFactory {
public:
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override
    {
        return new RequestLogHandler(route(request));
    }

private:
    static HTTPRequestHandler* route(const HTTPServerRequest& request)
    {
        const std::string path = Poco::URI(request.getURI()).getPath();
        if (path == "/")
            return new RootHandler;
        if (path == "/health")
            return new HealthHandler;
        if (path == "/api/time")
            return new TimeHandler;
        return new NotFoundHandler;
    }
};

class App : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>&) override
    {
        Poco::UInt16 port = 8080;
        unsigned parsed = 0;
        if (Poco::NumberParser::tryParseUnsigned(Poco::Environment::get("PORT", "8080"), parsed) && parsed > 0 &&
            parsed < 65536)
            port = static_cast<Poco::UInt16>(parsed);

        Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("0.0.0.0", port));
        Poco::Net::HTTPServer server(new RequestHandlerFactory, socket, new Poco::Net::HTTPServerParams);
        server.start();
        std::cout << "Listening on 0.0.0.0:" << port << std::endl;

        // Blocks until SIGINT / SIGTERM (docker stop), then shuts down cleanly.
        waitForTerminationRequest();
        server.stop();
        return Application::EXIT_OK;
    }
};

}  // namespace

POCO_SERVER_MAIN(App)
