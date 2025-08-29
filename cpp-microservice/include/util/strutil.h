#pragma once
#include <string>
#include <optional>

inline std::optional<long long> to_ll(const std::string& s){
    try{ return std::stoll(s); }catch(...){ return std::nullopt; }
}
