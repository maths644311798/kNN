// Copyright 2025 Shanghai Fudan Microelectronics Group Company Limited
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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <future>
#include "fmt/format.h"
#include "yacl/link/factory.h"
#include "yacl/link/context.h"

using namespace yacl::link;

inline void Channel_Test()
{
//Inititalize
    std::vector<std::vector<std::string>> send_buffer_;
    std::vector<std::vector<std::string>> receive_buffer;
    std::vector<std::vector<std::future<void>>> futures_;
    std::vector<std::shared_ptr<Context>> ctxs_;
    size_t world_size_ = 3;

    ContextDesc ctx_desc;
    ctx_desc.recv_timeout_ms = 2000;  // 2 second
    for (size_t rank = 0; rank < world_size_; rank++) {
      const auto id = fmt::format("id-{}", rank);
      const auto host = fmt::format("host-{}", rank);
      ctx_desc.parties.push_back({id, host});
    }

    for (size_t rank = 0; rank < world_size_; rank++) {
      ctxs_.push_back(yacl::link::FactoryMem().CreateContext(ctx_desc, rank));
    }

    send_buffer_.resize(world_size_);
    receive_buffer.resize(world_size_);
    futures_.resize(world_size_);
    for (size_t sender = 0; sender < world_size_; ++sender) {
      send_buffer_[sender].resize(world_size_);
      receive_buffer[sender].resize(world_size_);
      futures_[sender].resize(world_size_);
    }

    for (size_t sender = 0; sender < world_size_; ++sender) {
        for (size_t receiver = 0; receiver < world_size_; ++receiver) {
            if (sender == receiver) {
                send_buffer_[sender][receiver] = "null";
                receive_buffer[sender][receiver] = "null";
                continue;
            }
            send_buffer_[sender][receiver] = fmt::format("Hello, {}->{}", sender, receiver);
        }
    }
//Send
    auto recv_fn = [&](size_t receiver, size_t sender) {
        receive_buffer[sender][receiver] = ctxs_[receiver]->Recv(sender, "tag");
    };
    auto send_fn = [&](size_t sender, size_t receiver, const yacl::Buffer & /*value*/) {
        ctxs_[sender]->SendAsync(
            receiver, yacl::ByteContainerView(send_buffer_[sender][receiver]), "tag");
    };
    for (size_t sender = 0; sender < world_size_; ++sender) {
        for (size_t receiver = 0; receiver < world_size_; ++receiver) {
            if (sender == receiver) {
                continue;
            }
            ctxs_[sender]->SendAsync(receiver, yacl::ByteContainerView(send_buffer_[sender][receiver]), "tag");
            futures_[sender][receiver] = std::async(recv_fn, receiver, sender);
            send_buffer_[sender][receiver] = fmt::format("Second, {}->{}", sender, receiver);
            auto _1 = std::async(send_fn, sender, receiver, yacl::Buffer(send_buffer_[sender][receiver]));
        }
    }
    for (size_t i = 0; i < world_size_; ++i) {
      for (size_t j = 0; j < world_size_; ++j) {
        if (futures_[i][j].valid()) {
          futures_[i][j].get();
          std::cout << fmt::format("{}->{} : ", i, j) << receive_buffer[i][j] << "\n";
        }
      }
    }
    std::cout << "Second round\n";
    for (size_t sender = 0; sender < world_size_; ++sender) {
        for (size_t receiver = 0; receiver < world_size_; ++receiver) {
            if (sender == receiver)
                continue;
            futures_[sender][receiver] = std::async(recv_fn, receiver, sender);
        }
    }
    for (size_t i = 0; i < world_size_; ++i) {
      for (size_t j = 0; j < world_size_; ++j) {
        if (futures_[i][j].valid()) {
          futures_[i][j].get();
          std::cout << fmt::format("{}->{} : ", i, j) << receive_buffer[i][j] << "\n";
        }
      }
    }
}

int main()
{
    Channel_Test();
    std::cout << "End\n";
    return 0;
}