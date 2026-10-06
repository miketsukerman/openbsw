/********************************************************************************
 * Copyright (c) 2026 An Dao
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/**
 * USB descriptors for the single CDC-ACM console device.
 *
 * VID/PID: this uses the pid.codes test PID 0x1209:0x0001, which is
 * explicitly reserved for testing and must NOT be used for products that
 * leave the lab. Replace with a properly allocated VID/PID before
 * distributing binaries (see https://pid.codes).
 */

#include "tusb.h"

#define USB_VID 0x1209U // pid.codes
#define USB_PID 0x0001U // pid.codes test PID - lab use only

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

enum
{
    EPNUM_CDC_NOTIF = 0x81,
    EPNUM_CDC_OUT   = 0x02,
    EPNUM_CDC_IN    = 0x82
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static tusb_desc_device_t const deviceDescriptor
    = {.bLength         = sizeof(tusb_desc_device_t),
       .bDescriptorType = TUSB_DESC_DEVICE,
       .bcdUSB          = 0x0200,

       // Use Interface Association Descriptor (IAD) for CDC
       .bDeviceClass    = TUSB_CLASS_MISC,
       .bDeviceSubClass = MISC_SUBCLASS_COMMON,
       .bDeviceProtocol = MISC_PROTOCOL_IAD,

       .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

       .idVendor  = USB_VID,
       .idProduct = USB_PID,
       .bcdDevice = 0x0100,

       .iManufacturer = 0x01,
       .iProduct      = 0x02,
       .iSerialNumber = 0x03,

       .bNumConfigurations = 0x01};

uint8_t const* tud_descriptor_device_cb(void) { return (uint8_t const*)&deviceDescriptor; }

static uint8_t const configurationDescriptor[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface number, string index, EP notification address and size,
    // EP data address (out, in) and size (512 bytes for bulk at high speed).
    TUD_CDC_DESCRIPTOR(
        ITF_NUM_CDC,
        4,
        EPNUM_CDC_NOTIF,
        8,
        EPNUM_CDC_OUT,
        EPNUM_CDC_IN,
        (TUD_OPT_HIGH_SPEED ? 512 : 64)),
};

#if TUD_OPT_HIGH_SPEED
// Same interface layout at the other (full) speed, only the bulk endpoint
// size differs.
static uint8_t const otherSpeedConfigurationDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

// Device qualifier: mandatory for high-speed capable devices.
static tusb_desc_device_qualifier_t const deviceQualifierDescriptor
    = {.bLength            = sizeof(tusb_desc_device_qualifier_t),
       .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
       .bcdUSB             = 0x0200,
       .bDeviceClass       = TUSB_CLASS_MISC,
       .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
       .bDeviceProtocol    = MISC_PROTOCOL_IAD,
       .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
       .bNumConfigurations = 0x01,
       .bReserved          = 0x00};

uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
    return (uint8_t const*)&deviceQualifierDescriptor;
}

static uint8_t otherSpeedBuffer[CONFIG_TOTAL_LEN];

uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
    (void)index;
    memcpy(otherSpeedBuffer, otherSpeedConfigurationDescriptor, CONFIG_TOTAL_LEN);
    otherSpeedBuffer[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
    return otherSpeedBuffer;
}
#endif

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return configurationDescriptor;
}

// String descriptors
static char const* stringDescriptors[] = {
    (char const[]){0x09, 0x04}, // 0: language: English (0x0409)
    "Eclipse OpenBSW",          // 1: manufacturer
    "OpenBSW Console",          // 2: product
    "000001",                   // 3: serial number
    "OpenBSW CDC Console",      // 4: CDC interface
};

static uint16_t descriptorString[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    uint8_t charCount;

    if (index == 0)
    {
        memcpy(&descriptorString[1], stringDescriptors[0], 2);
        charCount = 1;
    }
    else
    {
        if (index >= (sizeof(stringDescriptors) / sizeof(stringDescriptors[0])))
        {
            return NULL;
        }

        char const* str = stringDescriptors[index];

        charCount = (uint8_t)strlen(str);
        if (charCount > 31)
        {
            charCount = 31;
        }

        // Convert ASCII to UTF-16
        for (uint8_t i = 0; i < charCount; i++)
        {
            descriptorString[1 + i] = (uint16_t)str[i];
        }
    }

    // first byte is length (including header), second byte is descriptor type
    descriptorString[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2U * charCount + 2U));

    return descriptorString;
}
