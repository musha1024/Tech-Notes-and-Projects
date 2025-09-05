#pragma once
#include <string>
#include <algorithm>
#include <regex>
#include <cstdio>
#include <cctype>

inline std::string url_decode(const std::string &src) {
    std::string ret;
    char ch;
    int ii;
    for (size_t i = 0; i < src.length(); i++) {
        if (src[i] == '%') {
            if (i + 2 < src.size() && std::isxdigit(src[i+1]) && std::isxdigit(src[i+2])) {
                sscanf(src.substr(i + 1, 2).c_str(), "%x", &ii);
                ch = static_cast<char>(ii);
                ret += ch;
                i += 2;
            }
        } else if (src[i] == '+') {
            ret += ' ';
        } else {
            ret += src[i];
        }
    }
    return ret;
}

inline std::string json_escape(std::string s) {
    std::string out; out.reserve(s.size()+8);
    for(char c: s){
        switch(c){
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if(static_cast<unsigned char>(c) < 0x20){
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c & 0xFF);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// Minimal JSON helpers for flat objects: {"key":"...","value":"...","ttl":123,"name":"..."}
// NOT a full JSON parser.
inline bool json_get_string(const std::string& body, const std::string& key, std::string& out) {
    auto kpos = body.find("\"" + key + "\"");
    if (kpos == std::string::npos) return false;
    auto colon = body.find(':', kpos);
    if (colon == std::string::npos) return false;
    size_t i = colon + 1;
    while (i < body.size() && isspace(body[i])) ++i;
    if (i < body.size() && body[i] == '"') {
        auto q1 = i;
        auto q2 = body.find('"', q1+1);
        if (q2 == std::string::npos) return false;
        out = body.substr(q1+1, q2-q1-1);
        return true;
    }
    return false;
}

inline bool json_get_int(const std::string& body, const std::string& key, int& out) {
    auto kpos = body.find("\"" + key + "\"");
    if (kpos == std::string::npos) return false;
    auto colon = body.find(':', kpos);
    if (colon == std::string::npos) return false;
    size_t start = colon + 1;
    while (start < body.size() && isspace(body[start])) start++;
    size_t end = start;
    while (end < body.size() && (isdigit(body[end]) || body[end]=='-')) end++;
    if (end == start) return false;
    try { out = std::stoi(body.substr(start, end-start)); return true; }
    catch(...) { return false; }
}

// key validation: printable, limited length
inline bool is_valid_key(const std::string& k) {
    if(k.empty() || k.size() > 255) return false;
    for(char c: k){
        if(static_cast<unsigned char>(c) < 32 || c == '\"') return false;
    }
    return true;
}

// table name validation: only letters/digits/_ and length <= 64
inline bool is_valid_table(const std::string& t){
    static const std::regex re("^[A-Za-z0-9_]{1,64}$");
    return std::regex_match(t, re);
}