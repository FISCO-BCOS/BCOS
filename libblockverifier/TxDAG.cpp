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
/**
 * @brief : Generate transaction DAG for parallel execution
 * @author: jimmyshi
 * @date: 2019-1-8
 */

#include "TxDAG.h"
#include "Common.h"
#include <map>

using namespace std;
using namespace dev;
using namespace dev::eth;
using namespace dev::blockverifier;

// Generate DAG according with given transactions
void TxDAG::init(Transactions const& _txs)
{
    m_txs = make_shared<Transactions const>(_txs);
    m_dag.init(_txs.size());

    CriticalField<string> latestCriticals;

    for (ID id = 0; id < _txs.size(); id++)
    {
        auto& tx = _txs[id];

        // Is para transaction?
        if (!dev::precompile::DagTransferPrecompiled::isDagTransfer(tx.receiveAddress()))
        {
            serialTxs.emplace_back(id);
            continue;
        }

        // Get critical field
        vector<string> criticals = dev::precompile::DagTransferPrecompiled::getTransferDagTag(tx);

        // Add edge between critical transaction
        for (string const& c : criticals)
        {
            shared_ptr<IDs> pIds = latestCriticals.get(c);
            if (pIds != nullptr)
            {
                for (ID prev : *pIds)
                    m_dag.addEdge(prev, id);  // add DAG edge
            }
        }

        for (string const& c : criticals)
        {
            latestCriticals.update(c, id);
        }
    }

    // Generate DAG
    m_dag.generate();

    m_totalParaTxs = _txs.size() - serialTxs.size();
}

// Set transaction execution function
void TxDAG::setTxExecuteFunc(ExecuteTxFunc const& _f)
{
    f_executeTx = _f;
}

void TxDAG::executeUnit()
{
    // PARA_LOG(TRACE) << LOG_DESC("executeUnit") << LOG_KV("exeCnt", m_exeCnt)
    //              << LOG_KV("total", m_txs->size());
    ID id;
    {
        WriteGuard l(x_dag);
        if (m_dag.isQueueEmpty())
            return;
        id = m_dag.pop();
    }

    // PARA_LOG(TRACE) << LOG_DESC("executeUnit transaction") << LOG_KV("txid", id);
    // TODO catch execute exception
    f_executeTx((*m_txs)[id]);

    {
        WriteGuard l(x_dag);
        m_dag.consume(id);
        m_exeCnt += 1;
        // PARA_LOG(TRACE) << LOG_DESC("executeUnit finish") << LOG_KV("exeCnt", m_exeCnt)
        //                << LOG_KV("total", m_txs->size());
    }
}

void TxDAG::executeSerialTxs()
{
    for (ID id : serialTxs)
    {
        // TODO catch execute exception
        f_executeTx((*m_txs)[id]);
    }
}
