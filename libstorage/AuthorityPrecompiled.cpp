/*
    This file is part of FISCO-BCOS.

    FISCO-BCOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FISCO-BCOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file AuthorityPrecompiled.h
 *  @author caryliao
 *  @date 20181205
 */
#include "AuthorityPrecompiled.h"
#include "Common.h"
#include "libstorage/TableFactoryPrecompiled.h"
#include <json_spirit/JsonSpiritHeaders.h>
#include <libdevcore/easylog.h>
#include <libethcore/ABI.h>
#include <boost/lexical_cast.hpp>

using namespace dev;
using namespace dev::blockverifier;
using namespace dev::storage;


const char* const AUP_METHOD_INS = "insert(string,string)";
const char* const AUP_METHOD_REM = "remove(string,string)";
const char* const AUP_METHOD_QUE = "queryByName(string)";


AuthorityPrecompiled::AuthorityPrecompiled()
{
    name2Selector[AUP_METHOD_INS] = getFuncSelector(AUP_METHOD_INS);
    name2Selector[AUP_METHOD_REM] = getFuncSelector(AUP_METHOD_REM);
    name2Selector[AUP_METHOD_QUE] = getFuncSelector(AUP_METHOD_QUE);
}


std::string AuthorityPrecompiled::toString(ExecutiveContext::Ptr)
{
    return "Authority";
}

storage::Table::Ptr AuthorityPrecompiled::openTable(
    ExecutiveContext::Ptr context, const std::string& tableName)
{
    STORAGE_LOG(DEBUG) << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("open table")
                       << LOG_KV("tableName", tableName);

    TableFactoryPrecompiled::Ptr tableFactoryPrecompiled =
        std::dynamic_pointer_cast<TableFactoryPrecompiled>(
            context->getPrecompiled(Address(0x1001)));
    return tableFactoryPrecompiled->getmemoryTableFactory()->openTable(tableName);
}

bytes AuthorityPrecompiled::call(
    ExecutiveContext::Ptr context, bytesConstRef param, Address const& origin)
{
    STORAGE_LOG(TRACE) << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("call")
                       << LOG_KV("param", toHex(param));

    // parse function name
    uint32_t func = getParamFunc(param);
    bytesConstRef data = getParamData(param);

    dev::eth::ContractABI abi;
    bytes out;

    if (func == name2Selector[AUP_METHOD_INS])
    {
        // insert(string tableName,string addr)
        std::string tableName, addr;
        abi.abiOut(data, tableName, addr);
        addPrefixToUserTable(tableName);
        STORAGE_LOG(DEBUG) << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("insert func")
                           << LOG_KV("tableName", tableName) << LOG_KV("address", addr);
        Table::Ptr table = openTable(context, SYS_ACCESS_TABLE);

        auto condition = table->newCondition();
        condition->EQ(SYS_AC_ADDRESS, addr);
        auto entries = table->select(tableName, condition);
        if (entries->size() != 0u)
        {
            STORAGE_LOG(WARNING) << LOG_BADGE("AuthorityPrecompiled")
                                 << LOG_DESC("tableName and address exist");

            out = abi.abiIn("", getOutJson(-33, "table name and address exist"));
        }
        else
        {
            auto entry = table->newEntry();
            entry->setField(SYS_AC_TABLE_NAME, tableName);
            entry->setField(SYS_AC_ADDRESS, addr);
            entry->setField(SYS_AC_ENABLENUM,
                boost::lexical_cast<std::string>(context->blockInfo().number + 1));
            int count = table->insert(tableName, entry, getOptions(origin));
            if (count == -1)
            {
                STORAGE_LOG(DEBUG)
                    << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("non-authorized");

                out = abi.abiIn("", getOutJson(-1, "non-authorized"));
            }
            else
            {
                STORAGE_LOG(DEBUG)
                    << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("insert successfully");

                out = abi.abiIn("", getOutJson(count, "success"));
            }
        }
    }
    else if (func == name2Selector[AUP_METHOD_REM])
    {
        // remove(string tableName,string addr)
        std::string tableName, addr;
        abi.abiOut(data, tableName, addr);
        addPrefixToUserTable(tableName);

        STORAGE_LOG(DEBUG) << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("remove func")
                           << LOG_KV("tableName", tableName) << LOG_KV("address", addr);

        Table::Ptr table = openTable(context, SYS_ACCESS_TABLE);

        auto condition = table->newCondition();
        condition->EQ(SYS_AC_ADDRESS, addr);
        auto entries = table->select(tableName, condition);
        if (entries->size() == 0u)
        {
            STORAGE_LOG(WARNING) << LOG_BADGE("AuthorityPrecompiled")
                                 << LOG_DESC("tableName and address does not exist");

            out = abi.abiIn("", getOutJson(-34, "table name and address does not exist"));
        }
        else
        {
            int count = table->remove(tableName, condition, getOptions(origin));
            if (count == -1)
            {
                STORAGE_LOG(DEBUG)
                    << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("non-authorized");

                out = abi.abiIn("", getOutJson(-1, "non-authorized"));
            }
            else
            {
                STORAGE_LOG(DEBUG)
                    << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("remove successfully");

                out = abi.abiIn("", getOutJson(count, "success"));
            }
        }
    }
    else if (func == name2Selector[AUP_METHOD_QUE])
    {
        // queryByName(string table_name)
        std::string tableName;
        abi.abiOut(data, tableName);
        addPrefixToUserTable(tableName);

        STORAGE_LOG(DEBUG) << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("queryByName func")
                           << LOG_KV("tableName", tableName);

        Table::Ptr table = openTable(context, SYS_ACCESS_TABLE);

        auto condition = table->newCondition();
        auto entries = table->select(tableName, condition);
        json_spirit::Array AuthorityInfos;
        if (entries)
        {
            for (size_t i = 0; i < entries->size(); i++)
            {
                auto entry = entries->get(i);
                json_spirit::Object AuthorityInfo;
                AuthorityInfo.push_back(json_spirit::Pair(SYS_AC_TABLE_NAME, tableName));
                AuthorityInfo.push_back(
                    json_spirit::Pair(SYS_AC_ADDRESS, entry->getField(SYS_AC_ADDRESS)));
                AuthorityInfo.push_back(
                    json_spirit::Pair(SYS_AC_ENABLENUM, entry->getField(SYS_AC_ENABLENUM)));
                AuthorityInfos.push_back(AuthorityInfo);
            }
        }
        json_spirit::Value value(AuthorityInfos);
        std::string str = json_spirit::write_string(value, true);

        out = abi.abiIn("", str);
    }
    else
    {
        STORAGE_LOG(ERROR) << LOG_BADGE("AuthorityPrecompiled") << LOG_DESC("error func")
                           << LOG_KV("func", func);
    }
    return out;
}

void AuthorityPrecompiled::addPrefixToUserTable(std::string& table_name)
{
    if (table_name == SYS_ACCESS_TABLE || table_name == SYS_MINERS || table_name == SYS_TABLES ||
        table_name == SYS_CNS || table_name == SYS_CONFIG)
    {
        return;
    }
    else
    {
        table_name = USER_TABLE_PREFIX + table_name;
    }
}
