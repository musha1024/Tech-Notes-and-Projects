#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "rpc/value.h"

/**
 * 协议层：定义消息类型、请求/响应结构、payload 编解码
 * 与传输层（frame/net）解耦：这里只关心“如何把Value打成字节/从字节解出”。
 */
namespace rpc {

enum class MsgType : uint8_t {
    REQUEST  = 1,
    RESPONSE = 2,
    ERROR    = 3, // 预留
};

struct Request {
    uint32_t req_id{};
    std::string method;
    std::vector<Value> args;
    std::vector<uint8_t> encode_payload() const;
};

struct Response {
    uint32_t req_id{};
    uint16_t status{};     // 0=OK, 非0=错误
    std::string err_msg;   // 错误文本
    bool has_result{false};
    Value result;

    std::vector<uint8_t> encode_payload() const;
};

// Value 编解码（对 payload 内部）
void encode_value(std::vector<uint8_t>& out, const Value& v);
std::pair<Value,size_t> decode_value(const uint8_t* p, size_t n);

// payload 解析
Request  parse_request_payload(uint32_t req_id, const std::string& method,
                               const std::vector<uint8_t>& pl);
Response parse_response_payload(uint32_t req_id,
                                const std::vector<uint8_t>& pl);

} // namespace rpc
