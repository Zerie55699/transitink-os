#pragma once

#include <cstddef>
#include <cstring>
#include <vector>

#include "Arduino.h"

class Preferences {
public:
    bool begin(const char*, bool) {
        return true;
    }

    String getString(const char*, const char* defaultValue) const {
        return hasValue_ ? value_ : String(defaultValue);
    }

    std::size_t putString(const char*, const String& value) {
        if (failWrites_) {
            return 0;
        }
        value_ = value;
        hasValue_ = true;
        ++writeCount_;
        return value.length();
    }

    std::size_t getBytesLength(const char*) const {
        return hasBlobValue_ ? blobValue_.size() : 0;
    }

    std::size_t getBytes(const char*, void* buffer, std::size_t maxLength) const {
        if (!hasBlobValue_ || maxLength < blobValue_.size()) {
            return 0;
        }
        std::memcpy(buffer, blobValue_.data(), blobValue_.size());
        return blobValue_.size();
    }

    std::size_t putBytes(const char*, const void* value, std::size_t length) {
        if (failWrites_) {
            return 0;
        }
        const auto* bytes = static_cast<const unsigned char*>(value);
        blobValue_.assign(bytes, bytes + length);
        hasBlobValue_ = true;
        ++writeCount_;
        return length;
    }

    bool getBool(const char*, bool defaultValue = false) const {
        return hasBoolValue_ ? boolValue_ : defaultValue;
    }

    std::size_t putBool(const char*, bool value) {
        if (failWrites_) {
            return 0;
        }
        boolValue_ = value;
        hasBoolValue_ = true;
        ++boolWriteCount_;
        return 1;
    }

    bool clear() {
        value_ = "";
        hasValue_ = false;
        blobValue_.clear();
        hasBlobValue_ = false;
        boolValue_ = false;
        hasBoolValue_ = false;
        return true;
    }

    static void seedTestValue(const String& value) {
        value_ = value;
        hasValue_ = true;
        blobValue_.clear();
        hasBlobValue_ = false;
        writeCount_ = 0;
    }

    static const String& testValue() {
        return value_;
    }

    static std::size_t testWriteCount() {
        return writeCount_;
    }

    static void seedTestBool(bool value) {
        boolValue_ = value;
        hasBoolValue_ = true;
        boolWriteCount_ = 0;
    }

    static std::size_t testBoolWriteCount() {
        return boolWriteCount_;
    }

    static void setTestWriteFailure(bool fail) {
        failWrites_ = fail;
    }

private:
    inline static String value_;
    inline static bool hasValue_ = false;
    inline static std::size_t writeCount_ = 0;
    inline static std::vector<unsigned char> blobValue_;
    inline static bool hasBlobValue_ = false;
    inline static bool boolValue_ = false;
    inline static bool hasBoolValue_ = false;
    inline static std::size_t boolWriteCount_ = 0;
    inline static bool failWrites_ = false;
};
