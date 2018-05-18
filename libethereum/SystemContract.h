/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file: SystemContract.h
 * @author: fisco-dev
 * 
 * @date: 2017
 */

#pragma once
#include <libdevcore/Guards.h>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>
#include <libdevcore/CommonIO.h>
#include "Client.h"
#include "SystemContractApi.h"

using namespace std;
using namespace dev;

using namespace dev::eth;


namespace dev
{
namespace eth
{

enum class FilterType {
    Account,
    Node
};

class SystemContractApi;
enum class SystemContractCode;

using Address = h160;


/**
 * @brief Main API hub for System Contract
 */

struct SystemFilter {
    Address filter;
    string  name;
};

struct SystemAction {
    Address action;
    string  name;
};

class SystemContract: public SystemContractApi
{
public:
    /// Constructor.
    SystemContract(const Address _address, const Address _god, Client* _client) : SystemContractApi(  _address, _god), m_client(_client), m_tempblock(0)
    {
        //m_tempblock = m_client->block(m_client->blockChain().number());
       // Transactions ts;
       // updateSystemContract(ts);

        std::shared_ptr<Block> tempblock(new Block(0));
        *tempblock = m_client->block(m_client->blockChain().number());

        //Transactions ts;
        updateSystemContract(tempblock);

    }

    /// Destructor.
    virtual ~SystemContract() {}

   
    virtual u256 transactionFilterCheck(const Transaction & transaction) override;

   virtual void startStatTranscation(h256)override;

    virtual void updateCache(Address address) override;

   
    virtual bool isAdmin(const Address & _address) override;
    
    virtual bool getValue(const string _key, string & _value) override;

    
    virtual void getAllNode(int _blocknumber ,std::vector< NodeConnParams> & _nodelist )override;
    virtual u256 getBlockChainNumber()override;

    virtual void getCaInfo(string _hash,CaInfo & _cainfo) override;

  
    virtual void updateSystemContract(std::shared_ptr<Block> block) override;

    Address getRoute(const string & _route) const override;

private:

    std::map<h256, pair<u256,u256> > m_stattransation;

    Client* m_client;

    mutable SharedMutex  m_blocklock; 
   
    std::shared_ptr<Block> m_tempblock;

    mutable SharedMutex  m_lockroute;
    std::vector<SystemAction> m_routes;

    mutable SharedMutex  m_lockfilter;
    SystemFilter m_transactionfilter;
    std::map<h256, u256> m_filterchecktranscache; 

    unsigned m_transcachehit;
    unsigned m_transcount;

    mutable SharedMutex  m_locknode;
    std::vector< NodeConnParams> m_nodelist;

    mutable SharedMutex  m_lockca;
    std::map<string,CaInfo> m_calist;

	mutable Address m_abiMgrAddr;


    ExecutionResult call(Address const& _to, bytes const& _inputdata, bool cache = false) ;

    //ExecutionResult call(const std::string &name, bytes const& _inputdata) ; 

    h256 filterCheckTransCacheKey(const Transaction & _t) const ;

    void updateRoute( );
    void updateNode( );
    void tempGetAllNode(int _blocknumber,std::vector< NodeConnParams> & _nodevector);//在指定块上面获取列表
    void updateConfig( );
    void updateCa( );
	void updateContractAbiInfo();

    void getNodeFromContract(std::function<ExecutionResult(Address const,bytes const,bool cache )>,std::vector< NodeConnParams> & _nodelist);

    struct CallCache {
    	std::map<bytes, ExecutionResult> res;
    };

    std::map<Address, CallCache> _callCaches;
};



}
}



