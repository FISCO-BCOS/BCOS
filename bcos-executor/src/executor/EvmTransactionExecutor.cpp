/*
 *  Copyright (C) 2021 FISCO BCOS.
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
 * @brief EvmTransactionExecutor
 * @file EvmTransactionExecutor.cpp
 * @author: jimmyshi
 * @date: 2022-01-18
 */

#include "EvmTransactionExecutor.h"
#include "../Common.h"
#include "../dag/Abi.h"
#include "../dag/ClockCache.h"
#include "../dag/CriticalFields.h"
#include "../dag/ScaleUtils.h"
#include "../dag/TxDAG.h"
#include "../dag/TxDAG2.h"
#include "../executive/BlockContext.h"
#include "../executive/TransactionExecutive.h"
#include "../precompiled/ConsensusPrecompiled.h"
#include "../precompiled/CryptoPrecompiled.h"
#include "../precompiled/FileSystemPrecompiled.h"
#include "../precompiled/KVTablePrecompiled.h"
#include "../precompiled/SystemConfigPrecompiled.h"
#include "../precompiled/TableManagerPrecompiled.h"
#include "../precompiled/TablePrecompiled.h"
#include "../precompiled/extension/AuthManagerPrecompiled.h"
#include "../precompiled/extension/ContractAuthMgrPrecompiled.h"
#include "../precompiled/extension/DagTransferPrecompiled.h"
#include "../precompiled/extension/UserPrecompiled.h"
#include "../vm/Precompiled.h"
#include "../vm/gas_meter/GasInjector.h"
#include "bcos-codec/abi/ContractABIType.h"
#include "bcos-executor/src/precompiled/common/Common.h"
#include "bcos-executor/src/precompiled/common/PrecompiledResult.h"
#include "bcos-executor/src/precompiled/common/Utilities.h"
#include "bcos-framework/interfaces/dispatcher/SchedulerInterface.h"
#include "bcos-framework/interfaces/executor/ExecutionMessage.h"
#include "bcos-framework/interfaces/executor/PrecompiledTypeDef.h"
#include "bcos-framework/interfaces/ledger/LedgerTypeDef.h"
#include "bcos-framework/interfaces/protocol/ProtocolTypeDef.h"
#include "bcos-framework/interfaces/protocol/TransactionReceipt.h"
#include "bcos-framework/interfaces/storage/StorageInterface.h"
#include "bcos-framework/interfaces/storage/Table.h"
#include "bcos-table/src/StateStorage.h"
#include "bcos-utilities/Error.h"
#include "bcos-utilities/ThreadPool.h"
#include "tbb/flow_graph.h"
#include <bcos-framework/interfaces/protocol/LogEntry.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/spin_mutex.h>
#include <boost/algorithm/hex.hpp>
#include <boost/exception/detail/exception_ptr.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/format.hpp>
#include <boost/format/format_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/latch.hpp>
#include <boost/throw_exception.hpp>
#include <cassert>
#include <exception>
#include <functional>
#include <gsl/gsl_util>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

using namespace bcos;
using namespace std;
using namespace bcos::executor;
using namespace bcos::executor::critical;
using namespace bcos::wasm;
using namespace bcos::protocol;
using namespace bcos::storage;
using namespace bcos::precompiled;
void EvmTransactionExecutor::initPrecompiled()
{
    auto fillZero = [](int _num) -> std::string {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(40) << std::hex << _num;
        return stream.str();
    };
    m_precompiledContract =
        std::make_shared<std::map<std::string, std::shared_ptr<PrecompiledContract>>>();
    m_builtInPrecompiled = std::make_shared<std::set<std::string>>();
    m_constantPrecompiled =
        std::make_shared<std::map<std::string, std::shared_ptr<precompiled::Precompiled>>>();

    m_precompiledContract->insert(std::make_pair(fillZero(1),
        make_shared<PrecompiledContract>(3000, 0, PrecompiledRegistrar::executor("ecrecover"))));
    m_precompiledContract->insert(std::make_pair(fillZero(2),
        make_shared<PrecompiledContract>(60, 12, PrecompiledRegistrar::executor("sha256"))));
    m_precompiledContract->insert(std::make_pair(fillZero(3),
        make_shared<PrecompiledContract>(600, 120, PrecompiledRegistrar::executor("ripemd160"))));
    m_precompiledContract->insert(std::make_pair(fillZero(4),
        make_shared<PrecompiledContract>(15, 3, PrecompiledRegistrar::executor("identity"))));
    m_precompiledContract->insert(
        {fillZero(5), make_shared<PrecompiledContract>(PrecompiledRegistrar::pricer("modexp"),
                          PrecompiledRegistrar::executor("modexp"))});
    m_precompiledContract->insert(
        {fillZero(6), make_shared<PrecompiledContract>(
                          150, 0, PrecompiledRegistrar::executor("alt_bn128_G1_add"))});
    m_precompiledContract->insert(
        {fillZero(7), make_shared<PrecompiledContract>(
                          6000, 0, PrecompiledRegistrar::executor("alt_bn128_G1_mul"))});
    m_precompiledContract->insert({fillZero(8),
        make_shared<PrecompiledContract>(PrecompiledRegistrar::pricer("alt_bn128_pairing_product"),
            PrecompiledRegistrar::executor("alt_bn128_pairing_product"))});
    m_precompiledContract->insert({fillZero(9),
        make_shared<PrecompiledContract>(PrecompiledRegistrar::pricer("blake2_compression"),
            PrecompiledRegistrar::executor("blake2_compression"))});

    auto sysConfig = std::make_shared<precompiled::SystemConfigPrecompiled>(m_hashImpl);
    auto consensusPrecompiled = std::make_shared<precompiled::ConsensusPrecompiled>(m_hashImpl);
    auto tableManagerPrecompiled =
        std::make_shared<precompiled::TableManagerPrecompiled>(m_hashImpl);
    auto kvTablePrecompiled = std::make_shared<precompiled::KVTablePrecompiled>(m_hashImpl);
    auto tablePrecompiled = std::make_shared<precompiled::TablePrecompiled>(m_hashImpl);

    // in EVM
    m_constantPrecompiled->insert({SYS_CONFIG_ADDRESS, sysConfig});
    m_constantPrecompiled->insert({CONSENSUS_ADDRESS, consensusPrecompiled});
    m_constantPrecompiled->insert({TABLE_MANAGER_ADDRESS, tableManagerPrecompiled});
    m_constantPrecompiled->insert({KV_TABLE_ADDRESS, kvTablePrecompiled});
    m_constantPrecompiled->insert({TABLE_ADDRESS, tablePrecompiled});
    m_constantPrecompiled->insert(
        {DAG_TRANSFER_ADDRESS, std::make_shared<precompiled::DagTransferPrecompiled>(m_hashImpl)});
    m_constantPrecompiled->insert(
        {CRYPTO_ADDRESS, std::make_shared<CryptoPrecompiled>(m_hashImpl)});
    m_constantPrecompiled->insert(
        {BFS_ADDRESS, std::make_shared<precompiled::FileSystemPrecompiled>(m_hashImpl)});
    /// auth precompiled
    if (m_isAuthCheck)
    {
        m_constantPrecompiled->insert({AUTH_MANAGER_ADDRESS,
            std::make_shared<precompiled::AuthManagerPrecompiled>(m_hashImpl)});
        m_constantPrecompiled->insert({AUTH_CONTRACT_MGR_ADDRESS,
            std::make_shared<precompiled::ContractAuthMgrPrecompiled>(m_hashImpl)});
    }

    CpuHeavyPrecompiled::registerPrecompiled(m_constantPrecompiled, m_hashImpl);
    SmallBankPrecompiled::registerPrecompiled(m_constantPrecompiled, m_hashImpl);

    set<string> builtIn = {CRYPTO_ADDRESS};
    m_builtInPrecompiled = make_shared<set<string>>(builtIn);
}