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
 * @file PeersRouterTable.h
 * @author: octopus
 * @date 2021-12-29
 */
#pragma once
#include "FrontServiceInfo.h"
#include <bcos-framework/interfaces/crypto/KeyFactory.h>
#include <bcos-framework/interfaces/crypto/KeyInterface.h>
#include <bcos-gateway/Common.h>
#include <memory>

namespace bcos
{
namespace gateway
{
class PeersRouterTable
{
public:
    using Ptr = std::shared_ptr<PeersRouterTable>;
    PeersRouterTable(bcos::crypto::KeyFactory::Ptr _keyFactory) : m_keyFactory(_keyFactory) {}
    virtual ~PeersRouterTable() {}

    bcos::crypto::NodeIDs getGroupNodeIDList(const std::string& _groupID) const;
    std::set<P2pID> queryP2pIDs(const std::string& _groupID, const std::string& _nodeID) const;
    std::set<P2pID> queryP2pIDsByGroupID(const std::string& _groupID) const;
    void removeP2PID(const std::string& _p2pID);

    void batchInsertNodeList(std::string const& _p2pNodeID,
        std::map<std::string, std::set<std::string>> const& _nodeList);

    void removePeer(std::string const& _p2pNodeID);
    void updatePeerNodeList(std::string const& _p2pNodeID,
        std::map<std::string, std::set<std::string>> const& _nodeIDList);
    std::map<std::string, std::set<std::string>> peersNodeInfo(std::string const& _p2pNodeID) const;

private:
    bcos::crypto::KeyFactory::Ptr m_keyFactory;

    // used for peer-to-peer router
    // groupID => NodeID => set<P2pID>
    std::map<std::string, std::map<std::string, std::set<P2pID>>> m_groupNodeList;
    mutable SharedMutex x_groupNodeList;

    // the nodeIDList infos of the peers
    // p2pNodeID => groupID => nodeIDList
    std::map<P2pID, std::map<std::string, std::set<std::string>>> m_peersNodeList;
    mutable SharedMutex x_peersNodeList;
};
}  // namespace gateway
}  // namespace bcos