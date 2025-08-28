#include "rpc/frame.h"
#include <cstring>
#include <stdexcept>

namespace rpc {

static const uint8_t MAGIC[4] = {'R','P','C','1'};

static void put_u32_be(std::vector<uint8_t>& out, uint32_t v){
    out.push_back((v>>24)&0xFF); out.push_back((v>>16)&0xFF);
    out.push_back((v>>8)&0xFF);  out.push_back((v    )&0xFF);
}
static uint32_t get_u32_be(const uint8_t* p){
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);
}

void build_request_frame(const Request& req, std::vector<uint8_t>& out){
    std::vector<uint8_t> payload = req.encode_payload();

    std::vector<uint8_t> body;
    body.insert(body.end(), MAGIC, MAGIC+4);
    body.push_back(VERSION);
    body.push_back((uint8_t)MsgType::REQUEST);
    // req id
    put_u32_be(body, req.req_id);
    // method len & payload len
    put_u32_be(body, (uint32_t)req.method.size());
    put_u32_be(body, (uint32_t)payload.size());
    // method bytes
    body.insert(body.end(), req.method.begin(), req.method.end());
    // payload bytes
    body.insert(body.end(), payload.begin(), payload.end());

    out.clear();
    put_u32_be(out, (uint32_t)body.size()); // 前置长度（不含自己）
    out.insert(out.end(), body.begin(), body.end());
}

void build_response_frame(const Response& rsp, std::vector<uint8_t>& out){
    std::vector<uint8_t> payload = rsp.encode_payload();

    std::vector<uint8_t> body;
    body.insert(body.end(), MAGIC, MAGIC+4);
    body.push_back(VERSION);
    body.push_back((uint8_t)MsgType::RESPONSE);
    put_u32_be(body, rsp.req_id);
    put_u32_be(body, 0); // no method for response
    put_u32_be(body, (uint32_t)payload.size());
    body.insert(body.end(), payload.begin(), payload.end());

    out.clear();
    put_u32_be(out, (uint32_t)body.size());
    out.insert(out.end(), body.begin(), body.end());
}

RawFrame parse_body_to_frame(const std::vector<uint8_t>& body){
    if (body.size() < 4+1+1+4+4+4) throw std::runtime_error("bad frame len");
    if (std::memcmp(body.data(), MAGIC, 4) != 0) throw std::runtime_error("bad magic");
    if (body[4] != VERSION) throw std::runtime_error("bad version");
    auto type = (MsgType)body[5];

    const uint8_t* p = body.data() + 6;
    uint32_t req_id = get_u32_be(p); p += 4;
    uint32_t method_len = get_u32_be(p); p += 4;
    uint32_t payload_len = get_u32_be(p); p += 4;

    if ((size_t)(p - body.data()) + method_len + payload_len != body.size())
        throw std::runtime_error("bad sizes");

    std::string method;
    if (method_len){
        method.assign((const char*)p, method_len);
        p += method_len;
    }

    std::vector<uint8_t> payload;
    if (payload_len){
        payload.assign(p, p+payload_len);
        p += payload_len;
    }

    return RawFrame{type, req_id, std::move(method), std::move(payload)};
}

} // namespace rpc
