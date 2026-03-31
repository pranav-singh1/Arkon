#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <nlohmann/json.hpp>
#include "writer.hpp"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <cstdio>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl   = asio::ssl;
using tcp  = asio::ip::tcp;
using json = nlohmann::json;

static const std::string KALSHI_HOST    = "api.elections.kalshi.com";
static const std::string KALSHI_PORT    = "443";
static const std::string KALSHI_WS_PATH = "/trade-api/ws/v2";

static std::string base64_encode(const unsigned char* data, size_t len)
{
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

//proving i own the private key
static std::string sign_rsa_pss(EVP_PKEY* pkey, const std::string& message)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_PKEY_CTX* pctx = nullptr;

    if (EVP_DigestSignInit(ctx, &pctx, EVP_sha256(), nullptr, pkey) <= 0)
        throw std::runtime_error("DigestSignInit failed");

    EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING);
    EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1); // salt len = digest len (SHA-256 = 32)

    if (EVP_DigestSignUpdate(ctx, message.data(), message.size()) <= 0)
        throw std::runtime_error("DigestSignUpdate failed");

    size_t sig_len = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sig_len);
    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSignFinal(ctx, sig.data(), &sig_len) <= 0)
        throw std::runtime_error("DigestSignFinal failed");

    EVP_MD_CTX_free(ctx);
    return base64_encode(sig.data(), sig_len);
}

static EVP_PKEY* load_private_key(const char* path)
{
    FILE* f = std::fopen(path, "r");
    if (!f) {
        std::cerr << "Cannot open PEM file: " << path << "\n";
        return nullptr;
    }
    EVP_PKEY* pkey = PEM_read_PrivateKey(f, nullptr, nullptr, nullptr);
    std::fclose(f);
    return pkey;
}

int main()
{
    TickWriter writer("ticks.jsonl");
    const char* api_key = std::getenv("KALSHI_API_KEY");
    if (!api_key) {
        std::cerr << "KALSHI_API_KEY not set\n";
        return 1;
    }
    std::cout << "Kalshi API key loaded.\n";

    // --- Load RSA private key ---
    const char* pem_path = std::getenv("KALSHI_RSA_KEY_PATH");
    if (!pem_path) pem_path = "kalshi_private_key.pem";

    EVP_PKEY* pkey = load_private_key(pem_path);
    if (!pkey) {
        std::cerr << "Failed to load RSA private key from " << pem_path << "\n";
        return 1;
    }

    // --- Generate auth headers ---
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string timestamp = std::to_string(now_ms);
    std::string sig_input = timestamp + "GET" + KALSHI_WS_PATH;
    std::string signature = sign_rsa_pss(pkey, sig_input);
    EVP_PKEY_free(pkey);

    // --- TLS + WebSocket setup ---
    asio::io_context ioc;
    ssl::context tls(ssl::context::tlsv12_client);
    tls.set_default_verify_paths();

    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, tls);

    // SNI — required so the server presents the right certificate
    SSL_set_tlsext_host_name(ws.next_layer().native_handle(), KALSHI_HOST.c_str());

    // --- TCP connect + TLS handshake ---
    tcp::resolver resolver(ioc);
    auto endpoints = resolver.resolve(KALSHI_HOST, KALSHI_PORT);
    asio::connect(beast::get_lowest_layer(ws), endpoints);
    // After asio::connect, before ws.next_layer().handshake()
    auto& sock = beast::get_lowest_layer(ws);
    struct timeval tv;
    tv.tv_sec = 30;   // 30 second timeout
    tv.tv_usec = 0;
    setsockopt(sock.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ws.next_layer().handshake(ssl::stream_base::client);

    // --- WebSocket upgrade with auth headers ---
    ws.set_option(websocket::stream_base::decorator(
        [&](websocket::request_type& req) {
            req.set("KALSHI-ACCESS-KEY",       api_key);
            req.set("KALSHI-ACCESS-SIGNATURE", signature);
            req.set("KALSHI-ACCESS-TIMESTAMP", timestamp);
        }));

    ws.handshake(KALSHI_HOST, KALSHI_WS_PATH);
    std::cout << "Connected to Kalshi WebSocket!\n";

    // --- Subscribe to ticker (public channel, all markets) ---
    json sub_msg = {
        {"id",  1},
        {"cmd", "subscribe"},
        {"params", {
            {"channels", {"ticker"}}
        }}
    };
    ws.write(asio::buffer(sub_msg.dump()));
    std::cout << "Subscribed to ticker channel.\n";

    // --- Read loop ---
    beast::flat_buffer buffer;
    while (true) {

        buffer.clear();

        //also need to catch if no data arrives for 30 seconds and then handle
        try {
            ws.read(buffer);
        } catch (const beast::system_error& e) {
            std::cerr << "WebSocket read error: " << e.what() << "\n";
            break;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            break;          
        }


        std::string msg = beast::buffers_to_string(buffer.data());
        json parsed = json::parse(msg);
        writer.write(parsed);
        std::cout << parsed.dump(2) << "\n";
    }
    return 0;
}
