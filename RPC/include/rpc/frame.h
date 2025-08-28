#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "rpc/protocol.h"

/**
 * 帧层：长度前缀 + 固定头（MAGIC、VERSION、TYPE、REQID、METHODLEN、PAYLOADLEN）
 * 与具体 socket 读写分离（读写在 net.h 中）。
 */
namespace rpc {

struct RawFrame {
    MsgType type;
    uint32_t req_id;
    std::string method;         // 仅 Request 用
    std::vector<uint8_t> payload;
};

constexpr uint8_t VERSION = 0x01;

void build_request_frame(const Request& req, std::vector<uint8_t>& out);
void build_response_frame(const Response& rsp, std::vector<uint8_t>& out);

// 从完整的“帧体”（不含4字节长度前缀）解析出 RawFrame。
// 注意：长度前缀的读取在 net 层负责；这里仅校验MAGIC/VERSION并切出method/payload。
RawFrame parse_body_to_frame(const std::vector<uint8_t>& body);

} // namespace rpc
