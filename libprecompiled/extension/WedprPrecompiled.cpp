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
 * (c) 2016-2019 fisco-dev contributors.
 */
/** @file WedprPrecompiled.cpp
 *  @author caryliao
 *  @date 20190917
 */

#include "WedprPrecompiled.h"
#include "libprecompiled/Common.h"
#include "libstorage/EntriesPrecompiled.h"
#include "libstorage/StorageException.h"
#include "libstorage/TableFactoryPrecompiled.h"
#include "wedpr-generated/ffi_anonymous_voting.h"
#include "wedpr-generated/ffi_hidden_asset.h"
#include <libdevcore/Common.h>
#include <libdevcore/easylog.h>
#include <libdevcrypto/Hash.h>

namespace dev
{
namespace precompiled
{
using namespace dev;
using namespace std;
using namespace dev::blockverifier;
using namespace dev::storage;
using namespace dev::precompiled;

// hidden asset
const char API_HIDDEN_ASSET_VERIFY_ISSUED_CREDIT[] = "hiddenAssetVerifyIssuedCredit(string)";
const char API_HIDDEN_ASSET_VERIFY_FULFILLED_CREDIT[] = "hiddenAssetVerifyFulfilledCredit(string)";
const char API_HIDDEN_ASSET_VERIFY_TRANSFERRED_CREDIT[] =
    "hiddenAssetVerifyTransferredCredit(string)";
const char API_HIDDEN_ASSET_VERIFY_SPLIT_CREDIT[] = "hiddenAssetVerifySplitCredit(string)";

// anonymous voting
const char API_ANONYMOUS_VOTING_BOUNDED_VERIFY_VOTE_REQUEST[] =
    "anonymousVotingVerifyBoundedVoteRequest(bytes,bytes)";
// anonymous voting
const char API_ANONYMOUS_VOTING_UNBOUNDED_VERIFY_VOTE_REQUEST[] =
    "anonymousVotingVerifyUnboundedVoteRequest(bytes,bytes)";
const char API_ANONYMOUS_VOTING_AGGREGATE_VOTE_SUM_RESPONSE[] =
    "anonymousVotingAggregateVoteSumResponse(bytes,bytes,bytes)";
const char API_ANONYMOUS_VOTING_VERIFY_COUNT_REQUEST[] =
    "anonymousVotingVerifyCountRequest(bytes,bytes,string,bytes)";
const char API_ANONYMOUS_VOTING_AGGREGATE_DECRYPTED_PART_SUM[] =
    "anonymousVotingAggregateDecryptedPartSum(bytes,bytes,bytes)";
const char API_ANONYMOUS_VOTING_COUNT_CANDIDATES_RESULT[] =
    "anonymousVotingCountCandidatesResult(bytes,bytes,bytes)";

const char WEDPR_VERFIY_FAILED[] = "verfiy failed";

const int WEDPR_SUCCESS = 0;
const int WEDPR_FAILURE = -1;

const char WEDPR_PRECOMPILED[] = "WedprPrecompiled";

WedprPrecompiled::WedprPrecompiled()
{
    // hidden asset
    name2Selector[API_HIDDEN_ASSET_VERIFY_ISSUED_CREDIT] =
        getFuncSelector(API_HIDDEN_ASSET_VERIFY_ISSUED_CREDIT);
    name2Selector[API_HIDDEN_ASSET_VERIFY_FULFILLED_CREDIT] =
        getFuncSelector(API_HIDDEN_ASSET_VERIFY_FULFILLED_CREDIT);
    name2Selector[API_HIDDEN_ASSET_VERIFY_TRANSFERRED_CREDIT] =
        getFuncSelector(API_HIDDEN_ASSET_VERIFY_TRANSFERRED_CREDIT);
    name2Selector[API_HIDDEN_ASSET_VERIFY_SPLIT_CREDIT] =
        getFuncSelector(API_HIDDEN_ASSET_VERIFY_SPLIT_CREDIT);

    // anonymous voting
    name2Selector[API_ANONYMOUS_VOTING_UNBOUNDED_VERIFY_VOTE_REQUEST] =
        getFuncSelector(API_ANONYMOUS_VOTING_UNBOUNDED_VERIFY_VOTE_REQUEST);
    name2Selector[API_ANONYMOUS_VOTING_BOUNDED_VERIFY_VOTE_REQUEST] =
        getFuncSelector(API_ANONYMOUS_VOTING_BOUNDED_VERIFY_VOTE_REQUEST);
    name2Selector[API_ANONYMOUS_VOTING_AGGREGATE_VOTE_SUM_RESPONSE] =
        getFuncSelector(API_ANONYMOUS_VOTING_AGGREGATE_VOTE_SUM_RESPONSE);
    name2Selector[API_ANONYMOUS_VOTING_VERIFY_COUNT_REQUEST] =
        getFuncSelector(API_ANONYMOUS_VOTING_VERIFY_COUNT_REQUEST);
    name2Selector[API_ANONYMOUS_VOTING_AGGREGATE_DECRYPTED_PART_SUM] =
        getFuncSelector(API_ANONYMOUS_VOTING_AGGREGATE_DECRYPTED_PART_SUM);
    name2Selector[API_ANONYMOUS_VOTING_COUNT_CANDIDATES_RESULT] =
        getFuncSelector(API_ANONYMOUS_VOTING_COUNT_CANDIDATES_RESULT);
}

std::string WedprPrecompiled::toString()
{
    return WEDPR_PRECOMPILED;
}

bytes WedprPrecompiled::call(
    ExecutiveContext::Ptr context, bytesConstRef param, Address const& origin)
{
    PRECOMPILED_LOG(TRACE) << LOG_BADGE(WEDPR_PRECOMPILED) << LOG_DESC("call")
                           << LOG_KV("param", toHex(param)) << LOG_KV("origin", origin)
                           << LOG_KV("context", context->txGasLimit());

    uint32_t func = getParamFunc(param);
    bytesConstRef data = getParamData(param);

    dev::eth::ContractABI abi;
    bytes out;

    // hiddenAssetVerifyIssuedCredit(bytes issueArgument)
    if (func == name2Selector[API_HIDDEN_ASSET_VERIFY_ISSUED_CREDIT])
    {
        out = verifyIssuedCredit(abi, data);
    }
    // hiddenAssetVerifyFulfilledCredit(bytes fulfillArgument)
    else if (func == name2Selector[API_HIDDEN_ASSET_VERIFY_FULFILLED_CREDIT])
    {
        out = verifyFulfilledCredit(abi, data);
    }
    // hiddenAssetVerifyTransferredCredit(bytes transferRequest)
    else if (func == name2Selector[API_HIDDEN_ASSET_VERIFY_TRANSFERRED_CREDIT])
    {
        out = verifyTransferredCredit(abi, data);
    }
    // hiddenAssetVerifySplitCredit(bytes splitRequest)
    else if (func == name2Selector[API_HIDDEN_ASSET_VERIFY_SPLIT_CREDIT])
    {
        out = verifySplitCredit(abi, data);
    }
    // anonymousVotingVerifyVoteRequest(bytes systemParameters, bytes voteRequest)
    else if (func == name2Selector[API_ANONYMOUS_VOTING_BOUNDED_VERIFY_VOTE_REQUEST])
    {
        out = verifyBoundedVoteRequest(abi, data);
    }
    // anonymousVotingVerifyVoteRequest(bytes systemParameters, bytes voteRequest)
    else if (func == name2Selector[API_ANONYMOUS_VOTING_UNBOUNDED_VERIFY_VOTE_REQUEST])
    {
        out = verifyUnboundedVoteRequest(abi, data);
    }
    // anonymousVotingAggregateVoteSumResponse(bytes systemParameters, bytes voteRequest, bytes
    // voteStorage)
    else if (func == name2Selector[API_ANONYMOUS_VOTING_AGGREGATE_VOTE_SUM_RESPONSE])
    {
        out = aggregateVoteSumResponse(abi, data);
    }
    // anonymousVotingVerifyCountRequest(bytes systemParameters, bytes voteStorage, string
    // hPointShare, bytes decryptedRequest)
    else if (func == name2Selector[API_ANONYMOUS_VOTING_VERIFY_COUNT_REQUEST])
    {
        out = verifyCountRequest(abi, data);
    }
    // anonymousVotingAggregateDecryptedPartSum(bytes systemParameters, bytes decryptedRequest,
    // bytes decryptedResultPartStorage)
    else if (func == name2Selector[API_ANONYMOUS_VOTING_AGGREGATE_DECRYPTED_PART_SUM])
    {
        out = aggregateDecryptedPartSum(abi, data);
    }
    // anonymousVotingCountsCandidatesResult(bytes systemParameters, bytes voteStorage, bytes
    // voteSumTotal)
    else if (func == name2Selector[API_ANONYMOUS_VOTING_COUNT_CANDIDATES_RESULT])
    {
        out = countCandidatesResult(abi, data);
    }
    else
    {
        // unknown function call
        logError(WEDPR_PRECOMPILED, "unknown func", "func", std::to_string(func));
        throwException("unknown func");
    }
    return out;
}

bytes WedprPrecompiled::verifyIssuedCredit(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    // parse parameter
    std::string issueArgument;
    abi.abiOut(data, issueArgument);

    // verify issued credit
    char* issueArgumentChar = stringToChar(issueArgument);
    if (verify_issued_credit(issueArgumentChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_issued_credit", WEDPR_VERFIY_FAILED);
        throwException("verify_issued_credit failed");
    }

    std::string currentCredit = get_current_credit_by_issue_argument(issueArgumentChar);
    std::string creditStorage = get_storage_credit_by_issue_argument(issueArgumentChar);

    return abi.abiIn("", currentCredit, creditStorage);
}

bytes WedprPrecompiled::verifyFulfilledCredit(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string fulfillArgument;
    abi.abiOut(data, fulfillArgument);
    char* fulfillArgumentChar = stringToChar(fulfillArgument);
    if (verify_fulfilled_credit(fulfillArgumentChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_fulfilled_credit", WEDPR_VERFIY_FAILED);
        throwException("verify_fulfilled_credit failed");
    }

    std::string currentCredit = get_current_credit_by_fulfill_argument(fulfillArgumentChar);
    std::string creditStorage = get_credit_storage_by_fulfill_argument(fulfillArgumentChar);

    return abi.abiIn("", currentCredit, creditStorage);
}

bytes WedprPrecompiled::verifyTransferredCredit(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string transferRequest;
    abi.abiOut(data, transferRequest);

    char* transferRequestChar = stringToChar(transferRequest);
    if (verify_transferred_credit(transferRequestChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_transfer_credit", WEDPR_VERFIY_FAILED);
        throwException("verify_transfer_credit failed");
    }

    std::string spentCurrentCredit =
        get_spent_current_credit_by_transfer_request(transferRequestChar);
    std::string spentCreditStorage =
        get_spent_credit_storage_by_transfer_request(transferRequestChar);
    std::string newCurrentCredit = get_new_current_credit_by_transfer_request(transferRequestChar);
    std::string newCreditStorage = get_new_credit_storage_by_transfer_request(transferRequestChar);

    return abi.abiIn(
        "", spentCurrentCredit, spentCreditStorage, newCurrentCredit, newCreditStorage);
}

bytes WedprPrecompiled::verifySplitCredit(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string splitRequest;
    abi.abiOut(data, splitRequest);

    char* splitRequestChar = stringToChar(splitRequest);
    if (verify_split_credit(splitRequestChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_split_credit", WEDPR_VERFIY_FAILED);
        throwException("verify_split_credit failed");
    }

    std::string spentCurrentCredit = get_spent_current_credit_by_split_request(splitRequestChar);
    std::string spentCreditStorage = get_spent_credit_storage_by_split_request(splitRequestChar);
    std::string newCurrentCredit1 = get_new_current_credit1_by_split_request(splitRequestChar);
    std::string newCreditStorage1 = get_new_credit_storage1_by_split_request(splitRequestChar);
    std::string newCurrentCredit2 = get_new_current_credit2_by_split_request(splitRequestChar);
    std::string newCreditStorage2 = get_new_credit_storage2_by_split_request(splitRequestChar);

    // return currentCredit and creditStorage
    return abi.abiIn("", spentCurrentCredit, spentCreditStorage, newCurrentCredit1,
        newCreditStorage1, newCurrentCredit2, newCreditStorage2);
}

bytes WedprPrecompiled::verifyBoundedVoteRequest(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string systemParameters;
    std::string voteRequest;
    abi.abiOut(data, systemParameters, voteRequest);

    char* systemParametersChar = stringToChar(systemParameters);
    char* voteRequestChar = stringToChar(voteRequest);
    if (verify_bounded_vote_request(systemParametersChar, voteRequestChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_vote_request", WEDPR_VERFIY_FAILED);
        throwException("verify_vote_request failed");
    }

    return abi.abiIn("", WEDPR_SUCCESS);
}

bytes WedprPrecompiled::verifyUnboundedVoteRequest(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string systemParameters;
    std::string voteRequest;
    abi.abiOut(data, systemParameters, voteRequest);

    char* systemParametersChar = stringToChar(systemParameters);
    char* voteRequestChar = stringToChar(voteRequest);
    if (verify_unbounded_vote_request(systemParametersChar, voteRequestChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_vote_request", WEDPR_VERFIY_FAILED);
        throwException("verify_vote_request failed");
    }

    return abi.abiIn("", WEDPR_SUCCESS);
}

bytes WedprPrecompiled::aggregateVoteSumResponse(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string systemParameters;
    std::string voteRequest;
    std::string voteStorage;
    abi.abiOut(data, systemParameters, voteRequest, voteStorage);

    char* systemParametersChar = stringToChar(systemParameters);
    char* voteRequestChar = stringToChar(voteRequest);
    char* voteStorageChar = stringToChar(voteStorage);

    char* voteStoragePartChar = get_vote_storage_from_vote_request(voteRequestChar);
    std::string blankBallot = get_blank_ballot_from_vote_storage(voteStoragePartChar);
    std::string voteStoragePart = voteStoragePartChar;
    std::string voteStorageSum =
        aggregate_vote_sum_response(systemParametersChar, voteRequestChar, voteStorageChar);

    return abi.abiIn("", blankBallot, voteStoragePart, voteStorageSum);
}

bytes WedprPrecompiled::verifyCountRequest(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string systemParameters;
    std::string voteStorage;
    std::string hPointShare;
    std::string decryptedRequest;
    abi.abiOut(data, systemParameters, voteStorage, hPointShare, decryptedRequest);

    char* systemParametersChar = stringToChar(systemParameters);
    char* voteStorageChar = stringToChar(voteStorage);
    char* hPointShareChar = stringToChar(hPointShare);
    char* decryptedRequestChar = stringToChar(decryptedRequest);
    if (verify_count_request(systemParametersChar, voteStorageChar, hPointShareChar,
            decryptedRequestChar) != WEDPR_SUCCESS)
    {
        logError(WEDPR_PRECOMPILED, "verify_count_request", WEDPR_VERFIY_FAILED);
        throwException("verify_count_request failed");
    }

    return abi.abiIn("", WEDPR_SUCCESS);
}
bytes WedprPrecompiled::aggregateDecryptedPartSum(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string systemParameters;
    std::string decryptedRequest;
    std::string decryptedResultPartStorage;
    abi.abiOut(data, systemParameters, decryptedRequest, decryptedResultPartStorage);
    char* systemParametersChar = stringToChar(systemParameters);
    char* decryptedRequestChar = stringToChar(decryptedRequest);
    char* decryptedResultPartStorageChar = stringToChar(decryptedResultPartStorage);

    std::string counterId = get_counter_id_from_decrypted_result_part_request(decryptedRequestChar);
    std::string decryptedResultPartStoragePart =
        get_decrypted_result_part_storage_from_decrypted_result_part_request(decryptedRequestChar);
    std::string decryptedResultPartStorageSum = aggregate_decrypted_part_sum(
        systemParametersChar, decryptedRequestChar, decryptedResultPartStorageChar);

    return abi.abiIn("", counterId, decryptedResultPartStoragePart, decryptedResultPartStorageSum);
}
bytes WedprPrecompiled::countCandidatesResult(dev::eth::ContractABI& abi, bytesConstRef& data)
{
    std::string systemParameters;
    std::string voteStorage;
    std::string voteSumTotal;
    abi.abiOut(data, systemParameters, voteStorage, voteSumTotal);

    char* systemParametersChar = stringToChar(systemParameters);
    char* voteStorageChar = stringToChar(voteStorage);
    char* voteSumTotalChar = stringToChar(voteSumTotal);
    std::string countResult =
        count_candidates_result(systemParametersChar, voteStorageChar, voteSumTotalChar);

    return abi.abiIn("", countResult);
}
}  // namespace precompiled
}  // namespace dev
