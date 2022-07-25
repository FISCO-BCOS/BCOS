#pragma once

#include "Converter.h"
#include <bcos-concepts/ledger/Ledger.h>
#include <bcos-crypto/hasher/Hasher.h>
#include <bcos-rpc/jsonrpc/JsonRpcInterface.h>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <type_traits>

namespace bcos::rpc
{

template <bcos::concepts::ledger::Ledger Ledger, bcos::crypto::hasher::Hasher Hasher>
class LightNodeRPC : public bcos::rpc::JsonRpcInterface
{
public:
    LightNodeRPC(Ledger ledger) {}

    void call([[maybe_unused]] std::string_view _groupID,
        [[maybe_unused]] std::string_view _nodeName, [[maybe_unused]] std::string_view _to,
        [[maybe_unused]] std::string_view _data, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void sendTransaction([[maybe_unused]] std::string_view _groupID,
        [[maybe_unused]] std::string_view _nodeName, [[maybe_unused]] std::string_view _data,
        [[maybe_unused]] bool _requireProof, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getTransaction([[maybe_unused]] std::string_view _groupID,
        [[maybe_unused]] std::string_view _nodeName, [[maybe_unused]] std::string_view _txHash,
        [[maybe_unused]] bool _requireProof, RespFunc _respFunc) override
    {
        std::array<bcos::h256, 1> hashes{bcos::h256{_txHash, bcos::h256::FromHex}};
        auto transaction =
            ledger().template getTransactionsOrReceipts<bcos::concepts::ledger::TRANSACTIONS>(
                hashes);

        Json::Value resp;
        toJsonResp<Hasher>(transaction, resp);

        _respFunc(nullptr, resp);
    }

    void getTransactionReceipt([[maybe_unused]] std::string_view _groupID,
        [[maybe_unused]] std::string_view _nodeName, [[maybe_unused]] std::string_view _txHash,
        [[maybe_unused]] bool _requireProof, RespFunc _respFunc) override
    {
        std::array<bcos::h256, 1> hashes{bcos::h256{_txHash, bcos::h256::FromHex}};
        auto receipt =
            ledger().template getTransactionsOrReceipts<bcos::concepts::ledger::RECEIPTS>(hashes);

        Json::Value resp;
        toJsonResp<Hasher>(receipt, _txHash, resp);

        _respFunc(nullptr, resp);
    }

    void getBlockByHash([[maybe_unused]] std::string_view _groupID,
        [[maybe_unused]] std::string_view _nodeName, [[maybe_unused]] std::string_view _blockHash,
        [[maybe_unused]] bool _onlyHeader, [[maybe_unused]] bool _onlyTxHash,
        RespFunc _respFunc) override
    {}

    void getBlockByNumber([[maybe_unused]] std::string_view _groupID,
        [[maybe_unused]] std::string_view _nodeName, int64_t _blockNumber, bool _onlyHeader,
        bool _onlyTxHash, RespFunc _respFunc) override
    {
        auto block = ledger().template getBlock<bcos::concepts::ledger::HEADER>(_blockNumber);

        Json::Value resp;
        toJsonResp<Hasher>(block, resp);

        _respFunc(nullptr, resp);
    }

    void getBlockHashByNumber(std::string_view _groupID, std::string_view _nodeName,
        int64_t _blockNumber, RespFunc _respFunc) override;

    void getBlockNumber(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        auto count = ledger().template getTotalTransactionCount();

        Json::Value resp = count.blockNumber;
        _respFunc(nullptr, resp);
    }

    void getCode(std::string_view _groupID, std::string_view _nodeName,
        std::string_view _contractAddress, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getABI(std::string_view _groupID, std::string_view _nodeName,
        std::string_view _contractAddress, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getSealerList(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getObserverList(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getPbftView(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getPendingTxSize(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getSyncStatus(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }
    void getConsensusStatus(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getSystemConfigByKey(std::string_view _groupID, std::string_view _nodeName,
        std::string_view _keyValue, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getTotalTransactionCount(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getGroupPeers(std::string_view _groupID, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }
    void getPeers(RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }
    void getGroupList(RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }
    void getGroupInfo(std::string_view _groupID, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }
    void getGroupInfoList(RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }
    void getGroupNodeInfo(
        std::string_view _groupID, std::string_view _nodeName, RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

    void getGroupBlockNumber(RespFunc _respFunc) override
    {
        Json::Value value;
        _respFunc(BCOS_ERROR_PTR(-1, "Unspported method!"), value);
    }

private:
    auto& ledger() { return bcos::concepts::getRef(m_ledger); }

    Ledger m_ledger;
};
}  // namespace bcos::rpc