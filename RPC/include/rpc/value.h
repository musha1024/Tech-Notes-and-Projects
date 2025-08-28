#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * Value：RPC 参数/返回值的统一承载体。
 * 仅演示两种类型：INT64、STRING。便于后续扩展（double/bytes/map/array）。
 */
namespace rpc {

enum class ValueType : uint8_t {
    INT64  = 1,
    STRING = 2,
};

struct Value {
    ValueType type{};
    int64_t   i64{};
    std::string str;

    static Value make_int(int64_t v);
    static Value make_str(std::string s);
};

// 简单的类型检查工具（服务端 handler 读参数时常用）
int64_t as_i64(const std::vector<Value>& a, size_t i);
std::string as_str(const std::vector<Value>& a, size_t i);

} // namespace rpc
