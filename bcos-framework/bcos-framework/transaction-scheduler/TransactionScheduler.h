#pragma once

#include "../protocol/Block.h"
#include "bcos-task/Task.h"
#include "bcos-task/Trait.h"
#include "bcos-utilities/Ranges.h"
#include <oneapi/tbb/task_group.h>
#include <concepts>

namespace bcos::transaction_scheduler
{

struct Execute
{
    auto operator()(auto& scheduler, auto& storage, auto& executor,
        protocol::BlockHeader const& blockHeader,
        RANGES::input_range auto const& transactions) const
        -> task::Task<task::AwaitableReturnType<decltype(tag_invoke(
            *this, scheduler, storage, executor, blockHeader, transactions))>>
        requires RANGES::range<task::AwaitableReturnType<decltype(tag_invoke(
                     *this, scheduler, storage, executor, blockHeader, transactions))>> &&
                 std::convertible_to<
                     RANGES::range_value_t<task::AwaitableReturnType<decltype(tag_invoke(
                         *this, scheduler, storage, executor, blockHeader, transactions))>>,
                     protocol::TransactionReceipt::Ptr>
    {
        co_return co_await tag_invoke(
            *this, scheduler, storage, executor, blockHeader, transactions);
    }
};
inline constexpr Execute execute{};

template <auto& Tag>
using tag_t = std::decay_t<decltype(Tag)>;

}  // namespace bcos::transaction_scheduler