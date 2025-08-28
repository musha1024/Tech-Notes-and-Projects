#include "rpc/protocol.h"
#include <stdexcept>

namespace rpc {

// ------------- 基础 BE 写读 -------------
static void put_u32_be(std::vector<uint8_t>& out, uint32_t v){
    out.push_back((v>>24)&0xFF); out.push_back((v>>16)&0xFF);
    out.push_back((v>>8)&0xFF);  out.push_back((v    )&0xFF);
}
static void put_u16_be(std::vector<uint8_t>& out, uint16_t v){
    out.push_back((v>>8)&0xFF); out.push_back((v   )&0xFF);
}
static void put_i64_be(std::vector<uint8_t>& out, int64_t v){
    uint64_t u = (uint64_t)v;
    out.push_back((u>>56)&0xFF); out.push_back((u>>48)&0xFF);
    out.push_back((u>>40)&0xFF); out.push_back((u>>32)&0xFF);
    out.push_back((u>>24)&0xFF); out.push_back((u>>16)&0xFF);
    out.push_back((u>>8 )&0xFF); out.push_back((u    )&0xFF);
}
static uint32_t get_u32_be(const uint8_t* p){
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);
}
static uint16_t get_u16_be(const uint8_t* p){
    return (uint16_t(p[0])<<8)|uint16_t(p[1]);
}
static int64_t get_i64_be(const uint8_t* p){
    uint64_t u = (uint64_t(p[0])<<56)|(uint64_t(p[1])<<48)|(uint64_t(p[2])<<40)|(uint64_t(p[3])<<32)|
                 (uint64_t(p[4])<<24)|(uint64_t(p[5])<<16)|(uint64_t(p[6])<<8)|uint64_t(p[7]);
    return (int64_t)u;
}

// ------------- Value 编解码 -------------
void encode_value(std::vector<uint8_t>& out, const Value& v){
    out.push_back((uint8_t)v.type);
    if (v.type == ValueType::INT64){
        put_i64_be(out, v.i64);
    } else if (v.type == ValueType::STRING){
        put_u32_be(out, (uint32_t)v.str.size());
        out.insert(out.end(), v.str.begin(), v.str.end());
    } else {
        throw std::runtime_error("unknown ValueType");
    }
}

std::pair<Value,size_t> decode_value(const uint8_t* p, size_t n){
    if (n < 1) throw std::runtime_error("decode_value: not enough bytes");
    auto t = (ValueType)p[0];
    size_t off = 1;
    if (t == ValueType::INT64){
        if (n < off + 8) throw std::runtime_error("decode_value: need int64");
        int64_t x = get_i64_be(p+off);
        off += 8;
        return { Value::make_int(x), off };
    } else if (t == ValueType::STRING){
        if (n < off + 4) throw std::runtime_error("decode_value: need len");
        uint32_t len = get_u32_be(p+off);
        off += 4;
        if (n < off + len) throw std::runtime_error("decode_value: need str bytes");
        std::string s((const char*)p+off, len);
        off += len;
        return { Value::make_str(std::move(s)), off };
    }
    throw std::runtime_error("decode_value: bad type");
}

// ------------- Request/Response payload -------------
std::vector<uint8_t> Request::encode_payload() const {
    std::vector<uint8_t> out;
    put_u32_be(out, (uint32_t)args.size());
    for (auto& v : args) encode_value(out, v);
    return out;
}

std::vector<uint8_t> Response::encode_payload() const {
    std::vector<uint8_t> out;
    put_u16_be(out, status);
    if (status != 0){
        put_u32_be(out, (uint32_t)err_msg.size());
        out.insert(out.end(), err_msg.begin(), err_msg.end());
    } else {
        put_u32_be(out, 0);
    }
    out.push_back(has_result ? 1 : 0);
    if (has_result) encode_value(out, result);
    return out;
}

Request parse_request_payload(uint32_t req_id, const std::string& method,
                              const std::vector<uint8_t>& pl){
    Request r; r.req_id=req_id; r.method=method;
    const uint8_t* p = pl.data(); size_t n = pl.size();
    if (n < 4) throw std::runtime_error("req payload too short");
    uint32_t argc = get_u32_be(p); p+=4; n-=4;
    r.args.reserve(argc);
    for (uint32_t i=0;i<argc;++i){
        auto [val, used] = decode_value(p, n);
        r.args.push_back(std::move(val));
        p += used; n -= used;
    }
    if (n != 0) throw std::runtime_error("extra bytes in req payload");
    return r;
}

Response parse_response_payload(uint32_t req_id, const std::vector<uint8_t>& pl){
    Response rsp; rsp.req_id=req_id;
    const uint8_t* p = pl.data(); size_t n = pl.size();
    if (n < 2+4+1) throw std::runtime_error("rsp payload too short");
    rsp.status = get_u16_be(p); p+=2; n-=2;

    uint32_t err_len = get_u32_be(p); p+=4; n-=4;
    if (err_len>0){
        if (n < err_len) throw std::runtime_error("rsp err too long");
        rsp.err_msg.assign((const char*)p, err_len);
        p+=err_len; n-=err_len;
    }
    uint8_t has = *p; p+=1; n-=1;
    rsp.has_result = (has != 0);

    if (rsp.has_result){
        auto [val, used] = decode_value(p, n);
        rsp.result = std::move(val);
        p += used; n -= used;
    }
    if (n != 0) throw std::runtime_error("extra bytes in rsp payload");
    return rsp;
}

} // namespace rpc
