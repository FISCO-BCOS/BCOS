/**
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
 *
 * @file storage_main.cpp
 * @author: xingqiangbai
 * @date 2018-11-14
 */
#include "libinitializer/Initializer.h"
#include "libstorage/MemoryTableFactory.h"
#include <leveldb/db.h>
#include <libdevcore/BasicLevelDB.h>
#include <libdevcore/Common.h>
#include <libdevcore/easylog.h>
#include <libstorage/CachedStorage.h>
#include <libstorage/MemoryTable2.h>
#include <libstorage/MemoryTableFactoryFactory2.h>
#include <tbb/parallel_for.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
INITIALIZE_EASYLOGGINGPP

using namespace std;
using namespace dev;
using namespace boost;
using namespace dev::storage;
using namespace dev::initializer;

void testMemoryTable2(size_t round, size_t count)
{
    CachedStorage::Ptr cachedStorage = std::make_shared<CachedStorage>();

    auto factoryFactory = std::make_shared<MemoryTableFactoryFactory2>();
    factoryFactory->setStorage(cachedStorage);

    auto createFactory = factoryFactory->newTableFactory(dev::h256(0), 0);
    createFactory->createTable("test_data", "key", "value", 1, Address(0x0));
    createFactory->createTable("tx_hash_2_block", "tx_hash", "block_hash", 1, Address(0x0));

    createFactory->commitDB(dev::h256(0), 1);

    std::set<std::string> accounts;
    for (size_t i = 0; i < count; ++i)
    {
        auto dataTable = createFactory->openTable("test_data");
        auto entry = dataTable->newEntry();
        auto key = (boost::format("[%08d]") % i).str();
        entry->setField("key", key);
        entry->setField("value", "0");

        dataTable->insert(key, entry);
    }

    auto start = std::chrono::system_clock::time_point();

    for (size_t i = 0; i < round; ++i)
    {
        auto factory = factoryFactory->newTableFactory(dev::h256(0), i + 2);

        tbb::parallel_for(
            tbb::blocked_range<size_t>(0, count), [&](const tbb::blocked_range<size_t>& range) {
                for (size_t j = range.begin(); j < range.end(); ++j)
                {
                    for (int k = 0; k < 1000; ++k)
                    {
                        auto dataTable = factory->openTable("test_data");
                        auto txTable = factory->openTable("tx_hash_2_block");

                        auto key = (boost::format("[%08d]") % j).str();
                        auto condition = dataTable->newCondition();
                        auto dataEntries = dataTable->select(key, condition);

                        auto dataEntry = dataEntries->get(0);

                        auto newDataEntry = dataTable->newEntry();
                        newDataEntry->setField("value",
                            boost::lexical_cast<std::string>(
                                boost::lexical_cast<size_t>(dataEntry->getField("value")) + 1));

                        dataTable->update(key, newDataEntry, condition);

                        std::string txKey = (boost::format("[%32d]") % j).str();
                        auto txEntry = txTable->newEntry();
                        txEntry->setField("tx_hash", txKey);
                        txEntry->setField("block_hash", txKey);

                        txTable->insert(txKey, txEntry);
                    }
                }
            });

        factory->commitDB(dev::h256(0), i + 2);
    }

    auto end = std::chrono::system_clock::time_point();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Time elapsed " << std::setiosflags(std::ios::fixed) << std::setprecision(4)
              << elapsed.count();
}

int main(int argc, char* argv[])
{
    auto logInitializer = std::make_shared<LogInitializer>();
    logInitializer->stopLogging();

    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " [round] [count]";
        return 1;
    }

    size_t round = boost::lexical_cast<size_t>(argv[1]);
    size_t count = boost::lexical_cast<size_t>(argv[2]);

    TIME_RECORD("testMemoryTable2");
    testMemoryTable2(round, count);

    return 0;
}
