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
 * @brief : Global configure of the node
 * @author: jimmyshi
 * @date: 2018-11-30
 */

#pragma once

#include <string>

namespace dev
{
enum VERSION
{
    RC1_VERSION = 1,
    RC2_VERSION = 2,
    LATEST_VERSION
};
class GlobalConfigure
{
public:
    static GlobalConfigure& instance()
    {
        static GlobalConfigure ins;
        return ins;
    }
    void setVersion(std::string const& versionStr)
    {
        if (dev::stringCmpIgnoreCase(versionStr, "2.0.0-rc1") == 0)
        {
            m_version = RC1_VERSION;
        }
        if (dev::stringCmpIgnoreCase(versionStr, "2.0.0-rc2") == 0)
        {
            m_version = RC2_VERSION;
        }
        else
        {
            m_version = LATEST_VERSION;
        }
    }

    VERSION const& version() const { return m_version; }
    void setCompress(bool const& compress) { m_compress = compress; }

    bool compressEnabled() const { return m_compress; }

    struct DiskEncryption
    {
        bool enable = false;
        std::string keyCenterIP;
        int keyCenterPort;
        std::string cipherDataKey;
    } diskEncryption;

    /// default block time
    const unsigned c_intervalBlockTime = 1000;
    /// omit empty block or not
    const bool c_omitEmptyBlock = true;
    /// default blockLimit
    const unsigned c_blockLimit = 1000;

    /// default compress threshold: 1KB
    const uint64_t c_compressThreshold = 1024;

private:
    VERSION m_version;
    bool m_compress;
};

#define g_BCOSConfig GlobalConfigure::instance()

}  // namespace dev