#include "update/http.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

#if defined(METROZERO_STATIC_LIBCURL)
#include <curl/curl.h>
#include "app/tls_common.h"
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace metrozero::update {
namespace fs = std::filesystem;

namespace {

bool cancelled(const HttpOptions& o) { return o.cancel && o.cancel->load(); }

bool url_allowed(const std::string& url, const HttpOptions& o, std::string* error) {
    if (url.rfind("https://", 0) == 0) return true;
    if (o.allow_plain_http && url.rfind("http://", 0) == 0) return true;
    if (error) *error = "Refusing to download from a non-HTTPS address.";
    return false;
}

#if defined(METROZERO_STATIC_LIBCURL)
// Streaming download state for the static libcurl backend.
struct ProgressCtx {
    const std::atomic<bool>* cancel;
    const std::function<void(std::uint64_t, std::uint64_t)>* progress;
    std::ofstream* out;
    std::uint64_t expected_total = 0;
    std::uint64_t done = 0;
};

size_t tls_write_cb(char* ptr, size_t size, size_t nmemb, void* user) {
    if (size != 0 && nmemb > size_t(-1) / size) return 0;  // multiplication overflow
    size_t n = size * nmemb;
    auto* ctx = static_cast<ProgressCtx*>(user);
    if (ctx->cancel && ctx->cancel->load()) return 0;  // abort the transfer
    ctx->out->write(ptr, std::streamsize(n));
    ctx->done += n;
    if (ctx->progress) (*ctx->progress)(ctx->done, ctx->expected_total);
    return n;
}
#endif

#if defined(_WIN32) && !defined(METROZERO_STATIC_LIBCURL)

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() { if (h) WinHttpCloseHandle(h); }
};

std::string winhttp_error(const char* what) {
    DWORD e = GetLastError();
    switch (e) {
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        case ERROR_WINHTTP_CANNOT_CONNECT:
            return "Couldn't reach the update server. Check your internet connection.";
        case ERROR_WINHTTP_TIMEOUT: return "The update server took too long to answer.";
        case ERROR_WINHTTP_SECURE_FAILURE: return "The secure connection to the update server failed.";
        default: break;
    }
    std::ostringstream s;
    s << "Download failed (" << what << ", error " << e << ").";
    return s.str();
}

#endif

}  // namespace

bool http_download(const std::string& url, const fs::path& file, const HttpOptions& opt, std::string* error) {
    if (!url_allowed(url, opt, error)) return false;
    std::error_code ec;
    fs::remove(file, ec);
#if defined(METROZERO_STATIC_LIBCURL)
    // MetroZero-XP: HTTPS via statically linked libcurl+OpenSSL. Certificates are always
    // verified against the cacert.pem next to MetroZero.exe (or METROZERO_CACERT).
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) { if (error) *error = "Couldn't write the download to disk."; return false; }
    ProgressCtx ctx{opt.cancel ? opt.cancel : nullptr, opt.progress ? &opt.progress : nullptr, &out,
                    opt.expected_size, 0};
    CURL* h = curl_easy_init();
    if (!h) {
        out.close();
        fs::remove(file, ec);
        if (error) *error = "Downloading updates failed to start libcurl.";
        return false;
    }
    char errbuf[CURL_ERROR_SIZE] = {};
    std::string why;
    if (!tls::curl_use_ca_bundle(h, &why)) {
        curl_easy_cleanup(h);
        out.close();
        fs::remove(file, ec);
        if (error) *error = why;
        return false;
    }
    curl_easy_setopt(h, CURLOPT_URL, url.c_str());
    curl_easy_setopt(h, CURLOPT_USERAGENT, opt.user_agent.c_str());
    curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(h, CURLOPT_SSL_SESSIONID_CACHE, 1L);
    curl_easy_setopt(h, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &tls_write_cb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &ctx);
    curl_slist* headers = nullptr;
    if (!opt.accept.empty()) {
        std::string accept = "Accept: " + opt.accept;
        headers = curl_slist_append(nullptr, accept.c_str());
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
    }
    CURLcode res = curl_easy_perform(h);
    long status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(h);
    out.close();
    if (!out) {
        fs::remove(file, ec);
        if (error) *error = "Couldn't write the download to disk.";
        return false;
    }
    if (opt.status_out) *opt.status_out = int(status);
    if (res == CURLE_ABORTED_BY_CALLBACK) {
        fs::remove(file, ec);
        if (error) *error = "Cancelled.";
        return false;
    }
    if (res != CURLE_OK) {
        fs::remove(file, ec);
        if (error) *error = "Download failed (" + tls::curl_error(res, errbuf) + ").";
        return false;
    }
    if (status != 200) {
        fs::remove(file, ec);
        if (error) *error = "The update server answered with HTTP " + std::to_string(status) + ".";
        return false;
    }
    if (ctx.progress) (*ctx.progress)(ctx.done, ctx.expected_total);
    return true;
#elif defined(_WIN32)
    std::wstring wurl = widen(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof uc;
    uc.dwSchemeLength = DWORD(-1);
    uc.dwHostNameLength = DWORD(-1);
    uc.dwUrlPathLength = DWORD(-1);
    uc.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        if (error) *error = "Bad download address.";
        return false;
    }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path = L"/";
    bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;

    std::wstring ua = widen(opt.user_agent);
    Handle session;
#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    session.h = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
#endif
    if (!session.h)
        session.h = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.h) { if (error) *error = winhttp_error("open"); return false; }
    WinHttpSetTimeouts(session.h, 20000, 20000, 30000, 60000);
    Handle conn;
    conn.h = WinHttpConnect(session.h, host.c_str(), uc.nPort, 0);
    if (!conn.h) { if (error) *error = winhttp_error("connect"); return false; }
    Handle req;
    req.h = WinHttpOpenRequest(conn.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!req.h) { if (error) *error = winhttp_error("request"); return false; }
    // Never follow a redirect from HTTPS to plain HTTP (this is also WinHTTP's default).
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(req.h, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy);
    std::wstring headers;
    if (!opt.accept.empty()) headers = L"Accept: " + widen(opt.accept) + L"\r\n";
    if (!WinHttpSendRequest(req.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            headers.empty() ? 0 : DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr)) {
        if (error) *error = winhttp_error("send");
        return false;
    }
    DWORD status = 0, len = sizeof status;
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &len, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        if (opt.status_out) *opt.status_out = int(status);
        if (error) *error = "The update server answered with HTTP " + std::to_string(status) + ".";
        return false;
    }
    std::uint64_t total = opt.expected_size;
    {
        wchar_t buf[32] = {};
        DWORD blen = sizeof buf;
        if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, buf, &blen,
                                WINHTTP_NO_HEADER_INDEX))
            total = std::wcstoull(buf, nullptr, 10);
    }
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) { if (error) *error = "Couldn't write the download to disk."; return false; }
    std::vector<char> buf(1 << 16);
    std::uint64_t done = 0;
    for (;;) {
        if (cancelled(opt)) {
            out.close();
            fs::remove(file, ec);
            if (error) *error = "Cancelled.";
            return false;
        }
        DWORD got = 0;
        if (!WinHttpReadData(req.h, buf.data(), DWORD(buf.size()), &got)) {
            out.close();
            fs::remove(file, ec);
            if (error) *error = winhttp_error("read");
            return false;
        }
        if (got == 0) break;
        out.write(buf.data(), got);
        done += got;
        if (opt.progress) opt.progress(done, total);
    }
    out.close();
    if (!out) { if (error) *error = "Couldn't write the download to disk."; return false; }
    return true;
#else
#if defined(__APPLE__)
    const char* curl = "/usr/bin/curl";
#else
    const char* curl = "curl";
#endif
    std::string proto = opt.allow_plain_http ? "=http,https" : "=https";
    std::string ua = opt.user_agent;
    std::string out = file.string();
    std::string accept = "Accept: " + opt.accept;
    // No --fail: the HTTP status is printed after a marker instead, because curl versions differ
    // in how they report HTTP errors (macOS curl exits with 56 rather than 22 for a 403).
    const char* status_marker = "\nMETROZERO_HTTP_STATUS=%{http_code}\n";
    std::vector<const char*> args = {curl, "--silent", "--show-error", "--location",
                                     "--write-out", status_marker,
                                     "--proto", proto.c_str(), "--proto-redir", proto.c_str(),
                                     "--connect-timeout", "20", "--retry", "2",
                                     "--speed-limit", "1024", "--speed-time", "60",
                                     "--user-agent", ua.c_str(), "--output", out.c_str()};
    if (!opt.accept.empty()) { args.push_back("--header"); args.push_back(accept.c_str()); }
    args.push_back(url.c_str());
    args.push_back(nullptr);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args.data());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* proc = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!proc) {
        if (error) *error = "Downloading updates needs the curl command, which wasn't found.";
        return false;
    }
    int code = -1;
    while (!SDL_WaitProcess(proc, false, &code)) {
        if (cancelled(opt)) {
            SDL_KillProcess(proc, true);
            SDL_WaitProcess(proc, true, &code);
            SDL_DestroyProcess(proc);
            fs::remove(file, ec);
            if (error) *error = "Cancelled.";
            return false;
        }
        if (opt.progress) {
            auto size = fs::file_size(file, ec);
            opt.progress(ec ? 0 : std::uint64_t(size), opt.expected_size);
        }
        SDL_Delay(100);
    }
    size_t n = 0;
    void* text = SDL_ReadProcess(proc, &n, nullptr);
    std::string msg = text ? std::string(static_cast<const char*>(text), n) : std::string();
    SDL_free(text);
    SDL_DestroyProcess(proc);
    int status = 0;
    if (size_t m = msg.rfind("METROZERO_HTTP_STATUS="); m != std::string::npos) {
        status = std::atoi(msg.c_str() + m + 17);
        msg.erase(m);
    }
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
    if (code == 0 && status >= 200 && status < 300) {
        if (opt.progress) {
            auto size = fs::file_size(file, ec);
            opt.progress(ec ? 0 : std::uint64_t(size), opt.expected_size);
        }
        return true;
    }
    fs::remove(file, ec);
    if (code == 0 || status >= 300) {  // the server answered, with an error
        if (opt.status_out) *opt.status_out = status;
        if (error) *error = "The update server answered with HTTP " + std::to_string(status) + ".";
        return false;
    }
    if (error) {
        switch (code) {
            case 6: case 7: *error = "Couldn't reach the update server. Check your internet connection."; break;
            case 28: *error = "The update server took too long to answer."; break;
            case 35: case 51: case 58: case 60: *error = "The secure connection to the update server failed."; break;
            case 23: *error = "Couldn't write the download to disk."; break;
            default: *error = "Download failed (curl " + std::to_string(code) + (msg.empty() ? ")." : "): " + msg); break;
        }
    }
    return false;
#endif
}

bool http_get(const std::string& url, std::string* body, const HttpOptions& opt, std::string* error) {
    std::random_device rd;
    fs::path tmp = fs::temp_directory_path() / ("metrozero-http-" + std::to_string(rd()) + ".tmp");
    HttpOptions o = opt;
    o.progress = nullptr;
    bool ok = http_download(url, tmp, o, error);
    if (ok) {
        std::ifstream f(tmp, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        *body = s.str();
    }
    std::error_code ec;
    fs::remove(tmp, ec);
    return ok;
}

}  // namespace metrozero::update
