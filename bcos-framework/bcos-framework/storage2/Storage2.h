#pragma once

#include "Exception.h"
#include "Iterator.h"
#include <bcos-concepts/Basic.h>
#include <bcos-concepts/ByteBuffer.h>
#include <bcos-framework/storage/Entry.h>
#include <bcos-task/Task.h>
#include <bcos-utilities/Ranges.h>
#include <boost/throw_exception.hpp>
#include <type_traits>

namespace bcos::storage2
{
using OptionalEntry = std::optional<bcos::storage::Entry>;

template <class KeysType>
concept InputKeys = RANGES::input_range<KeysType> &&
    concepts::bytebuffer::ByteBuffer<RANGES::range_value_t<KeysType>>;

template <class EntriesType>
concept InputEntries = RANGES::range<EntriesType> &&
    std::same_as<RANGES::range_value_t<EntriesType>, bcos::storage::Entry>;
template <class EntriesType>
concept OutputEntries = RANGES::output_range<EntriesType, OptionalEntry>;

// The class Impl only need impl_getRows and impl_setRows
template <class Impl>
class StorageBase
{
public:
    static constexpr std::string_view SYS_TABLES{"s_tables"};
    static constexpr std::string_view SYS_TABLE_VALUE_FIELDS{"key_field,value_fields"};

    // BEGIN: Interfaces need to impl
    task::Task<void> getRows(
        std::string_view tableName, InputKeys auto const& keys, OutputEntries auto& out)
    {
        if (RANGES::size(keys) != RANGES::size(out))
        {
            BOOST_THROW_EXCEPTION(UnmatchKeyEntries{});
        }

        return impl().impl_getRows(tableName, keys, out);
    }

    task::Task<void> setRows(
        std::string_view tableName, InputKeys auto const& keys, InputEntries auto const& entries)
    {
        if (RANGES::size(keys) != RANGES::size(entries))
        {
            BOOST_THROW_EXCEPTION(UnmatchKeyEntries{});
        }

        return impl().impl_setRows(tableName, keys, entries);
    }

    template <Iterator IteratorType>
    task::Task<IteratorType> seek(std::string_view tableName, std::string_view key)
    {
        return impl().impl_seek(tableName, key);
    }
    // END: Interfaces need to impl

    task::Task<OptionalEntry> getRow(std::string_view tableName, std::string_view key)
    {
        RANGES::single_view<decltype(key)> keys{key};

        OptionalEntry entry;
        RANGES::single_view<decltype(entry)> entries(entry);

        co_await impl().impl_getRows(tableName, keys, entries);
        co_return entry;
    }

    task::Task<void> setRow(
        std::string_view tableName, std::string_view key, bcos::storage::Entry entry)
    {
        RANGES::single_view<decltype(key)> keys{key};
        RANGES::single_view<decltype(entry)> entries(entry);

        co_await impl().impl_setRows(tableName, keys, entries);
    }

    task::Task<void> createTable(std::string_view tableName)
    {
        auto entry = co_await getRow(SYS_TABLES, tableName);
        if (entry)
        {
            BOOST_THROW_EXCEPTION(-1);
        }

        storage::Entry tableEntry;
        // TODO: Write value fields
        // ...
        co_await setRow(SYS_TABLES, tableName, std::move(tableEntry));
    }

private:
    friend Impl;
    auto& impl() { return static_cast<Impl&>(*this); }

    StorageBase() = default;
};

template <class Impl>
concept Storage = std::derived_from<Impl, StorageBase<Impl>>;

}  // namespace bcos::storage2