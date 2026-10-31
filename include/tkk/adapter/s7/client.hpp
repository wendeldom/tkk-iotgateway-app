// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <memory>
#include <snap7micro/s7_micro_client.h>

class ISnap7Client {

public:
  ISnap7Client() = default;
  virtual ~ISnap7Client() = default;
  ISnap7Client(const ISnap7Client &) = delete;
  auto operator=(const ISnap7Client &) -> ISnap7Client & = delete;
  ISnap7Client(ISnap7Client &&) = delete;
  auto operator=(ISnap7Client &&) -> ISnap7Client & = delete;

  [[nodiscard]] virtual auto ConnectTo(const char *rem_address, int rack, int slot) -> int = 0;
  virtual auto Reset(bool do_reconnect) -> int = 0;
  virtual auto Disconnect() -> int = 0;
  [[nodiscard]] virtual auto ReadArea(int area, int db_number, int start, int amount, int word_len,
                                      void *p_usr_data) -> int = 0;
  [[nodiscard]] virtual auto ReadMultiVars(PS7DataItem item, int items_count) -> int = 0;
  [[nodiscard]] virtual auto GetPduLength() -> int = 0;
  [[nodiscard]] virtual auto Connected() -> bool = 0;
};

class Snap7Client final : public ISnap7Client {

public:
  Snap7Client() : client_{std::make_unique<TSnap7MicroClient>()} {}

  [[nodiscard]] auto ConnectTo(const char *rem_address, int rack, int slot) -> int override {
    return client_->ConnectTo(rem_address, rack, slot);
  }

  auto Reset(bool do_reconnect) -> int override { return client_->Reset(do_reconnect); }

  auto Disconnect() -> int override { return client_->Disconnect(); }

  [[nodiscard]] auto ReadArea(int area, int db_number, int start, int amount, int word_len,
                              void *p_usr_data) -> int override {
    return client_->ReadArea(area, db_number, start, amount, word_len, p_usr_data);
  }

  [[nodiscard]] auto ReadMultiVars(PS7DataItem item, int items_count) -> int override {
    return client_->ReadMultiVars(item, items_count);
  }

  [[nodiscard]] auto GetPduLength() -> int override { return client_->PDULength; }

  [[nodiscard]] auto Connected() -> bool override { return client_->Connected; }

private:
  std::unique_ptr<TSnap7MicroClient> client_;
};