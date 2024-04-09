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
 * @file Web3Transaction.cpp
 * @author: kyonGuo
 * @date 2024/4/8
 */

#include "Web3Transaction.h"

#include <bcos-crypto/hash/Keccak256.h>

namespace bcos
{
using codec::rlp::decode;
using codec::rlp::encode;
using codec::rlp::header;
using codec::rlp::length;
bcos::bytes bcos::Web3Transaction::encode() const
{
    bcos::bytes out;
    if (type == TransactionType::Legacy)
    {
        // rlp([nonce, gasPrice, gasLimit, to, value, data])
        codec::rlp::encodeHeader(out, codec::rlp::headerForSign(*this));
        codec::rlp::encode(out, nonce);
        // for legacy tx, it means gas price
        codec::rlp::encode(out, maxFeePerGas);
        codec::rlp::encode(out, gasLimit);
        codec::rlp::encode(out, to.ref());
        codec::rlp::encode(out, value);
        codec::rlp::encode(out, data);
        if (chainId)
        {
            // EIP-155
            codec::rlp::encode(out, chainId.value());
            codec::rlp::encode(out, 0u);
            codec::rlp::encode(out, 0u);
        }
    }
    else
    {
        // EIP2930: 0x01 || rlp([chainId, nonce, gasPrice, gasLimit, to, value, data, accessList])

        // EIP1559: 0x02 || rlp([chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas,
        // gas_limit, destination, amount, data, access_list])

        // EIP4844: 0x03 || rlp([chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas,
        // gas_limit, to, value, data, access_list, max_fee_per_blob_gas, blob_versioned_hashes])
        out.push_back(static_cast<byte>(type));
        codec::rlp::encodeHeader(out, codec::rlp::headerForSign(*this));
        codec::rlp::encode(out, chainId.value_or(0));
        codec::rlp::encode(out, nonce);
        if (type != TransactionType::EIP2930)
        {
            codec::rlp::encode(out, maxPriorityFeePerGas);
        }
        // for EIP2930 it means gasPrice; for EIP1559 and EIP4844, it means max priority fee per gas
        codec::rlp::encode(out, maxFeePerGas);
        codec::rlp::encode(out, gasLimit);
        codec::rlp::encode(out, to.ref());
        codec::rlp::encode(out, value);
        codec::rlp::encode(out, data);
        codec::rlp::encode(out, accessList);
        if (type == TransactionType::EIP4844)
        {
            codec::rlp::encode(out, maxFeePerBlobGas);
            codec::rlp::encode(out, blobVersionedHashes);
        }
    }
    return out;
}

bcos::crypto::HashType bcos::Web3Transaction::hash() const
{
    bcos::bytes encoded{};
    codec::rlp::encode(encoded, *this);
    return bcos::crypto::keccak256Hash(bcos::ref(encoded));
}

bcostars::Transaction Web3Transaction::toTarsTransaction() const
{
    bcostars::Transaction tarsTx{};
    tarsTx.data.nonce = std::to_string(this->nonce);
    tarsTx.data.to = this->to.hexPrefixed();
    tarsTx.data.input.insert(tarsTx.data.input.end(), this->data.begin(), this->data.end());
    tarsTx.data.value = std::to_string(this->value);
    tarsTx.data.gasLimit = this->gasLimit;
    if (static_cast<uint8_t>(this->type) >= static_cast<uint8_t>(TransactionType::EIP1559))
    {
        tarsTx.data.maxFeePerGas = std::to_string(this->maxFeePerGas);
        tarsTx.data.maxPriorityFeePerGas = std::to_string(this->maxPriorityFeePerGas);
    }
    else
    {
        tarsTx.data.gasPrice = std::to_string(this->maxPriorityFeePerGas);
    }
    auto hash = this->hash();
    auto encodedForSign = this->encode();
    tarsTx.dataHash.insert(tarsTx.dataHash.end(), hash.begin(), hash.end());
    // FISCO BCOS signature is r||s||v
    tarsTx.signature.insert(
        tarsTx.signature.end(), this->signatureR.begin(), this->signatureR.end());
    tarsTx.signature.insert(
        tarsTx.signature.end(), this->signatureS.begin(), this->signatureS.end());
    tarsTx.signature.push_back(static_cast<tars::Char>(this->signatureV));

    tarsTx.type = 1;

    tarsTx.extraTransactionBytes.insert(
        tarsTx.extraTransactionBytes.end(), encodedForSign.begin(), encodedForSign.end());

    return tarsTx;
}

namespace codec::rlp
{
Header header(const AccessListEntry& entry) noexcept
{
    auto len = length(entry.storageKeys);
    return {.isList = true, .payloadLength = Address::SIZE + 1 + len};
}

size_t length(AccessListEntry const& entry) noexcept
{
    auto head = header(entry);
    return lengthOfLength(head.payloadLength) + head.payloadLength;
}
Header headerTxBase(const Web3Transaction& tx) noexcept
{
    Header h{.isList = true};

    if (tx.type != TransactionType::Legacy)
    {
        h.payloadLength += length(tx.chainId.value_or(0));
    }

    h.payloadLength += length(tx.nonce);
    if (tx.type == TransactionType::EIP1559 || tx.type == TransactionType::EIP4844)
    {
        h.payloadLength += length(tx.maxPriorityFeePerGas);
    }
    h.payloadLength += length(tx.maxFeePerGas);
    h.payloadLength += length(tx.gasLimit);
    h.payloadLength += tx.to ? (Address::SIZE + 1) : 1;
    h.payloadLength += length(tx.value);
    h.payloadLength += length(tx.data);

    if (tx.type != TransactionType::Legacy)
    {
        h.payloadLength += codec::rlp::length(tx.accessList);
        if (tx.type == TransactionType::EIP4844)
        {
            h.payloadLength += length(tx.maxFeePerBlobGas);
            h.payloadLength += length(tx.blobVersionedHashes);
        }
    }

    return h;
}
Header header(Web3Transaction const& tx) noexcept
{
    auto header = headerTxBase(tx);
    header.payloadLength += (tx.type == TransactionType::Legacy) ? length(tx.getSignatureV()) : 1;
    header.payloadLength += length(tx.signatureR);
    header.payloadLength += length(tx.signatureS);
    return header;
}
Header headerForSign(Web3Transaction const& tx) noexcept
{
    auto header = headerTxBase(tx);
    if (tx.type == TransactionType::Legacy && tx.chainId)
    {
        header.payloadLength += length(tx.chainId.value()) + 2;
    }
    return header;
}
size_t length(Web3Transaction const& tx) noexcept
{
    auto head = header(tx);
    auto len = lengthOfLength(head.payloadLength) + head.payloadLength;
    len = (tx.type == TransactionType::Legacy) ? len : lengthOfLength(len + 1) + len + 1;
    return len;
}
void encode(bcos::bytes& out, const AccessListEntry& entry) noexcept
{
    encodeHeader(out, header(entry));
    encode(out, entry.account.ref());
    encode(out, entry.storageKeys);
}
void encode(bcos::bytes& out, const Web3Transaction& tx) noexcept
{
    if (tx.type == TransactionType::Legacy)
    {
        // rlp([nonce, gasPrice, gasLimit, to, value, data, v, r, s])
        encodeHeader(out, header(tx));
        encode(out, tx.nonce);
        // for legacy tx, it means gas price
        encode(out, tx.maxFeePerGas);
        encode(out, tx.gasLimit);
        encode(out, tx.to.ref());
        encode(out, tx.value);
        encode(out, tx.data);
        encode(out, tx.getSignatureV());
        encode(out, tx.signatureR);
        encode(out, tx.signatureS);
    }
    else
    {
        // EIP2930: 0x01 || rlp([chainId, nonce, gasPrice, gasLimit, to, value, data, accessList,
        // signatureYParity, signatureR, signatureS])

        // EIP1559: 0x02 || rlp([chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas,
        // gas_limit, destination, amount, data, access_list, signature_y_parity, signature_r,
        // signature_s])

        // EIP4844: 0x03 || rlp([chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas,
        // gas_limit, to, value, data, access_list, max_fee_per_blob_gas, blob_versioned_hashes,
        // signature_y_parity, signature_r, signature_s])
        out.push_back(static_cast<bcos::byte>(tx.type));
        encodeHeader(out, header(tx));
        encode(out, tx.chainId.value_or(0));
        encode(out, tx.nonce);
        if (tx.type != TransactionType::EIP2930)
        {
            encode(out, tx.maxPriorityFeePerGas);
        }
        // for EIP2930 it means gasPrice; for EIP1559 and EIP4844, it means max priority fee per gas
        encode(out, tx.maxFeePerGas);
        encode(out, tx.gasLimit);
        encode(out, tx.to.ref());
        encode(out, tx.value);
        encode(out, tx.data);
        encode(out, tx.accessList);
        if (tx.type == TransactionType::EIP4844)
        {
            encode(out, tx.maxFeePerBlobGas);
            encode(out, tx.blobVersionedHashes);
        }
        encode(out, tx.signatureV);
        encode(out, tx.signatureR);
        encode(out, tx.signatureS);
    }
}
bcos::Error::UniquePtr decode(bcos::bytesRef& in, AccessListEntry& out) noexcept
{
    return decode(in, out.account, out.storageKeys);
}
bcos::Error::UniquePtr decode(bcos::bytesRef& in, Web3Transaction& out) noexcept
{
    if (in.empty())
    {
        return BCOS_ERROR_UNIQUE_PTR(InputTooShort, "Input too short");
    }
    if (auto const& firstByte = in[0]; 0 < firstByte && firstByte < BYTES_HEAD_BASE)
    {
        // EIP-2718: Transaction Type
        // EIP2930: 0x01 || rlp([chainId, nonce, gasPrice, gasLimit, to, value, data, accessList,
        // signatureYParity, signatureR, signatureS])

        // EIP1559: 0x02 || rlp([chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas,
        // gas_limit, destination, amount, data, access_list, signature_y_parity, signature_r,
        // signature_s])

        // EIP4844: 0x03 || rlp([chain_id, nonce, max_priority_fee_per_gas, max_fee_per_gas,
        // gas_limit, to, value, data, access_list, max_fee_per_blob_gas, blob_versioned_hashes,
        // signature_y_parity, signature_r, signature_s])

        out.type = static_cast<TransactionType>(firstByte);
        in = in.getCroppedData(1);
        auto&& [e, header] = decodeHeader(in);
        if (e != nullptr)
        {
            return std::move(e);
        }
        if (!header.isList)
        {
            return BCOS_ERROR_UNIQUE_PTR(UnexpectedString, "Unexpected String");
        }
        uint64_t chainId = 0;
        if (auto error = decodeItems(in, chainId, out.nonce, out.maxPriorityFeePerGas);
            error != nullptr)
        {
            return error;
        }
        out.chainId.emplace(chainId);
        if (out.type == TransactionType::EIP2930)
        {
            out.maxFeePerGas = out.maxPriorityFeePerGas;
        }
        else if (auto error = decode(in, out.maxFeePerGas); error != nullptr)
        {
            return error;
        }

        if (auto error = decodeItems(in, out.gasLimit, out.to, out.value, out.data, out.accessList);
            error != nullptr)
        {
            return error;
        }

        if (out.type == TransactionType::EIP4844)
        {
            if (auto error = decodeItems(in, out.maxFeePerBlobGas, out.blobVersionedHashes);
                error != nullptr)
            {
                return error;
            }
        }

        return decodeItems(in, out.signatureV, out.signatureR, out.signatureS);
    }
    // rlp([nonce, gasPrice, gasLimit, to, value, data, v, r, s])
    auto&& [error, header] = decodeHeader(in);
    if (error != nullptr)
    {
        return std::move(error);
    }
    if (!header.isList)
    {
        return BCOS_ERROR_UNIQUE_PTR(UnexpectedList, "Unexpected list");
    }
    out.type = TransactionType::Legacy;
    auto decodeError = decodeItems(in, out.nonce, out.maxPriorityFeePerGas, out.gasLimit, out.to,
        out.value, out.data, out.signatureV, out.signatureR, out.signatureS);
    out.maxFeePerGas = out.maxPriorityFeePerGas;
    auto v = out.signatureV;
    if (v == 27 || v == 28)
    {
        // pre EIP-155
        out.chainId = std::nullopt;
        out.signatureV = v - 27;
    }
    else if (v < 35)
    {
        return BCOS_ERROR_UNIQUE_PTR(InvalidVInSignature, "Invalid V in signature");
    }
    else
    {
        // https://eips.ethereum.org/EIPS/eip-155
        // Find chain_id and y_parity ∈ {0, 1} such that
        // v = chain_id * 2 + 35 + y_parity
        out.signatureV = (v - 35) % 2;
        out.chainId = ((v - 35) >> 1);
    }
    return decodeError;
}
}  // namespace codec::rlp
}  // namespace bcos
