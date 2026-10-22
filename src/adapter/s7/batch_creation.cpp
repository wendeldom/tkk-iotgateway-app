// SPDX-FileCopyrightText: 2026 Dominik Wendel
// SPDX-License-Identifier: AGPL-3.0-only

#include "tkk/adapter/s7/batch_creation.hpp"
#include "tkk/adapter/s7/config_key.hpp"
#include "tkk/adapter/s7/helper.hpp"
#include "tkk/adapter/s7/type.hpp"
#include "tkk/core/util.hpp"

namespace tkk::s7 {
namespace {
[[nodiscard]] auto ExceedsAreaLimit(const TagItem &tag, size_t amount) -> bool {
  return (detail::GetItemReadSize(tag) + tag.offset) > amount;
}
} // namespace

auto Creator::Create() -> Result<ConfigItem> {

  if (!util::InRange(pdu_length_, limit::kS7MinPdu, limit::kS7MaxPdu)) {
    return Error(ErrC::kBatchError, ErrA::kReconnect, "pdu out of bounds")
        .With("value", pdu_length_)
        .With("min", limit::kS7MinPdu)
        .With("max", limit::kS7MaxPdu)
        .Fail();
  }

  CreateAdapterCtx();
  if (auto res = CreateReadConfig(); !res) {
    return (std::move(res).error().Fail());
  }
  SetupBatches();
  SetupS7Items();

  return std::move(config_item_);
}

auto Creator::CreateAdapterCtx() -> void {
  namespace key = detail::key;

  const auto &adap = config_.at(key::kAdapter);
  config_item_.ctx.enabled = adap.at(key::kEnabled);

  const auto &poll = config_.at(key::kPolling);
  config_item_.ctx.poll_interval = poll.at(key::kIntervalMs);

  const auto &recon = config_.at(key::kReconnect);
  config_item_.ctx.init_reconn_delay = recon.at(key::kInitialDelayMs);
  config_item_.ctx.max_reconn_delay = recon.at(key::kMaxDelayMs);
  config_item_.ctx.reconn_factor = recon.at(key::kFactor);
}

auto Creator::CreateReadConfig() -> Result<void> {
  namespace key = detail::key;
  using util::GetStr;

  Result<void> res{};

  const auto &blocks = config_.at(key::kRead);

  for (const auto &block : blocks) {
    auto mode = detail::StringToReadMode(GetStr(block, key::kMode));

    switch (mode) {
    case ReadMode::kArea:
      res = AreaCreator{block, config_item_.batches}.Create();
      break;
    case ReadMode::kMulti:
      res = MultiCreator{block, config_item_.batches, static_cast<size_t>(pdu_length_)}.Create();
      break;
    case ReadMode::kUnset:
      assert(false);
      std::abort();
    }
    if (!res) {
      return res;
    }
  }

  return {};
}

auto Creator::SetupBatches() -> void {
  U32 batch_no{0};

  for (auto &batch : config_item_.batches) {
    batch.header.batch_no = batch_no;

    batch.buffer = std::vector<U8>(batch.header.read_buffer_size);
    batch.data_points.reserve(batch.header.item_count);

    config_item_.total_read_size += batch.header.read_buffer_size;
    config_item_.total_dp += batch.tags.size();
    config_item_.total_batch++;
    batch_no++;
  }
}

auto Creator::SetupS7Items() -> void {
  using enum ReadMode;

  // Iterate over all batches and set the data pointers
  for (auto &batch : config_item_.batches) {

    // Only required for multi read batches
    if (batch.header.mode == ReadMode::kArea) {
      continue;
    }

    batch.snap7_data_items.reserve(batch.header.item_count);
    size_t offset{0};

    for (const auto &tag : batch.tags) {

      TS7DataItem item{};
      item.Area = tag.target;
      item.WordLen = S7WLByte;
      item.DBNumber = tag.number;

      // Amount for timer/counter set to 1 and not size of item (snap7 specific)
      switch (item.Area) {
      case S7AreaTM:
      case S7AreaCT:
        item.Start = tag.number;
        item.Amount = 1;
        break;
      default:
        item.Start = tag.offset;
        item.Amount = static_cast<int>(detail::GetItemReadSize(tag));
        break;
      }
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      item.pdata = batch.buffer.data() + offset;
      batch.snap7_data_items.push_back(item);

      offset += detail::GetItemReadSize(tag);
    }
  }
}

auto AreaCreator::Create() -> Result<void> {
  namespace key = detail::key;
  using util::GetStr;

  ReadBatch batch{};

  const auto &params = config_.at(key::kAreaParams);

  batch.header.mode = ReadMode::kArea;
  batch.header.target = detail::StringToSnap7Area(GetStr(params, key::kAreaTarget));
  // Only target = "db" has number field
  if (batch.header.target == S7AreaDB) {
    batch.header.number = params.at(key::kNumber);
  }
  batch.header.offset = params.at(key::kOffset);
  batch.header.read_buffer_size = params.at(key::kAmount);

  const auto &tags = config_.at(key::kTags);
  batch.tags.reserve(tags.size());
  for (const auto &tag : tags) {
    TagItem tag_item = CreateTagItem(tag);
    if (ExceedsAreaLimit(tag_item, batch.header.read_buffer_size)) {
      return Error(ErrC::kBatchError, ErrA::kFatal, "tag offset exceeds area")
          .With("at id", tag_item.id)
          .With("limit", batch.header.read_buffer_size)
          .Fail();
    }
    batch.tags.push_back(std::move(tag_item));
  }
  batches_.push_back(std::move(batch));
  return {};
}

auto AreaCreator::CreateTagItem(const Json &tag) -> TagItem {
  namespace key = detail::key;
  using util::GetStr;

  TagItem item{};

  item.id = tag.at(key::kId);
  item.name = tag.at(key::kName);
  item.offset = tag.at(key::kOffset);
  item.type = detail::StringToDataType(GetStr(tag, key::kDataType));

  switch (item.type) {
  case DataType::kString:
  case DataType::kWstring:
    item.length = tag.at(key::kLength);
    break;
  case DataType::kBool:
    item.bit = tag.at(key::kBit);
    break;
  default:
    break;
  }
  return item;
}

auto MultiCreator::Create() -> Result<void> {
  namespace key = detail::key;

  ReadBatch batch{};
  TagItem tag_item{};
  constexpr size_t kDefaultPduSizeRcv{14};
  constexpr size_t kDefaultPduSizeSnd{12};
  constexpr size_t kItemHdrSizeRcv{4};
  constexpr size_t kRequestHdrSize{12};
  constexpr size_t kItemSizeSnd{12};
  size_t item_size_rcv{0};
  size_t pdu_size_rcv{kDefaultPduSizeRcv};
  size_t max_item_count{0};

  max_item_count = (pdu_length_ - kDefaultPduSizeSnd) / kItemSizeSnd;

  const auto &tags = config_.at(key::kTags);

  for (const auto &tag : tags) {

    tag_item = CreateTagItem(tag);
    item_size_rcv = kItemHdrSizeRcv + detail::GetItemReadSize(tag_item);
    if (item_size_rcv % 2 != 0) {
      item_size_rcv++; // Must be 2 byte aligned
    }

    // Check if every item fits at least in a whole recieve PDU
    if (item_size_rcv + kDefaultPduSizeRcv > pdu_length_) {
      return Error(ErrC::kBatchError, ErrA::kFatal, "item too big for pdu")
          .With("max", pdu_length_)
          .With("is", item_size_rcv + kDefaultPduSizeRcv)
          .Fail();
    }
    // Create an new Read config if pdu size/max items is over the limit
    // Request max size is calcualted via max item count
    // Response max size is calcualted via pdu size
    if (batch.header.item_count >= max_item_count || (pdu_size_rcv + item_size_rcv) > pdu_length_) {

      batch.header.request_pdu_size = kRequestHdrSize + (batch.header.item_count * kItemSizeSnd);
      batch.header.response_pdu_size = pdu_size_rcv;

      batches_.push_back(std::move(batch));

      batch = ReadBatch{};               // Reset to empty batch
      pdu_size_rcv = kDefaultPduSizeRcv; // Reset to default pdu size
    }

    pdu_size_rcv += item_size_rcv;
    batch.header.item_count++;
    batch.header.read_buffer_size += detail::GetItemReadSize(tag_item);
    batch.tags.push_back(tag_item);
  }

  // Split the last batch and add it into the batch container
  if (!batch.tags.empty()) {
    batch.header.request_pdu_size = kRequestHdrSize + (batch.header.item_count * kItemSizeSnd);
    batch.header.response_pdu_size = pdu_size_rcv;
    batches_.push_back(std::move(batch));
  }

  return {};
}

auto MultiCreator::CreateTagItem(const Json &tag) -> TagItem {
  namespace key = detail::key;
  using util::GetStr;

  TagItem item{};

  item.id = tag.at(key::kId);
  item.name = tag.at(key::kName);
  item.target = detail::StringToSnap7Area(GetStr(tag, key::kTarget));

  switch (item.target) {
  case S7AreaDB:
    item.number = tag.at(key::kNumber);
    item.offset = tag.at(key::kOffset);
    item.type = detail::StringToDataType(GetStr(tag, key::kDataType));
    break;
  case S7AreaPE:
  case S7AreaPA:
  case S7AreaMK:
    item.offset = tag.at(key::kOffset);
    item.type = detail::StringToDataType(GetStr(tag, key::kDataType));
    break;
  case S7AreaTM:
    item.number = tag.at(key::kNumber);
    item.type = DataType::kTimer;
    break;
  case S7AreaCT:
    item.number = tag.at(key::kNumber);
    item.type = DataType::kCounter;
    break;
  default:
    break;
  }

  switch (item.type) {
  case DataType::kString:
  case DataType::kWstring:
    item.length = tag.at(key::kLength);
    break;
  case DataType::kBool:
    item.bit = tag.at(key::kBit);
    break;
  default:
    break;
  }

  return item;
}

} // namespace tkk::s7