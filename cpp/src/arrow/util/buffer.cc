// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "arrow/util/buffer.h"

#include <cstdint>
#include <limits>

#include "arrow/util/logging.h"
#include "arrow/util/memory-pool.h"
#include "arrow/util/status.h"

namespace arrow {

namespace {
int64_t RoundUpToMultipleOf64(int64_t num) {
  DCHECK_GE(num, 0);
  constexpr int64_t round_to = 64;
  constexpr int64_t force_carry_addend = round_to - 1;
  constexpr int64_t truncate_bitmask = ~(round_to - 1);
  constexpr int64_t max_roundable_num = std::numeric_limits<int64_t>::max() - round_to;
  if (num <= max_roundable_num) { return (num + force_carry_addend) & truncate_bitmask; }
  // handle overflow case.  This should result in a malloc error upstream
  return num;
}
}  // namespace

Buffer::Buffer(const std::shared_ptr<Buffer>& parent, int64_t offset, int64_t size) {
  data_ = parent->data() + offset;
  size_ = size;
  parent_ = parent;
  capacity_ = size;
}

Buffer::~Buffer() {}

std::shared_ptr<Buffer> MutableBuffer::GetImmutableView() {
  return std::make_shared<Buffer>(this->get_shared_ptr(), 0, size());
}

PoolBuffer::PoolBuffer(MemoryPool* pool) : ResizableBuffer(nullptr, 0) {
  if (pool == nullptr) { pool = default_memory_pool(); }
  pool_ = pool;
}

PoolBuffer::~PoolBuffer() {
  if (mutable_data_ != nullptr) { pool_->Free(mutable_data_, capacity_); }
}

Status PoolBuffer::Reserve(int64_t new_capacity) {
  if (!mutable_data_ || new_capacity > capacity_) {
    uint8_t* new_data;
    new_capacity = RoundUpToMultipleOf64(new_capacity);
    if (mutable_data_) {
      RETURN_NOT_OK(pool_->Allocate(new_capacity, &new_data));
      memcpy(new_data, mutable_data_, size_);
      pool_->Free(mutable_data_, capacity_);
    } else {
      RETURN_NOT_OK(pool_->Allocate(new_capacity, &new_data));
    }
    mutable_data_ = new_data;
    data_ = mutable_data_;
    capacity_ = new_capacity;
  }
  return Status::OK();
}

Status PoolBuffer::Resize(int64_t new_size) {
  RETURN_NOT_OK(Reserve(new_size));
  size_ = new_size;
  return Status::OK();
}

}  // namespace arrow
