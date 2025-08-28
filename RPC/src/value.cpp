#include "rpc/value.h"
#include <stdexcept>

namespace rpc {

Value Value::make_int(int64_t v){ Value x; x.type=ValueType::INT64; x.i64=v; return x; }
Value Value::make_str(std::string s){ Value x; x.type=ValueType::STRING; x.str=std::move(s); return x; }

int64_t as_i64(const std::vector<Value>& a, size_t i){
    if (i >= a.size()) throw std::runtime_error("missing arg");
    if (a[i].type != ValueType::INT64) throw std::runtime_error("arg type not int64");
    return a[i].i64;
}
std::string as_str(const std::vector<Value>& a, size_t i){
    if (i >= a.size()) throw std::runtime_error("missing arg");
    if (a[i].type != ValueType::STRING) throw std::runtime_error("arg type not string");
    return a[i].str;
}

} // namespace rpc
