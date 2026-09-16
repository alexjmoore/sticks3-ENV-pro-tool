#pragma once

#include <platform/DeviceInstanceInfoProvider.h>
#include <lib/support/CHIPMemString.h>

namespace m5stack {

class StickS3DeviceInfoProvider : public chip::DeviceLayer::DeviceInstanceInfoProvider {
public:
    StickS3DeviceInfoProvider() {}

    void init() {
        // Explicitly set this provider without calling GetDeviceInstanceInfoProvider()
        // which calls abort() if invoked prior to ConfigurationManager initialization.
        chip::DeviceLayer::SetDeviceInstanceInfoProvider(this);
    }

    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "M5Stack");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetVendorId(uint16_t & vendorId) override {
        vendorId = 0xFFF1; // Standard test/development Vendor ID
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "StickS3-PRO-Env");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductId(uint16_t & productId) override {
        productId = 0x8001; // Standard test/development Product ID
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "StickS3-BME688");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "https://m5stack.com");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "M5Stack StickS3 Environmental Monitor");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "M5S3-ENV-2026");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override {
        year = 2026;
        month = 9;
        day = 16;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override {
        hardwareVersion = 1;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override {
        chip::Platform::CopyString(buf, bufSize, "v1.0-ESP32S3");
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan & uniqueIdSpan) override {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    CHIP_ERROR GetProductFinish(chip::app::Clusters::BasicInformation::ProductFinishEnum * finish) override {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    CHIP_ERROR GetProductPrimaryColor(chip::app::Clusters::BasicInformation::ColorEnum * primaryColor) override {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }
};

} // namespace m5stack
