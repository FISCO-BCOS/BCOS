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
/** @file SystemConfigPrecompiled.cpp
 *  @author chaychen
 *  @date 20181211
 */
#include "SystemConfigPrecompiled.h"
#include "libstorage/EntriesPrecompiled.h"
#include "libstorage/TableFactoryPrecompiled.h"
#include <libdevcore/easylog.h>
#include <libethcore/ABI.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
using namespace dev;
using namespace dev::blockverifier;
using namespace dev::storage;


const char* const SYSCONFIG_METHOD_SET_STR = "setValueByKey(string,string)";

SystemConfigPrecompiled::SystemConfigPrecompiled()
{
    name2Selector[SYSCONFIG_METHOD_SET_STR] = getFuncSelector(SYSCONFIG_METHOD_SET_STR);
}

bytes SystemConfigPrecompiled::call(
    ExecutiveContext::Ptr context, bytesConstRef param, Address const& origin)
{
    STORAGE_LOG(TRACE) << LOG_BADGE("SystemConfigPrecompiled") << LOG_DESC("call")
                       << LOG_KV("param", toHex(param));

    // parse function name
    uint32_t func = getParamFunc(param);
    bytesConstRef data = getParamData(param);

    dev::eth::ContractABI abi;
    bytes out;
    int count = 0;

    if (func == name2Selector[SYSCONFIG_METHOD_SET_STR])
    {
        // setValueByKey(string,string)
        std::string configKey, configValue;
        abi.abiOut(data, configKey, configValue);
        // Uniform lowercase configKey
        boost::to_lower(configKey);
        STORAGE_LOG(DEBUG) << LOG_BADGE("SystemConfigPrecompiled") << LOG_DESC("setValueByKey func")
                           << LOG_KV("configKey", configKey) << LOG_KV("configValue", configValue);

        if (!checkValueValid(configKey, configValue))
        {
            STORAGE_LOG(DEBUG) << LOG_BADGE("SystemConfigPrecompiled")
                               << LOG_DESC("SystemConfigPrecompiled set invalid value")
                               << LOG_KV("configKey", configKey)
                               << LOG_KV("configValue", configValue);
            out = abi.abiIn("", getOutJson(-40, "invalid configuration values"));
            return out;
        }

        storage::Table::Ptr table = openTable(context, SYS_CONFIG);

        auto condition = table->newCondition();
        auto entries = table->select(configKey, condition);
        auto entry = table->newEntry();
        entry->setField(SYSTEM_CONFIG_KEY, configKey);
        entry->setField(SYSTEM_CONFIG_VALUE, configValue);
        entry->setField(SYSTEM_CONFIG_ENABLENUM,
            boost::lexical_cast<std::string>(context->blockInfo().number + 1));

        if (entries->size() == 0u)
        {
            count = table->insert(configKey, entry, getOptions(origin));
            if (count == -1)
            {
                STORAGE_LOG(DEBUG)
                    << LOG_BADGE("SystemConfigPrecompiled") << LOG_DESC("non-authorized");

                out = abi.abiIn("", getOutJson(-1, "non-authorized"));
            }
            else
            {
                STORAGE_LOG(DEBUG) << LOG_BADGE("SystemConfigPrecompiled")
                                   << LOG_DESC("setValueByKey successfully");

                out = abi.abiIn("", getOutJson(count, "success"));
            }
        }
        else
        {
            count = table->update(configKey, entry, condition, getOptions(origin));
            if (count == -1)
            {
                STORAGE_LOG(DEBUG)
                    << LOG_BADGE("SystemConfigPrecompiled") << LOG_DESC("non-authorized");

                out = abi.abiIn("", getOutJson(-1, "non-authorized"));
            }
            else
            {
                STORAGE_LOG(DEBUG) << LOG_BADGE("SystemConfigPrecompiled")
                                   << LOG_DESC("update value by key successfully");

                out = abi.abiIn("", getOutJson(count, "success"));
            }
        }
    }
    else
    {
        STORAGE_LOG(ERROR) << LOG_BADGE("SystemConfigPrecompiled") << LOG_DESC("error func")
                           << LOG_KV("func", func);
    }
    return out;
}

bool SystemConfigPrecompiled::checkValueValid(std::string const& key, std::string const& value)
{
    if (SYSTEM_KEY_TX_COUNT_LIMIT == key)
    {
        return (boost::lexical_cast<uint64_t>(value) >= TX_COUNT_LIMIT_MIN);
    }
    else if (SYSTEM_KEY_TX_GAS_LIMIT == key)
    {
        return (boost::lexical_cast<uint64_t>(value) >= TX_GAS_LIMIT_MIN);
    }
    return true;
}
