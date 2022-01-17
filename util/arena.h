/ Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace leveldb {

class Arena {
 public:
  Arena();

  // 不允许copy&赋值
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  ~Arena();

  // Return a pointer to a newly allocated memory block of "bytes" bytes.
  char* Allocate(size_t bytes);

  // Allocate memory with the normal alignment guarantees provided by malloc.
  char* AllocateAligned(size_t bytes);

  // Returns an estimate of the total memory usage of data allocated
  // by the arena.
  size_t MemoryUsage() const {
    // 仅保证原子性不保证读写有序
    return memory_usage_.load(std::memory_order_relaxed);
  }

 private:
  char* AllocateFallback(size_t bytes);
  char* AllocateNewBlock(size_t block_bytes);

  // Allocation state
  char* alloc_ptr_;
  size_t alloc_bytes_remaining_;

  // Array of new[] allocated memory blocks
  // 每个块大小不是固定的, 但大小至少是 kBlockSize/4 以上
  std::vector<char*> blocks_;

  // Total memory usage of the arena.
  // 包括没有使用的, 只要new申请了内存都会算入memory_usage_
  //
  // TODO(costan): This member is accessed via atomics, but the others are
  //               accessed without any locking. Is this OK?
  std::atomic<size_t> memory_usage_;
};

// 1. 查看当前块是否满足需求, 满足的话返回指针, 调整内部指针
// 2. 假如当前块不满足需求, 进入 AllocateFallback 
//    2.1 假如请求的内存大于 kBlockSize / 4, 那么会根据请求内存大小申请块
//        这么做的目的是避免浪费大块空闲内存, 假设申请 2kb 内存, 当前块仅剩下 1kb
//        那么直接走 2.2 会浪费掉 1kb 内存
//    2.2 直接申请 kBlockSize 大小的内存
//        当前块的内存丢弃, 当前块指针重置为新申请的内存首地址
inline char* Arena::Allocate(size_t bytes) {
  // The semantics of what to return are a bit messy if we allow
  // 0-byte allocations, so we disallow them here (we don't need
  // them for our internal use).
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_

