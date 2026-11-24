#pragma once
// MetroZero-XP static-TLS build: locate the CA bundle used by libcurl+OpenSSL.
//
// Only meaningful when METROZERO_STATIC_LIBCURL is defined (the Windows XP / MinGW build).
// The bundle is not compiled into the binary: it ships next to MetroZero.exe as cacert.pem
// (produced by the XP workflow from https://curl.se/ca/cacert.pem). The METROZERO_CACERT
// environment variable overrides the location, which is useful on the developer machine.
//
// HTTPS verification is mandatory in this build: if no bundle is found, requests fail with a
// clear error instead of silently skipping verification.

#if defined(METROZERO_STATIC_LIBCURL)

#include <curl/curl.h>

#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <string>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace metrozero::tls {

namespace detail {
#if defined(_WIN32)
inline std::string wide_to_ansi(const wchar_t* wide, int wide_length) {
    if (wide_length <= 0) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, wide, wide_length, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(size_t(n), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide, wide_length, out.data(), n, nullptr, nullptr);
    return out;
}
#endif

inline std::string exe_dir_path() {
#if defined(_WIN32)
    wchar_t buf[32768] = {};
    DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(std::size(buf)));
    if (n == 0 || n >= std::size(buf)) return {};
    wchar_t* slash = wcsrchr(buf, L'\\');
    if (!slash) return {};
    return wide_to_ansi(buf, int(slash + 1 - buf));
#else
    char buf[4096] = {};
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    std::string s(buf);
    size_t slash = s.rfind('/');
    return slash == std::string::npos ? std::string() : s.substr(0, slash + 1);
#endif
}

inline bool file_exists_readable(const std::string& path) {
    if (path.empty()) return false;
    std::ifstream f(path, std::ios::binary);
    return f.good();
}
}  // namespace detail

// Path of the CA bundle to use for HTTPS verification, or "" if none was found.
inline const std::string& cacert_pem_path() {
    static const std::string path = [] {
        if (const char* env = std::getenv("METROZERO_CACERT"); env && *env)
            return std::string(env);
        std::string dir = detail::exe_dir_path();
        if (dir.empty()) return std::string();
#if defined(_WIN32)
        std::string candidate = dir + "cacert.pem";
#else
        std::string candidate = dir + "cacert.pem";
#endif
        if (detail::file_exists_readable(candidate)) return candidate;
        return std::string();
    }();
    return path;
}

// Points curl at the CA bundle. Returns false (and an explanatory message in `why`) when no
// bundle is available; the caller must fail the request in that case.
inline bool curl_use_ca_bundle(CURL* h, std::string* why) {
    const std::string& path = cacert_pem_path();
    if (path.empty()) {
        if (why)
            *why = "HTTPS certificates cannot be verified: cacert.pem was not found next to MetroZero.exe "
                   "(or in the METROZERO_CACERT environment variable).";
        return false;
    }
    curl_easy_setopt(h, CURLOPT_CAINFO, path.c_str());
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 2L);
    return true;
}

// Maps a curl result to a short, user-facing reason (Portuguese-neutral English like the rest
// of the app). Falls back to curl_easy_strerror for details.
inline std::string curl_error(CURLcode result, const char* fallback) {
    if (fallback && *fallback) return fallback;
    const char* msg = curl_easy_strerror(result);
    if (result == CURLE_PEER_FAILED_VERIFICATION)
        return std::string("The server's certificate could not be verified (") + (msg ? msg : "?") +
               "). Check the date and time on this computer.";
    return msg ? std::string(msg) : std::string("unknown network error");
}

}  // namespace metrozero::tls

#endif  // METROZERO_STATIC_LIBCURL
