#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <cstdlib>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

int main()
{
    const char* key = std::getenv("KALSHI_API_KEY");
    if(!key){
        std::cerr <<"Kalshi api key not detected, check again please.";
        return 1;
    }
    std::cout << "kalshi API key loaded.\n";

    //turning on ssl and websocket: tcp connection to kalshi websocket server
    asio::io_context ioc;
    ssl::context tls(ssl::context::tlsv12_client);
    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, tls);

    tcp::resolver resolver(ioc);
    auto endpoints = resolver.resolve("ws.kalshi.com", "443");


    
    return 0;
}