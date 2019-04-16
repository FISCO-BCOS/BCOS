/*
    This file is part of FISCO-BCOS.

    FISCO-BCOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FISCO-BCOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 *  @author jimmyshi
 *  @modify first draft
 *  @date 2018-11-30
 */


#include "GlobalConfigureInitializer.h"

using namespace std;
using namespace dev;
using namespace dev::initializer;

void GlobalConfigureInitializer::initConfig(const boost::property_tree::ptree& _pt)
{
    /// init version
    std::string version = _pt.get<std::string>("compatibility.supported_version", "2.0.0-rc1");
    if (dev::stringCmpIgnoreCase(version, "2.0.0-rc2") == 0)
    {
        g_BCOSConfig.setVersion(RC2_VERSION);
    }
    /// default is RC1
    else
    {
        g_BCOSConfig.setVersion(RC1_VERSION);
    }


    std::string sectionName = "data_secure";
    if (_pt.get_child_optional("storage_security"))
    {
        sectionName = "storage_security";
    }

    g_BCOSConfig.diskEncryption.enable = _pt.get<bool>(sectionName + ".enable", false);
    g_BCOSConfig.diskEncryption.keyCenterIP =
        _pt.get<std::string>(sectionName + ".key_manager_ip", "");
    g_BCOSConfig.diskEncryption.keyCenterPort = _pt.get<int>(sectionName + ".key_manager_port", 0);
    g_BCOSConfig.diskEncryption.cipherDataKey =
        _pt.get<std::string>(sectionName + ".cipher_data_key", "");

    /// compress related option, default enable
    bool enableCompress = _pt.get<bool>("p2p.enable_compress", true);
    g_BCOSConfig.setCompress(enableCompress);

    /// init version
    uint64_t chainId = _pt.get<uint64_t>("chain.id", 1);
    g_BCOSConfig.setChainId(chainId);

    INITIALIZER_LOG(DEBUG) << LOG_BADGE("initKeyManagerConfig") << LOG_DESC("load configuration")
                           << LOG_KV("enable", g_BCOSConfig.diskEncryption.enable)
                           << LOG_KV("url.IP", g_BCOSConfig.diskEncryption.keyCenterIP)
                           << LOG_KV("url.port",
                                  std::to_string(g_BCOSConfig.diskEncryption.keyCenterPort))
                           << LOG_KV("key", g_BCOSConfig.diskEncryption.cipherDataKey)
                           << LOG_KV("enableCompress", g_BCOSConfig.compressEnabled())
                           << LOG_KV("version_str", version)
                           << LOG_KV("version", g_BCOSConfig.version())
                           << LOG_KV("chainId", g_BCOSConfig.chainId());
}