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
 * @file DistributedRateLimiter.h
 * @author: octopus
 * @date 2022-06-30
 */

#pragma once

#include "bcos-gateway/Common.h"
#include "bcos-utilities/BoostLog.h"
#include <bcos-gateway/libratelimit/RateLimiterInterface.h>
#include <bcos-utilities/Common.h>
#include <sw/redis++/redis++.h>

namespace bcos
{
namespace gateway
{
namespace ratelimiter
{

class DistributedRateLimiter : public RateLimiterInterface
{
public:
    using Ptr = std::shared_ptr<DistributedRateLimiter>;
    using ConstPtr = std::shared_ptr<const DistributedRateLimiter>;
    using UniquePtr = std::unique_ptr<const DistributedRateLimiter>;

public:
    const static std::string luaScript;

public:
    DistributedRateLimiter(const std::string& _rateLimitKey, int64_t _maxPermits,
        std::shared_ptr<sw::redis::Redis> _redis)
      : m_rateLimitKey(_rateLimitKey), m_maxPermits(_maxPermits), m_redis(_redis)
    {
        GATEWAY_LOG(INFO) << LOG_BADGE("DistributedRateLimiter::NEWOBJ")
                          << LOG_DESC("construct distributed rate limiter")
                          << LOG_KV("rateLimitKey", _rateLimitKey)
                          << LOG_KV("maxPermits", _maxPermits);
    }

    DistributedRateLimiter(DistributedRateLimiter&&) = delete;
    DistributedRateLimiter(const DistributedRateLimiter&) = delete;
    DistributedRateLimiter& operator=(const DistributedRateLimiter&) = delete;
    DistributedRateLimiter& operator=(DistributedRateLimiter&&) = delete;

public:
    ~DistributedRateLimiter() override {}

public:
    /**
     * @brief acquire permits
     *
     * @param _requiredPermits
     * @return void
     */
    void acquire(int64_t _requiredPermits) override;

    /**
     * @brief
     *
     * @param _requiredPermits
     * @return true
     * @return false
     */
    bool tryAcquire(int64_t _requiredPermits) override;


    /**
     * @brief
     *
     * @return
     */
    void rollback(int64_t _requiredPermits) override;

public:
    int64_t maxPermits() const { return m_maxPermits; }
    std::string rateLimitKey() const { return m_rateLimitKey; }
    std::shared_ptr<sw::redis::Redis> redis() const { return m_redis; }

private:
    // key for distributed limit
    std::string m_rateLimitKey;
    //
    int64_t m_maxPermits;
    // redis instance
    std::shared_ptr<sw::redis::Redis> m_redis;
};

}  // namespace ratelimiter
}  // namespace gateway
}  // namespace bcos
