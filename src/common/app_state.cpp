#include "app_state.h"
#include "app_common.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <cctype>

namespace {

std::wstring ExeDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    auto pos = dir.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : dir.substr(0, pos + 1);
}

// ------------------------------------------------------------------
// 极简 JSON 解析器：仅用于读取本模块自身写出的状态文件
// ------------------------------------------------------------------
struct JVal {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool b = false;
    double num = 0;
    std::string str;
    std::map<std::string, JVal> obj;

    bool isObj() const { return type == OBJ; }
    const JVal* find(const std::string& key) const {
        if (type != OBJ) return nullptr;
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    bool asBool(bool def) const { return type == BOOL ? b : def; }
    int asInt(int def) const { return type == NUM ? static_cast<int>(num) : def; }
};

void skipWs(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) ++p;
}

JVal parseValue(const std::string& s, size_t& p);

JVal parseString(const std::string& s, size_t& p) {
    JVal v; v.type = JVal::STR;
    ++p;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\' && p + 1 < s.size()) {
            ++p;
            switch (s[p]) {
                case '"':  v.str += '"';  break;
                case '\\': v.str += '\\'; break;
                case 'n':  v.str += '\n'; break;
                case 'r':  v.str += '\r'; break;
                case 't':  v.str += '\t'; break;
                case '/':  v.str += '/';  break;
                default:   v.str += s[p];
            }
        } else {
            v.str += s[p];
        }
        ++p;
    }
    if (p < s.size()) ++p;
    return v;
}

JVal parseNumber(const std::string& s, size_t& p) {
    size_t st = p;
    if (p < s.size() && s[p] == '-') ++p;
    while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p] == '.' ||
                            s[p] == 'e' || s[p] == 'E' || s[p] == '+' || s[p] == '-')) ++p;
    JVal v; v.type = JVal::NUM;
    try { v.num = std::stod(s.substr(st, p - st)); } catch (...) { v.num = 0; }
    return v;
}

JVal parseObject(const std::string& s, size_t& p) {
    JVal v; v.type = JVal::OBJ;
    ++p;
    skipWs(s, p);
    if (p < s.size() && s[p] == '}') { ++p; return v; }
    while (p < s.size()) {
        skipWs(s, p);
        if (p >= s.size() || s[p] != '"') break;
        JVal key = parseString(s, p);
        skipWs(s, p);
        if (p < s.size() && s[p] == ':') ++p;
        v.obj[key.str] = parseValue(s, p);
        skipWs(s, p);
        if (p < s.size() && s[p] == ',') { ++p; continue; }
        if (p < s.size() && s[p] == '}') { ++p; break; }
        break;
    }
    return v;
}

JVal parseValue(const std::string& s, size_t& p) {
    skipWs(s, p);
    if (p >= s.size()) return JVal();
    char c = s[p];
    if (c == '"') return parseString(s, p);
    if (c == '{') return parseObject(s, p);
    if (c == 't') { if (s.compare(p, 4, "true") == 0) p += 4; JVal v; v.type = JVal::BOOL; v.b = true; return v; }
    if (c == 'f') { if (s.compare(p, 5, "false") == 0) p += 5; JVal v; v.type = JVal::BOOL; v.b = false; return v; }
    if (c == 'n') { if (s.compare(p, 4, "null") == 0) p += 4; return JVal(); }
    if (c == '[') {
        // 本模块不使用数组，跳过以保持解析器健壮
        JVal v; v.type = JVal::ARR;
        int depth = 0;
        while (p < s.size()) { if (s[p] == '[') ++depth; else if (s[p] == ']') { --depth; if (depth == 0) { ++p; break; } } ++p; }
        return v;
    }
    return parseNumber(s, p);
}

} // namespace

std::wstring AppState::filePath() const {
    return ExeDir() + L"AudioFlux.state.json";
}

void AppState::load() {
    loaded_ = true;
    std::wstring path = filePath();
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    if (content.empty()) return;

    size_t p = 0;
    JVal root = parseValue(content, p);
    if (!root.isObj()) return;

    if (const JVal* settings = root.find("settings")) {
        if (const JVal* tray = settings->find("trayEnabled")) {
            tray_enabled_ = tray->asBool(tray_enabled_);
        }
        if (const JVal* autoStart = settings->find("autoStartEnabled")) {
            auto_start_enabled_ = autoStart->asBool(auto_start_enabled_);
        }
    }
    if (const JVal* devices = root.find("devices")) {
        if (devices->isObj()) {
            for (const auto& kv : devices->obj) {
                const JVal& d = kv.second;
                if (!d.isObj()) continue;
                DeviceState st;
                if (const JVal* e = d.find("enabled")) st.enabled = e->asBool(true);
                if (const JVal* h = d.find("hpf")) st.hpf = h->asInt(0);
                if (const JVal* l = d.find("lpf")) st.lpf = l->asInt(0);
                devices_[kv.first] = st;
            }
        }
    }
}

void AppState::save() const {
    std::string json = "{\n  \"version\": 1,\n";
    json += "  \"settings\": { \"trayEnabled\": ";
    json += tray_enabled_ ? "true" : "false";
    json += ", \"autoStartEnabled\": ";
    json += auto_start_enabled_ ? "true" : "false";
    json += " },\n";
    json += "  \"devices\": {";
    bool first = true;
    for (const auto& kv : devices_) {
        json += first ? "\n" : ",\n";
        first = false;
        json += "    \"" + EscapeJson(kv.first) + "\": { \"enabled\": ";
        json += kv.second.enabled ? "true" : "false";
        json += ", \"hpf\": " + std::to_string(kv.second.hpf);
        json += ", \"lpf\": " + std::to_string(kv.second.lpf);
        json += " }";
    }
    json += first ? "}\n}\n" : "\n  }\n}\n";

    std::wstring path = filePath();
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
}

void AppState::setTrayEnabled(bool enabled) {
    tray_enabled_ = enabled;
}

void AppState::setAutoStartEnabled(bool enabled) {
    auto_start_enabled_ = enabled;
}

bool AppState::hasDevice(const std::string& id) const {
    return devices_.find(id) != devices_.end();
}

DeviceState AppState::getDevice(const std::string& id) const {
    auto it = devices_.find(id);
    return it == devices_.end() ? DeviceState{} : it->second;
}

void AppState::setDevice(const std::string& id, const DeviceState& state) {
    devices_[id] = state;
}
