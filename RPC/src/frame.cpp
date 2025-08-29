#include "rpc/frame.h"
#include <cstring>
#include <stdexcept>

namespace rpc {

// ======================= 模块功能总览 =======================
// 本文件负责把高层的 Request/Response 结构体 ↔ 字节序列（frame）。
// 采用“长度前缀 + body”格式：
//
// [4B body_len][ body ]
//
// body 布局（大端/BE）统一如下：
//   MAGIC(4B) = "RPC1"
//   VERSION(1B)
//   TYPE(1B) : 1=Request, 2=Response
//   REQ_ID(4B, BE)
//   METHOD_LEN(4B, BE)   // Response 固定为 0
//   PAYLOAD_LEN(4B, BE)
//   METHOD (METHOD_LEN bytes)
//   PAYLOAD(PAYLOAD_LEN bytes)
//
// 其中：
//  - Request 的 payload 来自 Request::encode_payload()
//  - Response 的 payload 来自 Response::encode_payload()
//  - parse_body_to_frame() 只负责把 body 解析成 RawFrame；
//    更高层的 parse_*_payload() 再把 payload 还原为具体对象。
// ============================================================

static const uint8_t MAGIC[4] = {'R','P','C','1'};

// 写入/读取 32 位大端整数
static void put_u32_be(std::vector<uint8_t>& out, uint32_t v){
    out.push_back((v>>24)&0xFF); out.push_back((v>>16)&0xFF);
    out.push_back((v>>8)&0xFF);  out.push_back((v    )&0xFF);
}
static uint32_t get_u32_be(const uint8_t* p){
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);
}

// ======================= Request → Frame =======================
// 输入：高层 Request（含 req_id、method、args）
// 输出：out = 一整帧字节（含 4B 前置 body_len）
// 失败：抛出 std::runtime_error（一般不会，除非 encode_payload 内部抛）
// ==============================================================
void build_request_frame(const Request& req, std::vector<uint8_t>& out){
    // 1) 让请求对象生成 payload（如：参数序列化后的二进制）
    std::vector<uint8_t> payload = req.encode_payload();

    // 2) 组装 body
    std::vector<uint8_t> body;
    body.insert(body.end(), MAGIC, MAGIC+4);          // MAGIC
    body.push_back(VERSION);                          // VERSION
    body.push_back((uint8_t)MsgType::REQUEST);        // TYPE
    put_u32_be(body, req.req_id);                     // REQ_ID
    put_u32_be(body, (uint32_t)req.method.size());    // METHOD_LEN
    put_u32_be(body, (uint32_t)payload.size());       // PAYLOAD_LEN
    body.insert(body.end(), req.method.begin(), req.method.end());   // METHOD
    body.insert(body.end(), payload.begin(), payload.end());         // PAYLOAD

    // 3) 前置长度（不含自身 4 字节）
    out.clear();
    put_u32_be(out, (uint32_t)body.size());
    out.insert(out.end(), body.begin(), body.end());
}

// ======================= Response → Frame =======================
// 与上面类似，但 METHOD_LEN 固定写 0（响应没有方法名段）
// ==============================================================
void build_response_frame(const Response& rsp, std::vector<uint8_t>& out){
    std::vector<uint8_t> payload = rsp.encode_payload();

    std::vector<uint8_t> body;
    body.insert(body.end(), MAGIC, MAGIC+4);
    body.push_back(VERSION);
    body.push_back((uint8_t)MsgType::RESPONSE);
    put_u32_be(body, rsp.req_id);
    put_u32_be(body, 0);                               // METHOD_LEN = 0
    put_u32_be(body, (uint32_t)payload.size());        // PAYLOAD_LEN
    body.insert(body.end(), payload.begin(), payload.end());

    out.clear();
    put_u32_be(out, (uint32_t)body.size());
    out.insert(out.end(), body.begin(), body.end());
}

// ======================= 解析 body → RawFrame =======================
// 输入：完整的 body（注意：不包含最前面的 4B body_len）
// 输出：RawFrame {type, req_id, method, payload}
// 失败：抛出 std::runtime_error（magic/version/长度不合法等）
// ==============================================================
RawFrame parse_body_to_frame(const std::vector<uint8_t>& body){
    // 基本长度判断：至少包含头部字段
    // 4(MAGIC)+1(VERSION)+1(TYPE)+4(REQ_ID)+4(METHOD_LEN)+4(PAYLOAD_LEN) = 18 字节
    if (body.size() < 4+1+1+4+4+4)
        throw std::runtime_error("bad frame len");

    // MAGIC
    if (std::memcmp(body.data(), MAGIC, 4) != 0)
        throw std::runtime_error("bad magic");

    // VERSION
    if (body[4] != VERSION)
        throw std::runtime_error("bad version");

    // TYPE
    auto type = (MsgType)body[5];

    // 解析主头字段
    const uint8_t* p = body.data() + 6;
    uint32_t req_id      = get_u32_be(p); p += 4;
    uint32_t method_len  = get_u32_be(p); p += 4;
    uint32_t payload_len = get_u32_be(p); p += 4;

    // 边界一致性检查：头 + method + payload 应该正好等于 body.size()
    if ((size_t)(p - body.data()) + method_len + payload_len != body.size())
        throw std::runtime_error("bad sizes");

    // 读取 METHOD（Response 的 method_len=0，则跳过）
    std::string method;
    if (method_len){
        method.assign((const char*)p, method_len);
        p += method_len;
    }

    // 读取 PAYLOAD
    std::vector<uint8_t> payload;
    if (payload_len){
        payload.assign(p, p+payload_len);
        p += payload_len;
    }

    return RawFrame{type, req_id, std::move(method), std::move(payload)};
}

} // namespace rpc
