// Copyright 2025 mjbots Robotic Systems, LLC.  info@mjbots.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstring>
#include <cstdio>
#include <cctype>

#include "mjlib/base/string_span.h"
#include "mjlib/micro/async_stream.h"
#include "mjlib/multiplex/micro_datagram_server.h"

namespace moteus {

// A very small ASCII fdcanusb line protocol emulator that allows
// using the moteus Python tools and GUI over a plain UART.
//
// It implements just enough of the fdcanusb text protocol to work
// with lib/python/moteus/fdcanusb_device.py:
//
// - Host -> device:
//     "can send <hex_id> <hex_payload> [flags]\n"
//   Device replies immediately:
//     "OK\n"
//
// - Device -> host (responses):
//     "rcv <hex_id> <hex_payload> [E] [B] [F]\n"
//
// The payload is padded to the CAN-FD DLC using 0x50 bytes.
class FdcanusbAsciiMicroServer : public mjlib::multiplex::MicroDatagramServer {
 public:
  // Flags in Header::flags (mirror FDCanMicroServer)
  static constexpr uint32_t kBrsFlag = 0x01;
  static constexpr uint32_t kFdcanFlag = 0x02;

  explicit FdcanusbAsciiMicroServer(mjlib::micro::AsyncStream* stream)
      : stream_(stream) {}

  void SetPrefix(uint32_t can_prefix) { can_prefix_ = can_prefix; }

  void AsyncRead(Header* header,
                 const mjlib::base::string_span& data,
                 const mjlib::micro::SizeCallback& callback) override {
    MJ_ASSERT(!current_read_callback_);
    current_read_header_ = header;
    current_read_data_ = data;
    current_read_callback_ = callback;
  }

  void AsyncWrite(const Header& header,
                  const std::string_view& data,
                  const Header& query_header,
                  const mjlib::micro::SizeCallback& callback) override {
    // Convert to: "rcv %x HEX [E] [B] [F]\n"
    const uint32_t id =
        ((header.source & 0xff) << 8) |
        (header.destination & 0xff) |
        (can_prefix_ << 16);

    const bool brs =
        (query_header.flags & kBrsFlag) != 0;
    const bool fd =
        // If the query requested classic frame and <= 8 bytes, we
        // will not mark FD.
        !((query_header.flags & kFdcanFlag) == 0 && data.size() <= 8);

    const size_t on_wire = RoundUpDlc(data.size());

    // Ensure we have space.
    const size_t max_hex = on_wire * 2;
    const size_t needed = 4 /*rcv */ + 1 /*space*/ + 8 /*id*/ + 1 /*space*/
        + max_hex + 1 /*space*/ + 3 /*flags*/ + 2 /*\r\n*/ + 16 /*margin*/;
    if (needed > sizeof(tx_buf_)) {
      // Should never happen.
      callback(mjlib::micro::error_code(), 0);
      return;
    }

    char* p = tx_buf_;
    p += std::sprintf(p, "rcv %x ", static_cast<unsigned int>(id));

    // Hex encode payload, padding with 0x50 as required.
    size_t i = 0;
    for (; i < data.size(); i++) {
      const uint8_t v = static_cast<uint8_t>(data[i]);
      p += std::sprintf(p, "%02X", static_cast<unsigned int>(v));
    }
    for (; i < on_wire; i++) {
      p += std::sprintf(p, "%02X", 0x50u);
    }

    // Flags: E (if extended), B (if brs), F (if fd)
    // For moteus arbitration IDs with prefix, 'extended' is always true.
    *p++ = ' ';
    *p++ = 'E';
    if (brs) { *p++ = ' '; *p++ = 'B'; }
    if (fd)  { *p++ = ' '; *p++ = 'F'; }
    *p++ = '\n';

    const std::string_view out(tx_buf_, p - tx_buf_);
    stream_->AsyncWriteSome(out, callback);
  }

  Properties properties() const override {
    Properties out;
    out.max_size = 64;
    return out;
  }

  void Poll() {
    // Keep a read in flight.
    if (!read_active_) {
      read_active_ = true;
      stream_->AsyncReadSome(mjlib::base::string_span(rx_buf_),
                             [this](mjlib::micro::error_code ec, size_t size) {
        if (!ec && size > 0) { AppendRx(rx_buf_, size); }
        read_active_ = false;
      });
    }

    // Process complete lines, one at a time.
    while (true) {
      const int newline = FindNewline();
      if (newline < 0) { break; }
      HandleLine(newline);
    }
  }

 private:
  static size_t RoundUpDlc(size_t value) {
    if (value <= 8) { return value; }
    if (value <= 12) { return 12; }
    if (value <= 16) { return 16; }
    if (value <= 20) { return 20; }
    if (value <= 24) { return 24; }
    if (value <= 32) { return 32; }
    if (value <= 48) { return 48; }
    if (value <= 64) { return 64; }
    return value;
  }

  void AppendRx(const char* data, size_t size) {
    size_t to_copy = size;
    if (to_copy > (sizeof(line_buf_) - line_len_)) {
      to_copy = sizeof(line_buf_) - line_len_;
    }
    std::memcpy(line_buf_ + line_len_, data, to_copy);
    line_len_ += to_copy;
    // Drop extra if we overflow.
    if (line_len_ == sizeof(line_buf_)) {
      line_len_ = 0;
    }
  }

  int FindNewline() const {
    for (size_t i = 0; i < line_len_; i++) {
      if (line_buf_[i] == '\n' || line_buf_[i] == '\r') {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  static bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }

  void HandleLine(int newline_index) {
    // Extract the line.
    const int line_size = newline_index;
    if (line_size <= 0) {
      Consume(line_size + 1);
      return;
    }
    char tmp[192] = {};
    const int copy = (line_size >= static_cast<int>(sizeof(tmp))) ?
        (static_cast<int>(sizeof(tmp)) - 1) : line_size;
    std::memcpy(tmp, line_buf_, copy);
    tmp[copy] = 0;

    // Consume the line + following newline char.
    Consume(line_size + 1);

    // Expected: "can send <id> <hex> [flags]"
    if (std::strncmp(tmp, "can send ", 9) != 0) {
      return;
    }
    const char* p = tmp + 9;
    // Skip spaces
    while (*p && IsSpace(*p)) { p++; }
    // Parse hex id
    uint32_t id = 0;
    {
      // strtoul handles 0x or plain hex
      char* endp = nullptr;
      id = static_cast<uint32_t>(std::strtoul(p, &endp, 16));
      if (endp == p) { return; }
      p = endp;
    }
    while (*p && IsSpace(*p)) { p++; }
    // Parse hex payload
    uint8_t payload[64] = {};
    size_t payload_len = 0;
    while (payload_len < sizeof(payload) && p[0] && std::isxdigit(static_cast<unsigned char>(p[0]))) {
      if (!p[1] || !std::isxdigit(static_cast<unsigned char>(p[1]))) { break; }
      auto hex = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
      };
      payload[payload_len++] = (hex(p[0]) << 4) | hex(p[1]);
      p += 2;
    }
    // Optional flags (ignored except B/F for round-trip metadata)
    bool brs = false;
    bool fd = (payload_len > 8);
    while (*p) {
      while (*p && IsSpace(*p)) { p++; }
      if (*p == 'B') { brs = true; }
      if (*p == 'F') { fd = true; }
      while (*p && !IsSpace(*p)) { p++; }
    }

    // Deliver to MicroServer if we have a pending read.
    if (current_read_callback_) {
      current_read_header_->destination = id & 0xff;
      current_read_header_->source = (id >> 8) & 0xff;
      current_read_header_->size = payload_len;
      current_read_header_->flags = 0
          | (brs ? kBrsFlag : 0)
          | (fd ? kFdcanFlag : 0);

    // Copy bytes into the provided read span.
      const size_t to_copy = (payload_len < static_cast<size_t>(current_read_data_.size())) ?
          payload_len : static_cast<size_t>(current_read_data_.size());
      std::memcpy(current_read_data_.data(), payload, to_copy);

      auto cb = current_read_callback_;
      current_read_callback_ = {};
      current_read_header_ = {};
      current_read_data_ = {};

      cb(mjlib::micro::error_code(), payload_len);
    }

    // Respond with OK\n
    static constexpr char kOk[] = "OK\n";
    stream_->AsyncWriteSome(std::string_view(kOk, sizeof(kOk) - 1),
                            [](const mjlib::micro::error_code&, int) {});
  }

  void Consume(size_t n) {
    if (n <= 0) { return; }
    if (static_cast<size_t>(n) >= line_len_) {
      line_len_ = 0;
      return;
    }
    std::memmove(line_buf_, line_buf_ + n, line_len_ - n);
    line_len_ -= n;
  }

  mjlib::micro::AsyncStream* const stream_;
  uint32_t can_prefix_ = 0;

  // In-flight read state for MicroDatagramServer.
  Header* current_read_header_ = nullptr;
  mjlib::base::string_span current_read_data_;
  mjlib::micro::SizeCallback current_read_callback_;

  // Serial IO buffers.
  bool read_active_ = false;
  char rx_buf_[128] = {};

  // Partial line accumulation.
  char line_buf_[256] = {};
  size_t line_len_ = 0;

  // Transmit buffer (single line).
  char tx_buf_[256] = {};
};

}  // namespace moteus

