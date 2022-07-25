#pragma once
#include "../Common.h"
#include "bcos-tars-protocol/tars/TransactionReceipt.h"
#include <bcos-concepts/Basic.h>
#include <bcos-crypto/hasher/Hasher.h>
#include <bcos-tars-protocol/tars/Block.h>
#include <bcos-tars-protocol/tars/Transaction.h>
#include <string>
#include <vector>

namespace bcos::concepts::hash
{

template <bcos::crypto::hasher::Hasher Hasher>
void impl_calculate(bcostars::Transaction const& transaction, bcos::concepts::ByteBuffer auto& out)
{
    if (!transaction.dataHash.empty())
        out = transaction.dataHash;

    Hasher hasher;
    auto const& hashFields = transaction.data;

    long version =
        boost::asio::detail::socket_ops::host_to_network_long((int32_t)hashFields.version);
    hasher.update(version);
    hasher.update(hashFields.chainID);
    hasher.update(hashFields.groupID);
    long blockLimit = boost::asio::detail::socket_ops::host_to_network_long(hashFields.blockLimit);
    hasher.update(blockLimit);
    hasher.update(hashFields.nonce);
    hasher.update(hashFields.to);
    hasher.update(hashFields.input);
    hasher.update(hashFields.abi);

    decltype(transaction.dataHash) hash(Hasher::HASH_SIZE);
    hasher.final(out);
}

template <bcos::crypto::hasher::Hasher Hasher>
void impl_calculate(
    bcostars::TransactionReceipt const& receipt, bcos::concepts::ByteBuffer auto& out)
{
    // if (!transaction.dataHash.empty())
    //     out = transaction.dataHash;

    // Hasher hasher;
    // auto const& hashFields = transaction.data;

    // long version =
    //     boost::asio::detail::socket_ops::host_to_network_long((int32_t)hashFields.version);
    // hasher.update(version);
    // hasher.update(hashFields.chainID);
    // hasher.update(hashFields.groupID);
    // long blockLimit =
    // boost::asio::detail::socket_ops::host_to_network_long(hashFields.blockLimit);
    // hasher.update(blockLimit);
    // hasher.update(hashFields.nonce);
    // hasher.update(hashFields.to);
    // hasher.update(hashFields.input);
    // hasher.update(hashFields.abi);

    // decltype(transaction.dataHash) hash(Hasher::HASH_SIZE);
    // hasher.final(out);
}

template <bcos::crypto::hasher::Hasher Hasher>
auto impl_calculate(bcostars::BlockHeader const& blockHeader, bcos::concepts::ByteBuffer auto& out)
{
    if (!blockHeader.dataHash.empty())
        out = blockHeader.dataHash;

    Hasher hasher;
    auto const& hashFields = blockHeader.data;

    long version = boost::asio::detail::socket_ops::host_to_network_long(hashFields.version);
    hasher.update(version);
    for (auto const& parent : hashFields.parentInfo)
    {
        long blockNumber =
            boost::asio::detail::socket_ops::host_to_network_long(parent.blockNumber);
        hasher.update(blockNumber);
        hasher.update(parent.blockHash);
    }
    hasher.update(hashFields.txsRoot);
    hasher.update(hashFields.receiptRoot);
    hasher.update(hashFields.stateRoot);

    long number = boost::asio::detail::socket_ops::host_to_network_long(hashFields.blockNumber);
    hasher.update(number);
    hasher.update(hashFields.gasUsed);

    long timestamp = boost::asio::detail::socket_ops::host_to_network_long(hashFields.timestamp);
    hasher.update(timestamp);

    long sealer = boost::asio::detail::socket_ops::host_to_network_long(hashFields.sealer);
    hasher.update(sealer);

    for (auto const& nodeID : hashFields.sealerList)
    {
        hasher.update(nodeID);
    }
    // update extraData to hashBuffer: 12
    hasher.update(hashFields.extraData);
    // update consensusWeights to hashBuffer: 13
    for (auto weight : hashFields.consensusWeights)
    {
        long networkWeight = boost::asio::detail::socket_ops::host_to_network_long(weight);
        hasher.update(networkWeight);
    }

    hasher.final(out);
}

}  // namespace bcos::concepts::hash
