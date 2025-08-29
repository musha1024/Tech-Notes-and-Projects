#include "rpc/value.h"
#include <stdexcept>

namespace rpc {

// =====================================================
// Value 工厂函数
// 功能：构造不同类型的 Value（INT64 / STRING）
// 输入：int64_t 或 std::string
// 输出：封装好的 Value 对象
// 失败：无
// =====================================================
Value Value::make_int(int64_t v){
    Value x; x.type = ValueType::INT64; x.i64 = v; return x;
}
Value Value::make_str(std::string s){
    Value x; x.type = ValueType::STRING; x.str = std::move(s); return x;
}

// =====================================================
// as_i64(args, i)
// 功能：从参数数组中第 i 个位置读取 int64
// 输入：a 参数向量，i 位置
// 输出：int64_t 值
// 失败：
//   - i 越界 → throw runtime_error("missing arg")
//   - 类型不为 INT64 → throw runtime_error("arg type not int64")
// =====================================================
int64_t as_i64(const std::vector<Value>& a, size_t i){
    if (i >= a.size()) throw std::runtime_error("missing arg");
    if (a[i].type != ValueType::INT64) throw std::runtime_error("arg type not int64");
    return a[i].i64;
}

// =====================================================
// as_str(args, i)
// 功能：从参数数组中第 i 个位置读取 string
// 输入：a 参数向量，i 位置
// 输出：std::string（值返回，会拷贝一次）
// 失败：
//   - i 越界 → throw runtime_error("missing arg")
//   - 类型不为 STRING → throw runtime_error("arg type not string")
// 说明：当前按值返回，简单直接；如需减少拷贝，见下方“可选优化”。
// =====================================================
std::string as_str(const std::vector<Value>& a, size_t i){
    if (i >= a.size()) throw std::runtime_error("missing arg");
    if (a[i].type != ValueType::STRING) throw std::runtime_error("arg type not string");
    return a[i].str;
}

} // namespace rpc
