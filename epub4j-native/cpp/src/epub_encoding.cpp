/**
 * epub_encoding.cpp - Encoding Detection and Conversion Implementation
 *
 * Uses uchardet for encoding detection and iconv (POSIX) / Win32 API
 * for UTF-8 conversion. All dependencies are statically linked.
 */

#include "epub_native.h"
#include "epub_native_internal.h"
#include <cstdlib>
#include <cstring>
#include <uchardet.h>

#ifdef _WIN32
#include <windows.h>
#include <vector>
#else
#include <iconv.h>
#include <cerrno>
#endif

// ============================================================================
// Internal Structures
// ============================================================================

struct EpubNativeEncodingDetector {
    uchardet_t handle;
    std::string last_error;
};

#ifdef _WIN32
// Map common encoding names to Windows code pages
static UINT encoding_to_codepage(const char* name) {
    struct Mapping { const char* name; UINT cp; };
    static const Mapping mappings[] = {
        {"UTF-8", 65001}, {"UTF8", 65001},
        {"ASCII", 20127}, {"US-ASCII", 20127},
        {"ISO-8859-1", 28591}, {"LATIN1", 28591},
        {"ISO-8859-2", 28592}, {"ISO-8859-15", 28605},
        {"WINDOWS-1250", 1250}, {"WINDOWS-1251", 1251},
        {"WINDOWS-1252", 1252}, {"WINDOWS-1253", 1253},
        {"WINDOWS-1254", 1254}, {"WINDOWS-1255", 1255},
        {"WINDOWS-1256", 1256},
        {"SHIFT_JIS", 932}, {"SHIFT-JIS", 932},
        {"EUC-JP", 51932}, {"EUC-KR", 51949},
        {"GB2312", 936}, {"GBK", 936}, {"GB18030", 54936},
        {"BIG5", 950}, {"KOI8-R", 20866}, {"KOI8-U", 21866},
    };
    for (const auto& m : mappings) {
        if (_stricmp(name, m.name) == 0) return m.cp;
    }
    return 65001; // Fallback to UTF-8
}
#endif

// ============================================================================
// Encoding Detector Implementation
// ============================================================================

EPUB_NATIVE_API EpubNativeError epub_native_encoding_detector_create(
    EpubNativeEncodingDetector** out_detector
) {
    if (!out_detector) {
        set_error("Invalid argument: null pointer");
        return EPUB_NATIVE_ERROR_INVALID_ARG;
    }

    try {
        auto detector = new EpubNativeEncodingDetector();
        detector->handle = uchardet_new();
        if (!detector->handle) {
            set_error("Failed to create uchardet detector");
            delete detector;
            return EPUB_NATIVE_ERROR_MEMORY;
        }

        *out_detector = detector;
        return EPUB_NATIVE_SUCCESS;

    } catch (const std::bad_alloc&) {
        set_error("Memory allocation failed");
        return EPUB_NATIVE_ERROR_MEMORY;
    } catch (...) {
        set_error("Unknown error creating encoding detector");
        return EPUB_NATIVE_ERROR_MEMORY;
    }
}

EPUB_NATIVE_API void epub_native_encoding_detector_free(
    EpubNativeEncodingDetector* detector
) {
    if (detector) {
        if (detector->handle) {
            uchardet_delete(detector->handle);
        }
        delete detector;
    }
}

EPUB_NATIVE_API EpubNativeError epub_native_detect_encoding(
    EpubNativeEncodingDetector* detector,
    const char* data,
    size_t data_length,
    char** out_encoding,
    int* out_confidence
) {
    if (!detector || !data || !out_encoding || !out_confidence) {
        set_error("Invalid argument: null pointer");
        return EPUB_NATIVE_ERROR_INVALID_ARG;
    }

    try {
        uchardet_reset(detector->handle);

        int rv = uchardet_handle_data(detector->handle, data, data_length);
        if (rv != 0) {
            set_error("uchardet_handle_data failed");
            return EPUB_NATIVE_ERROR_PARSE;
        }

        uchardet_data_end(detector->handle);

        const char* charset = uchardet_get_charset(detector->handle);
        if (!charset || charset[0] == '\0') {
            // Detection failed; fall back to UTF-8
            *out_encoding = duplicate_cstring("UTF-8");
            *out_confidence = 0;
        } else {
            *out_encoding = duplicate_cstring(charset);
            *out_confidence = 100;
        }

        if (!*out_encoding) {
            set_error("Memory allocation failed");
            return EPUB_NATIVE_ERROR_MEMORY;
        }

        return EPUB_NATIVE_SUCCESS;

    } catch (const std::bad_alloc&) {
        set_error("Memory allocation failed");
        return EPUB_NATIVE_ERROR_MEMORY;
    } catch (...) {
        set_error("Unknown error detecting encoding");
        return EPUB_NATIVE_ERROR_PARSE;
    }
}

EPUB_NATIVE_API EpubNativeError epub_native_convert_to_utf8(
    const char* source_encoding,
    const char* source_data,
    size_t source_length,
    char** out_utf8,
    size_t* out_utf8_length
) {
    if (!source_encoding || !source_data || !out_utf8 || !out_utf8_length) {
        set_error("Invalid argument: null pointer");
        return EPUB_NATIVE_ERROR_INVALID_ARG;
    }

#ifdef _WIN32
    try {
        UINT source_cp = encoding_to_codepage(source_encoding);

        // Source encoding → wide (UTF-16)
        int wlen = MultiByteToWideChar(source_cp, 0, source_data,
            static_cast<int>(source_length), nullptr, 0);
        if (wlen <= 0) {
            set_error("MultiByteToWideChar failed for source encoding");
            return EPUB_NATIVE_ERROR_PARSE;
        }

        std::vector<wchar_t> wbuf(static_cast<size_t>(wlen));
        MultiByteToWideChar(source_cp, 0, source_data,
            static_cast<int>(source_length), wbuf.data(), wlen);

        // Wide (UTF-16) → UTF-8
        int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), wlen,
            nullptr, 0, nullptr, nullptr);
        if (ulen <= 0) {
            set_error("WideCharToMultiByte failed for UTF-8 conversion");
            return EPUB_NATIVE_ERROR_PARSE;
        }

        char* result = static_cast<char*>(malloc(static_cast<size_t>(ulen) + 1));
        if (!result) {
            set_error("Memory allocation failed");
            return EPUB_NATIVE_ERROR_MEMORY;
        }

        WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), wlen,
            result, ulen, nullptr, nullptr);
        result[ulen] = '\0';

        *out_utf8 = result;
        *out_utf8_length = static_cast<size_t>(ulen);
        return EPUB_NATIVE_SUCCESS;

    } catch (const std::bad_alloc&) {
        set_error("Memory allocation failed");
        return EPUB_NATIVE_ERROR_MEMORY;
    } catch (...) {
        set_error("Unknown error converting to UTF-8");
        return EPUB_NATIVE_ERROR_PARSE;
    }
#else
    try {
        iconv_t cd = iconv_open("UTF-8", source_encoding);
        if (cd == (iconv_t)(-1)) {
            std::string err = "iconv_open failed for encoding: ";
            err += source_encoding;
            set_error(err.c_str());
            return EPUB_NATIVE_ERROR_INVALID_ARG;
        }

        // Allocate output buffer (worst case: 4x expansion for single-byte → UTF-8)
        size_t out_size = source_length * 4 + 4;
        char* out_buf = static_cast<char*>(malloc(out_size));
        if (!out_buf) {
            iconv_close(cd);
            set_error("Memory allocation failed");
            return EPUB_NATIVE_ERROR_MEMORY;
        }

        char* in_ptr = const_cast<char*>(source_data);
        size_t in_left = source_length;
        char* out_ptr = out_buf;
        size_t out_left = out_size - 1;

        size_t result = iconv(cd, &in_ptr, &in_left, &out_ptr, &out_left);
        iconv_close(cd);

        if (result == static_cast<size_t>(-1)) {
            free(out_buf);
            set_error("iconv conversion to UTF-8 failed");
            return EPUB_NATIVE_ERROR_PARSE;
        }

        size_t converted = static_cast<size_t>(out_ptr - out_buf);
        out_buf[converted] = '\0';

        *out_utf8 = out_buf;
        *out_utf8_length = converted;
        return EPUB_NATIVE_SUCCESS;

    } catch (const std::bad_alloc&) {
        set_error("Memory allocation failed");
        return EPUB_NATIVE_ERROR_MEMORY;
    } catch (...) {
        set_error("Unknown error converting to UTF-8");
        return EPUB_NATIVE_ERROR_PARSE;
    }
#endif
}
