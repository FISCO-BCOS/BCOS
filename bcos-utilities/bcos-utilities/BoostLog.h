/**
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
 * @brief: define Log
 *
 * @file: Log.h
 * @author: yujiechen
 * @date 2021-02-24
 */
#pragma once

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN32_)
// to fix boost log link error
// https://github.com/microsoft/vcpkg/discussions/22762
#define BOOST_USE_WINAPI_VERSION BOOST_WINAPI_VERSION_WIN7
#endif
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>

// BCOS log format
#ifndef LOG_BADGE
#define LOG_BADGE(_NAME) "[" << (_NAME) << "]"
#endif

#ifndef LOG_TYPE
#define LOG_TYPE(_TYPE) (_TYPE) << "|"
#endif

#ifndef LOG_DESC
#define LOG_DESC(_DESCRIPTION) (_DESCRIPTION)
#endif

#ifndef LOG_KV
#define LOG_KV(_K, _V) "," << (_K) << "=" << (_V)
#endif

#ifdef ERROR
#undef ERROR
#endif

namespace bcos
{
extern std::string const FileLogger;
/// the file logger
extern boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level,
    std::string>
    FileLoggerHandler;

// the statFileLogger
extern std::string const StatFileLogger;
extern boost::log::sources::severity_channel_logger_mt<boost::log::trivial::severity_level,
    std::string>
    StatFileLoggerHandler;

enum LogLevel
{
    TRACE = boost::log::trivial::trace,
    DEBUG = boost::log::trivial::debug,
    INFO = boost::log::trivial::info,
    WARNING = boost::log::trivial::warning,
    ERROR = boost::log::trivial::error,
    FATAL = boost::log::trivial::fatal,
};

extern LogLevel c_fileLogLevel;
extern LogLevel c_statLogLevel;

void setFileLogLevel(LogLevel const& _level);
void setStatLogLevel(LogLevel const& _level);

#define BCOS_LOG(level)                                \
    if (bcos::LogLevel::level >= bcos::c_fileLogLevel) \
    BOOST_LOG_SEV(                                     \
        bcos::FileLoggerHandler, (boost::log::trivial::severity_level)(bcos::LogLevel::level))
// for block number log
#define BLOCK_NUMBER(NUMBER) "[blk-" << (NUMBER) << "]"
}  // namespace bcos