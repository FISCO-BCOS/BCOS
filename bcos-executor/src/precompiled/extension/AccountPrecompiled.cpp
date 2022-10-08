/**
 *  Copyright (C) 2022 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * @file AccountPrecompiled.cpp
 * @author: kyonGuo
 * @date 2022/9/26
 */

#include "AccountPrecompiled.h"
#include "../../vm/HostContext.h"

using namespace bcos;
using namespace bcos::precompiled;
using namespace bcos::executor;
using namespace bcos::storage;
using namespace bcos::protocol;

const char* const AM_METHOD_SET_ACCOUNT_STATUS = "setAccountStatus(uint16)";
const char* const AM_METHOD_GET_ACCOUNT_STATUS = "getAccountStatus()";

AccountPrecompiled::AccountPrecompiled(crypto::Hash::Ptr _hashImpl) : Precompiled(_hashImpl)
{
    name2Selector[AM_METHOD_SET_ACCOUNT_STATUS] =
        getFuncSelector(AM_METHOD_SET_ACCOUNT_STATUS, _hashImpl);
    name2Selector[AM_METHOD_GET_ACCOUNT_STATUS] =
        getFuncSelector(AM_METHOD_GET_ACCOUNT_STATUS, _hashImpl);
}

std::shared_ptr<PrecompiledExecResult> AccountPrecompiled::call(
    std::shared_ptr<executor::TransactionExecutive> _executive,
    PrecompiledExecResult::Ptr _callParameters)
{
    auto blockContext = _executive->blockContext().lock();
    auto codec = CodecWrapper(blockContext->hashHandler(), blockContext->isWasm());
    // [tableName][actualParams]
    std::vector<std::string> dynamicParams;
    bytes param;
    codec.decode(_callParameters->input(), dynamicParams, param);
    auto accountTableName = dynamicParams.at(0);

    // get user call actual params
    auto originParam = ref(param);
    uint32_t func = getParamFunc(originParam);
    bytesConstRef data = getParamData(originParam);
    auto table = _executive->storage().openTable(accountTableName);
    if (!table.has_value())
    {
        BOOST_THROW_EXCEPTION(PrecompiledError(accountTableName + " does not exist"));
    }

    if (func == name2Selector[AM_METHOD_SET_ACCOUNT_STATUS])
    {
        setAccountStatus(accountTableName, _executive, data, _callParameters);
    }
    else if (func == name2Selector[AM_METHOD_GET_ACCOUNT_STATUS])
    {
        getAccountStatus(accountTableName, _executive, _callParameters);
    }
    else
    {
        PRECOMPILED_LOG(INFO) << LOG_BADGE("AccountPrecompiled")
                              << LOG_DESC("call undefined function") << LOG_KV("func", func);
        BOOST_THROW_EXCEPTION(
            bcos::protocol::PrecompiledError("AccountPrecompiled call undefined function!"));
    }
    return _callParameters;
}


void AccountPrecompiled::setAccountStatus(const std::string& accountTableName,
    const std::shared_ptr<executor::TransactionExecutive>& _executive, bytesConstRef& data,
    const PrecompiledExecResult::Ptr& _callParameters)
{
    auto blockContext = _executive->blockContext().lock();
    auto codec = CodecWrapper(blockContext->hashHandler(), blockContext->isWasm());
    auto accountMgrSender = blockContext->isWasm() ? ACCOUNT_MANAGER_NAME : ACCOUNT_MGR_ADDRESS;
    if (_callParameters->m_sender != accountMgrSender)
    {
        getErrorCodeOut(_callParameters->mutableExecResult(), CODE_NO_AUTHORIZED, codec);
        return;
    }

    uint16_t status = 0;
    codec.decode(data, status);

    PRECOMPILED_LOG(INFO) << BLOCK_NUMBER(blockContext->number()) << LOG_BADGE("AccountPrecompiled")
                          << LOG_DESC("setAccountStatus") << LOG_KV("account", accountTableName)
                          << LOG_KV("status", status);
    Entry statusEntry;
    statusEntry.importFields({std::to_string(status)});
    _executive->storage().setRow(accountTableName, ACCOUNT_STATUS, std::move(statusEntry));
    Entry updateEntry;
    updateEntry.importFields({std::to_string(blockContext->number())});
    _executive->storage().setRow(accountTableName, ACCOUNT_LAST_UPDATE, std::move(updateEntry));
    _callParameters->setExecResult(codec.encode(int32_t(CODE_SUCCESS)));
}

void AccountPrecompiled::getAccountStatus(const std::string& tableName,
    const std::shared_ptr<executor::TransactionExecutive>& _executive,
    const PrecompiledExecResult::Ptr& _callParameters)
{
    auto blockContext = _executive->blockContext().lock();
    auto codec = CodecWrapper(blockContext->hashHandler(), blockContext->isWasm());
    auto entry = _executive->storage().getRow(tableName, ACCOUNT_STATUS);
    if (!entry.has_value())
    {
        PRECOMPILED_LOG(DEBUG) << LOG_BADGE("AccountPrecompiled") << LOG_DESC("getAccountStatus")
                               << LOG_DESC("Status row not exist, return 0 by default");
        _callParameters->setExecResult(codec.encode(uint16_t(0), u256(0)));
        return;
    }
    auto statusStr = entry->get();
    auto lastUpdateEntry = _executive->storage().getRow(tableName, ACCOUNT_LAST_UPDATE);
    auto lastUpdateStr = lastUpdateEntry->get();
    PRECOMPILED_LOG(TRACE) << LOG_BADGE("AccountPrecompiled") << LOG_DESC("getAccountStatus")
                           << LOG_KV("status", statusStr)
                           << LOG_KV("lastUpdateNumber", lastUpdateStr);
    uint16_t status = boost::lexical_cast<uint16_t>(statusStr);
    u256 blockNumber = boost::lexical_cast<u256>(lastUpdateStr);
    _callParameters->setExecResult(codec.encode(status, blockNumber));
}
