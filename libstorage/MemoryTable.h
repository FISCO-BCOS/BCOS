/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/** @file MemoryTable.h
 *  @author ancelmo
 *  @date 20180921
 */
#pragma once

#include "MemoryTable.h"
#include "Storage.h"
#include "Table.h"
#include <json/json.h>
#include <libdevcore/Guards.h>
#include <libdevcore/easylog.h>
#include <libdevcrypto/Hash.h>
#include <libprecompiled/Common.h>
#include <tbb/concurrent_unordered_map.h>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/lexical_cast.hpp>
#include <type_traits>

namespace dev
{
namespace storage
{
template <typename Mode = Serial>
class MemoryTable : public Table
{
public:
    /*
    using CacheType = typename std::conditional<Mode::value,
        eth::ThreadSafeMap<std::string, Entries::Ptr>, std::map<std::string, Entries::Ptr>>::type;
    */
    using CacheType = typename std::conditional<Mode::value,
        tbb::concurrent_unordered_map<std::string, Entries::Ptr>,
        std::map<std::string, Entries::Ptr>>::type;
    using CacheItr = typename CacheType::iterator;
    using Ptr = std::shared_ptr<MemoryTable<Mode>>;

    virtual ~MemoryTable(){};

    virtual typename Entries::Ptr select(const std::string& key, Condition::Ptr condition) override
    {
        try
        {
            typename Entries::Ptr entries = std::make_shared<Entries>();

            CacheItr it;
            it = m_cache.find(key);
            if (it == m_cache.end())
            {
                if (m_remoteDB)
                {
                    entries = m_remoteDB->select(m_blockHash, m_blockNum, m_tableInfo->name, key);
                    m_cache.insert(std::make_pair(key, entries));
                    // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB
                    // selects")
                    //                    << LOG_KV("key", key) << LOG_KV("records",
                    //                    entries->size());
                }
            }
            else
            {
                entries = it->second;
            }

            if (!entries)
            {
                // STORAGE_LOG(DEBUG) << LOG_BADGE("MemoryTable") << LOG_DESC("Can't find data");
                return std::make_shared<Entries>();
            }
            auto indexes = processEntries(entries, condition);
            typename Entries::Ptr resultEntries = std::make_shared<Entries>();
            for (auto& i : indexes)
            {
                resultEntries->addEntry(entries->get(i));
            }
            return resultEntries;
        }
        catch (std::exception& e)
        {
            // STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("Table select failed for")
            //                   << LOG_KV("msg", boost::diagnostic_information(e));
        }

        return std::make_shared<Entries>();
    }

    virtual int update(const std::string& key, Entry::Ptr entry, Condition::Ptr condition,
        AccessOptions::Ptr options = std::make_shared<AccessOptions>()) override
    {
        try
        {
            if (options->check && !checkAuthority(options->origin))
            {
                // STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("update
                // non-authorized")
                //                     << LOG_KV("origin", options->origin.hex()) << LOG_KV("key",
                //                     key);
                return storage::CODE_NO_AUTHORIZED;
            }
            // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("update") << LOG_KV("key",
            // key);

            typename Entries::Ptr entries = std::make_shared<Entries>();

            CacheItr it;
            {
                // ReadGuard l(x_cache);
                it = m_cache.find(key);
            }
            if (it == m_cache.end())
            {
                if (m_remoteDB)
                {
                    entries = m_remoteDB->select(m_blockHash, m_blockNum, m_tableInfo->name, key);
                    m_cache.insert(std::make_pair(key, entries));
                    // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB
                    // selects")
                    //                    << LOG_KV("key", key) << LOG_KV("records",
                    //                    entries->size());
                }
            }
            else
            {
                entries = it->second;
            }

            if (!entries)
            {
                // STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("Can't find data");
                return 0;
            }
            checkField(entry);
            auto indexes = processEntries(entries, condition);
            std::vector<Change::Record> records;

            for (auto& i : indexes)
            {
                Entry::Ptr updateEntry = entries->get(i);
                for (auto& it : *(entry->fields()))
                {
                    records.emplace_back(i, it.first, updateEntry->getField(it.first));
                    updateEntry->setField(it.first, it.second);
                }
            }
            this->m_recorder(this->shared_from_this(), Change::Update, key, records);

            entries->setDirty(true);

            return indexes.size();
        }
        catch (std::exception& e)
        {
            STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable")
                               << LOG_DESC("Access MemoryTable failed for")
                               << LOG_KV("msg", boost::diagnostic_information(e));
        }

        return 0;
    }

    virtual int insert(const std::string& key, Entry::Ptr entry,
        AccessOptions::Ptr options = std::make_shared<AccessOptions>(),
        bool needSelect = true) override
    {
        try
        {
            if (options->check && !checkAuthority(options->origin))
            {
                // STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("insert
                // non-authorized")
                //                     << LOG_KV("origin", options->origin.hex()) << LOG_KV("key",
                //                     key);
                return storage::CODE_NO_AUTHORIZED;
            }
            // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("insert") << LOG_KV("key",
            // key);

            typename Entries::Ptr entries = std::make_shared<Entries>();
            Condition::Ptr condition = std::make_shared<Condition>();

            CacheItr it;
            {
                // ReadGuard l(x_cache);
                it = m_cache.find(key);
            }
            if (it == m_cache.end())
            {
                if (m_remoteDB)
                {
                    if (needSelect)
                        entries =
                            m_remoteDB->select(m_blockHash, m_blockNum, m_tableInfo->name, key);
                    else
                        entries = std::make_shared<Entries>();

                    m_cache.insert(std::make_pair(key, entries));
                    // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB
                    // selects")
                    //                    << LOG_KV("key", key) << LOG_KV("records",
                    //                    entries->size());
                }
            }
            else
            {
                entries = it->second;
            }
            checkField(entry);
            Change::Record record(entries->size() + 1u);
            std::vector<Change::Record> value{record};
            this->m_recorder(this->shared_from_this(), Change::Insert, key, value);
            if (entries->size() == 0)
            {
                entries->addEntry(entry);
                {
                    // WriteGuard l(x_cache);
                    m_cache.insert(std::make_pair(key, entries));
                }
                return 1;
            }
            else
            {
                entries->addEntry(entry);
                return 1;
            }
        }
        catch (std::exception& e)
        {
            STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable")
                               << LOG_DESC("Access MemoryTable failed for")
                               << LOG_KV("msg", boost::diagnostic_information(e));
        }

        return 1;
    }

    virtual int remove(const std::string& key, Condition::Ptr condition,
        AccessOptions::Ptr options = std::make_shared<AccessOptions>()) override
    {
        if (options->check && !checkAuthority(options->origin))
        {
            // STORAGE_LOG(WARNING) << LOG_BADGE("MemoryTable") << LOG_DESC("remove non-authorized")
            //                     << LOG_KV("origin", options->origin.hex()) << LOG_KV("key", key);
            return storage::CODE_NO_AUTHORIZED;
        }
        // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remove") << LOG_KV("key",
        // key);

        typename Entries::Ptr entries = std::make_shared<Entries>();

        CacheItr it;
        {
            // ReadGuard l(x_cache);
            it = m_cache.find(key);
        }
        if (it == m_cache.end())
        {
            if (m_remoteDB)
            {
                entries = m_remoteDB->select(m_blockHash, m_blockNum, m_tableInfo->name, key);
                m_cache.insert(std::make_pair(key, entries));
                // STORAGE_LOG(TRACE) << LOG_BADGE("MemoryTable") << LOG_DESC("remoteDB selects")
                //                    << LOG_KV("key", key) << LOG_KV("records", entries->size());
            }
        }
        else
        {
            entries = it->second;
        }

        auto indexes = processEntries(entries, condition);

        std::vector<Change::Record> records;
        for (auto& i : indexes)
        {
            Entry::Ptr removeEntry = entries->get(i);

            removeEntry->setStatus(1);
            records.emplace_back(i);
        }
        this->m_recorder(this->shared_from_this(), Change::Remove, key, records);

        entries->setDirty(true);

        return indexes.size();
    }

    virtual h256 hash() override
    {
        std::map<std::string, Entries::Ptr> tmpMap(m_cache.begin(), m_cache.end());
        bytes data;
        for (auto& it : tmpMap)
        {
            if (it.second->dirty())
            {
                // Entries = vector<Entry>
                // LOG(DEBUG) << LOG_BADGE("Report") << LOG_DESC("Entries") << LOG_KV(it.first,
                // "--->");
                data.insert(data.end(), it.first.begin(), it.first.end());
                for (size_t i = 0; i < it.second->size(); ++i)
                {
                    if (it.second->get(i)->dirty())
                    {
                        for (auto& fieldIt : *(it.second->get(i)->fields()))
                        {
                            // Field
                            // LOG(DEBUG) << LOG_BADGE("Report") << LOG_DESC("Field")
                            //          << LOG_KV(fieldIt.first, toHex(fieldIt.second));
                            if (isHashField(fieldIt.first))
                            {
                                data.insert(data.end(), fieldIt.first.begin(), fieldIt.first.end());
                                data.insert(
                                    data.end(), fieldIt.second.begin(), fieldIt.second.end());
                            }
                        }
                    }
                }
            }
        }

        if (data.empty())
        {
            return h256();
        }
        bytesConstRef bR(data.data(), data.size());
        h256 hash = dev::sha256(bR);

        return hash;
    }
    virtual void clear() override { m_cache.clear(); }
    virtual bool empty() override { return m_cache.empty(); }

    void setStateStorage(Storage::Ptr amopDB) override { m_remoteDB = amopDB; }
    void setBlockHash(h256 blockHash) override { m_blockHash = blockHash; }
    void setBlockNum(int blockNum) override { m_blockNum = blockNum; }
    void setTableInfo(TableInfo::Ptr tableInfo) override { m_tableInfo = tableInfo; }

    bool checkAuthority(Address const& _origin) const override
    {
        if (m_tableInfo->authorizedAddress.empty())
            return true;
        auto it = find(m_tableInfo->authorizedAddress.cbegin(),
            m_tableInfo->authorizedAddress.cend(), _origin);
        return it != m_tableInfo->authorizedAddress.cend();
    }

    virtual void setRecorder(std::function<void(
            Table::Ptr, Change::Kind, std::string const&, std::vector<Change::Record>&)>
            _recorder) override;

    virtual bool dump(TableData::Ptr _data) override
    {
        bool dirtyTable = false;
        for (auto it : m_cache)
        {
            _data->data.insert(make_pair(it.first, it.second));

            if (it.second->dirty())
            {
                dirtyTable = true;
            }
        }
        return dirtyTable;
    }
    virtual void rollback(const Change& _change) override;

    size_t cacheSize() override { return m_cache.size(); }

private:
    std::vector<size_t> processEntries(Entries::Ptr entries, Condition::Ptr condition)
    {
        std::vector<size_t> indexes;
        indexes.reserve(entries->size());
        if (condition->getConditions()->empty())
        {
            for (size_t i = 0; i < entries->size(); ++i)
                indexes.emplace_back(i);
            return indexes;
        }

        for (size_t i = 0; i < entries->size(); ++i)
        {
            Entry::Ptr entry = entries->get(i);
            if (processCondition(entry, condition))
            {
                indexes.push_back(i);
            }
        }

        return indexes;
    }

    bool processCondition(Entry::Ptr entry, Condition::Ptr condition)
    {
        try
        {
            for (auto& it : *condition->getConditions())
            {
                if (entry->getStatus() == Entry::Status::DELETED)
                {
                    return false;
                }

                std::string lhs = entry->getField(it.first);
                std::string rhs = it.second.second;

                if (it.second.first == Condition::Op::eq)
                {
                    if (lhs != rhs)
                    {
                        return false;
                    }
                }
                else if (it.second.first == Condition::Op::ne)
                {
                    if (lhs == rhs)
                    {
                        return false;
                    }
                }
                else
                {
                    if (lhs.empty())
                    {
                        lhs = "0";
                    }
                    if (rhs.empty())
                    {
                        rhs = "0";
                    }

                    int lhsNum = boost::lexical_cast<int>(lhs);
                    int rhsNum = boost::lexical_cast<int>(rhs);

                    switch (it.second.first)
                    {
                    case Condition::Op::eq:
                    case Condition::Op::ne:
                    {
                        break;
                    }
                    case Condition::Op::gt:
                    {
                        if (lhsNum <= rhsNum)
                        {
                            return false;
                        }
                        break;
                    }
                    case Condition::Op::ge:
                    {
                        if (lhsNum < rhsNum)
                        {
                            return false;
                        }
                        break;
                    }
                    case Condition::Op::lt:
                    {
                        if (lhsNum >= rhsNum)
                        {
                            return false;
                        }
                        break;
                    }
                    case Condition::Op::le:
                    {
                        if (lhsNum > rhsNum)
                        {
                            return false;
                        }
                        break;
                    }
                    }
                }
            }
        }
        catch (std::exception& e)
        {
            STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("Compare error")
                               << LOG_KV("msg", boost::diagnostic_information(e));
            return false;
        }

        return true;
    }

    bool isHashField(const std::string& _key)
    {
        if (!_key.empty())
        {
            return ((_key.substr(0, 1) != "_" && _key.substr(_key.size() - 1, 1) != "_") ||
                    (_key == STATUS));
        }
        // STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("Empty key error");
        return false;
    }

    void checkField(Entry::Ptr entry)
    {
        for (auto& it : *(entry->fields()))
        {
            if (m_tableInfo->fields.end() ==
                find(m_tableInfo->fields.begin(), m_tableInfo->fields.end(), it.first))
            {
                // STORAGE_LOG(ERROR) << LOG_BADGE("MemoryTable") << LOG_DESC("field doen not
                // exist")
                //                   << LOG_KV("table name", m_tableInfo->name)
                //                   << LOG_KV("field", it.first);
                throw std::invalid_argument("Invalid key.");
            }
        }
    }

    Storage::Ptr m_remoteDB;
    TableInfo::Ptr m_tableInfo;
    CacheType m_cache;
    h256 m_blockHash;
    int m_blockNum = 0;
    std::function<void(Table::Ptr, Change::Kind, std::string const&, std::vector<Change::Record>&)>
        m_recorder;
};  // namespace storage

template <>
inline void MemoryTable<Serial>::setRecorder(
    std::function<void(Table::Ptr, Change::Kind, std::string const&, std::vector<Change::Record>&)>
        _recorder)
{
    m_recorder = _recorder;
}

template <>
inline void MemoryTable<Parallel>::setRecorder(
    std::function<void(Table::Ptr, Change::Kind, std::string const&, std::vector<Change::Record>&)>)
{
    m_recorder = [](Table::Ptr, Change::Kind, std::string const&, std::vector<Change::Record>&) {};
}

template <>
inline void MemoryTable<Serial>::rollback(const Change& _change)
{
    // Public MemoryTable API cannot be used here because it will add another
    // change log entry.
    switch (_change.kind)
    {
    case Change::Insert:
    {
        auto entries = m_cache[_change.key];
        entries->removeEntry(_change.value[0].index);
        if (entries->size() == 0u)
        {
            m_cache.erase(_change.key);
        }
        break;
    }
    case Change::Update:
    {
        auto entries = m_cache[_change.key];
        for (auto& record : _change.value)
        {
            auto entry = entries->get(record.index);
            entry->setField(record.key, record.oldValue);
        }
        break;
    }
    case Change::Remove:
    {
        auto entries = m_cache[_change.key];
        for (auto& record : _change.value)
        {
            auto entry = entries->get(record.index);
            entry->setStatus(0);
        }
        break;
    }
    case Change::Select:

    default:
        break;
    }
}
template <>
inline void MemoryTable<Parallel>::rollback(const Change&)
{}
}  // namespace storage
}  // namespace dev
