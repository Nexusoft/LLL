/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <TAO/API/rpc/types/rpc.h>
#include <Util/include/json.h>
#include <Util/include/runtime.h>
#include <LLP/include/version.h>

#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/include/difficulty.h>
#include <TAO/Ledger/include/retarget.h>
#include <TAO/Ledger/include/supply.h>

#include <LLP/include/global.h>
#include <LLP/include/base_address.h>
#include <LLP/include/legacy_address.h>
#include <LLP/include/manager.h>

#include <LLP/templates/data.h>

#include <Common/include/version.h>

#include <Legacy/wallet/wallet.h>
#include <Legacy/wallet/walletdb.h>
#include <Legacy/include/money.h>

#include <TAO/Ledger/types/mempool.h>
#include <TAO/API/system/types/system.h>
#include <LLP/include/lisp.h>

#include <vector>


/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* getinfo
        Returns an object containing various state info */
        json::json RPC::GetInfo(const json::json& params, bool fHelp)
        {
            if(fHelp || params.size() != 0)
                return std::string(
                    "getinfo - Returns an object containing various state info.");

            json::json obj;
            obj["version"] = version::CLIENT_VERSION_BUILD_STRING;
            obj["protocolversion"] = LLP::PROTOCOL_VERSION;
            obj["walletversion"] = Legacy::Wallet::GetInstance().GetVersion();
            obj["testnet"] = config::fTestNet.load();
            obj["balance"] = Legacy::SatoshisToAmount(Legacy::Wallet::GetInstance().GetBalance());
            obj["unconfirmedbalance"] = Legacy::SatoshisToAmount(Legacy::Wallet::GetInstance().GetUnconfirmedBalance());
            obj["newmint"] = Legacy::SatoshisToAmount(Legacy::Wallet::GetInstance().GetNewMint());
            obj["stake"] = Legacy::SatoshisToAmount(Legacy::Wallet::GetInstance().GetStake());

            /* Staking metrics */
            obj["staking"] = "Not Started";
            obj["genesismature"] = false;
            obj["stakerate"]   = 0.0;
            obj["trustweight"] = 0.0;
            obj["blockweight"] = 0.0;
            obj["stakeweight"] = 0.0;
            obj["txtotal"] = (int32_t)Legacy::Wallet::GetInstance().mapWallet.size();

            obj["blocks"] = (int32_t)TAO::Ledger::ChainState::nBestHeight.load();

            obj["timestamp"] =  (uint64_t)runtime::unifiedtimestamp();
            obj["offset"]    =  (int32_t)UNIFIED_AVERAGE_OFFSET.load();
            obj["connections"] = GetTotalConnectionCount();
            obj["synccomplete"] = (int32_t)TAO::Ledger::ChainState::PercentSynchronized();

            obj["proxy"] = (config::fUseProxy.load() ? LLP::addrProxy.ToString() : std::string());

            // get the EID's if using LISP
            std::map<std::string, LLP::EID> mapEIDs = LLP::GetEIDs();
            if(mapEIDs.size() > 0)
            {
                json::json jsonEIDs = json::json::array();
                for(const auto& eid : mapEIDs)
                {
                    jsonEIDs.push_back(eid.first);
                }
                obj["eids"] = jsonEIDs;
            }

            obj["testnet"]       = config::fTestNet.load();
            obj["keypoololdest"] = (int64_t)Legacy::Wallet::GetInstance().GetKeyPool().GetOldestKeyPoolTime();
            obj["keypoolsize"]   = Legacy::Wallet::GetInstance().GetKeyPool().GetKeyPoolSize();
            obj["paytxfee"]      = Legacy::SatoshisToAmount(Legacy::TRANSACTION_FEE);

            if(Legacy::Wallet::GetInstance().IsCrypted())
            {
                obj["locked"] = Legacy::Wallet::GetInstance().IsLocked();
                if(!Legacy::Wallet::GetInstance().IsLocked())
                {
                    if((uint64_t) Legacy::Wallet::GetInstance().GetWalletUnlockTime() > 0)
                        obj["unlocked_until"] = (uint64_t) Legacy::Wallet::GetInstance().GetWalletUnlockTime();

                    obj["minting_only"] = Legacy::fWalletUnlockMintOnly;
                }
            }

        //    obj.push_back(std::make_pair("errors",        Core::GetWarnings("statusbar")));


            return obj;
        }

        /* getpeerinfo
        Returns data about each connected network node */
        json::json RPC::GetPeerInfo(const json::json& params, bool fHelp)
        {
            json::json response;

            if(fHelp || params.size() != 0)
                    return std::string(
                        "getpeerinfo - Returns data about each connected network node.");

            /* Get the connections from the tritium server */
            std::vector<memory::atomic_ptr<LLP::TritiumNode>*> vConnections = LLP::TRITIUM_SERVER->GetConnections();

            /* Iterate the connections*/
            for(const auto& connection : vConnections)
            {
                /* Skip over inactive connections. */
                if(!connection->load())
                    continue;

                /* Push the active connection. */
                if(connection->load()->Connected())
                {
                    json::json obj;

                    obj["addr"]     = connection->load()->addr.ToString();
                    obj["type"]     = connection->load()->strFullVersion;
                    obj["version"]  = connection->load()->nProtocolVersion;
                    obj["session"]  = connection->load()->nCurrentSession;
                    obj["height"]   = connection->load()->nCurrentHeight;
                    obj["best"]     = connection->load()->hashBestChain.SubString();
                    obj["latency"]  = connection->load()->nLatency.load();
                    obj["lastseen"] = connection->load()->nLastPing.load();
                    obj["session"]  = connection->load()->nCurrentSession;
                    obj["outgoing"] = connection->load()->fOUTGOING.load();

                    response.push_back(obj);
       
                }
            }

            return response;
        }


        /* getmininginfo
        Returns an object containing mining-related information.*/
        json::json RPC::GetMiningInfo(const json::json& params, bool fHelp)
        {
            if(fHelp || params.size() != 0)
                return std::string(
                    "getmininginfo - Returns an object containing mining-related information.");

            // Prime
            uint64_t nPrimePS = 0;
            uint64_t nHashRate = 0;
            if(TAO::Ledger::ChainState::nBestHeight.load()
            && TAO::Ledger::ChainState::stateBest.load() != TAO::Ledger::ChainState::stateGenesis)
            {
                double nPrimeAverageDifficulty = 0.0;
                uint32_t nPrimeAverageTime = 0;
                uint32_t nPrimeTimeConstant = 2480;
                int32_t nTotal = 0;
                TAO::Ledger::BlockState blockState = TAO::Ledger::ChainState::stateBest.load();

                bool bLastStateFound = TAO::Ledger::GetLastState(blockState, 1);
                for(; (nTotal < 1440 && bLastStateFound); ++nTotal)
                {
                    uint64_t nLastBlockTime = blockState.GetBlockTime();
                    blockState = blockState.Prev();
                    bLastStateFound = TAO::Ledger::GetLastState(blockState, 1);

                    nPrimeAverageTime += (nLastBlockTime - blockState.GetBlockTime());
                    nPrimeAverageDifficulty += (TAO::Ledger::GetDifficulty(blockState.nBits, 1));

                }
                if(nTotal > 0)
                {
                    nPrimeAverageDifficulty /= nTotal;
                    nPrimeAverageTime /= nTotal;
                    nPrimePS = (nPrimeTimeConstant / nPrimeAverageTime) * std::pow(50.0, (nPrimeAverageDifficulty - 3.0));
                }
                else
                {
                    /* Edge case where there are no prime blocks so use the difficulty from genesis */
                    blockState = TAO::Ledger::ChainState::stateGenesis;
                    nPrimeAverageDifficulty += (TAO::Ledger::GetDifficulty(blockState.nBits, 1));
                }



                // Hash
                int32_t nHTotal = 0;
                uint32_t nHashAverageTime = 0;
                double nHashAverageDifficulty = 0.0;
                uint64_t nTimeConstant = 276758250000;

                blockState = TAO::Ledger::ChainState::stateBest.load();

                bLastStateFound = TAO::Ledger::GetLastState(blockState, 2);
                for(;  (nHTotal < 1440 && bLastStateFound); ++nHTotal)
                {
                    uint64_t nLastBlockTime = blockState.GetBlockTime();
                    blockState = blockState.Prev();
                    bLastStateFound = TAO::Ledger::GetLastState(blockState, 2);

                    nHashAverageTime += (nLastBlockTime - blockState.GetBlockTime());
                    nHashAverageDifficulty += (TAO::Ledger::GetDifficulty(blockState.nBits, 2));

                }
                // protect against getmininginfo being called before hash channel start block
                if(nHTotal > 0)
                {
                    nHashAverageDifficulty /= nHTotal;
                    nHashAverageTime /= nHTotal;

                    nHashRate = (nTimeConstant / nHashAverageTime) * nHashAverageDifficulty;
                }
                else
                {
                    /* Edge case where there are no hash blocks so use the difficulty from genesis */
                    blockState = TAO::Ledger::ChainState::stateGenesis;
                    nPrimeAverageDifficulty += (TAO::Ledger::GetDifficulty(blockState.nBits, 2));
                }
            }

            json::json obj;
            obj["blocks"] = (int32_t)TAO::Ledger::ChainState::nBestHeight.load();
            obj["timestamp"] = (int32_t)runtime::unifiedtimestamp();

            //PS TODO
            //obj["currentblocksize"] = (uint64_t)Core::nLastBlockSize;
            //obj["currentblocktx"] =(uint64_t)Core::nLastBlockTx;

            TAO::Ledger::BlockState lastStakeBlockState = TAO::Ledger::ChainState::stateBest.load();
            bool fHasStake = TAO::Ledger::GetLastState(lastStakeBlockState, 0);

            TAO::Ledger::BlockState lastPrimeBlockState = TAO::Ledger::ChainState::stateBest.load();
            bool fHasPrime = TAO::Ledger::GetLastState(lastPrimeBlockState, 1);

            TAO::Ledger::BlockState lastHashBlockState = TAO::Ledger::ChainState::stateBest.load();
            bool fHasHash = TAO::Ledger::GetLastState(lastHashBlockState, 2);


            if(fHasStake == false)
                debug::error(FUNCTION, "couldn't find last stake block state");
            if(fHasPrime == false)
                debug::error(FUNCTION, "couldn't find last prime block state");
            if(fHasHash == false)
                debug::error(FUNCTION, "couldn't find last hash block state");


            obj["stakeDifficulty"] = fHasStake ?
                TAO::Ledger::GetDifficulty(TAO::Ledger::GetNextTargetRequired(lastStakeBlockState, 0, false), 0) : 0;

            obj["primeDifficulty"] = fHasPrime ?
                TAO::Ledger::GetDifficulty(TAO::Ledger::GetNextTargetRequired(lastPrimeBlockState, 1, false), 1) : 0;

            obj["hashDifficulty"] = fHasHash ?
                TAO::Ledger::GetDifficulty(TAO::Ledger::GetNextTargetRequired(lastHashBlockState, 2, false), 2) : 0;

            obj["primeReserve"] =    Legacy::SatoshisToAmount(lastPrimeBlockState.nReleasedReserve[0]);
            obj["hashReserve"] =     Legacy::SatoshisToAmount(lastHashBlockState.nReleasedReserve[0]);
            obj["primeValue"] =        Legacy::SatoshisToAmount(TAO::Ledger::GetCoinbaseReward(TAO::Ledger::ChainState::stateBest.load(), 1, 0));
            obj["hashValue"] =         Legacy::SatoshisToAmount(TAO::Ledger::GetCoinbaseReward(TAO::Ledger::ChainState::stateBest.load(), 2, 0));
            obj["pooledtx"] =       (uint64_t)TAO::Ledger::mempool.Size();
            obj["primesPerSecond"] = nPrimePS;
            obj["hashPerSecond"] =  nHashRate;

            if(config::GetBoolArg("-mining", false))
            {
                //PS TODO
                //obj["totalConnections"] = LLP::MINING_LLP->TotalConnections();
            }

            //obj["genesisblockhash"] = TAO::Ledger::ChainState::stateGenesis.GetHash().GetHex();
            //obj["currentblockhash"] = TAO::Ledger::ChainState::stateBest.load().GetHash().GetHex();

            return obj;
        }
    }
}
