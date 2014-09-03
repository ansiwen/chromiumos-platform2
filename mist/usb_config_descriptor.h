// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_CONFIG_DESCRIPTOR_H_
#define MIST_USB_CONFIG_DESCRIPTOR_H_

#include <stdint.h>

#include <ostream>  // NOLINT(readability/streams)
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>

struct libusb_config_descriptor;

namespace mist {

class UsbDevice;
class UsbInterface;

// A USB configuration descriptor, which wraps a libusb_config_descriptor C
// struct from libusb 1.0 into a C++ object.
class UsbConfigDescriptor {
 public:
  // Constructs a UsbConfigDescriptor object by taking a weak pointer to a
  // UsbDevice object as |device| and a raw pointer to a
  // libusb_config_descriptor struct as |config_descriptor|. |device| is
  // used for getting USB string descriptors related to this object. The
  // ownership of |config_descriptor| is transferred to this object if
  // |own_config_descriptor| is true. Otherwise, |config_descriptor| should
  // outlive this object.
  UsbConfigDescriptor(const base::WeakPtr<UsbDevice>& device,
                      libusb_config_descriptor* config_descriptor,
                      bool own_config_descriptor);

  // Destructs this UsbConfigDescriptor object and frees the underlying
  // libusb_config_descriptor struct if that is owned by this object.
  ~UsbConfigDescriptor();

  // Getters for retrieving fields of the libusb_config_descriptor struct.
  uint8_t GetLength() const;
  uint8_t GetDescriptorType() const;
  uint16_t GetTotalLength() const;
  uint8_t GetNumInterfaces() const;
  uint8_t GetConfigurationValue() const;
  std::string GetConfigurationDescription() const;
  uint8_t GetAttributes() const;
  uint8_t GetMaxPower() const;

  // Returns a pointer to a UsbInterface object for the USB interface indexed at
  // |index|, or a NULL pointer if the index is invalid. The returned object
  // should be deleted by the caller and should not be held beyond the lifetime
  // of this object.
  UsbInterface* GetInterface(uint8_t index) const;

  // Returns a string describing the properties of this object for logging
  // purpose.
  std::string ToString() const;

 private:
  FRIEND_TEST(UsbConfigDescriptorTest, TrivialGetters);

  base::WeakPtr<UsbDevice> device_;
  libusb_config_descriptor* config_descriptor_;
  bool own_config_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(UsbConfigDescriptor);
};

}  // namespace mist

// Output stream operator provided to facilitate logging.
std::ostream& operator<<(std::ostream& stream,
                         const mist::UsbConfigDescriptor& config_descriptor);

#endif  // MIST_USB_CONFIG_DESCRIPTOR_H_
