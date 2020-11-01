/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/hash/SK.h>

#include <LLD/include/global.h>

#include <TAO/API/types/finance.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/json.h>
#include <TAO/API/include/utils.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/stake.h>
#include <TAO/Ledger/include/stake_change.h>

#include <TAO/Ledger/types/sigchain.h>
#include <TAO/Ledger/types/stake_manager.h>
#include <TAO/Ledger/types/stake_minter.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/types/object.h>

#include <Util/include/config.h>


/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Get staking metrics for a trust account */
        json::json Finance::Info(const json::json& params, bool fHelp)
        {
            json::json ret;

            /* Get the session to be used for this API call */
            Session& session = users->GetSession(params);

            /* Get the user account. */
            const memory::encrypted_ptr<TAO::Ledger::SignatureChain>& user = session.GetAccount();
            if(!user)
                throw APIException(-10, "Invalid session ID");

            /* Retrieve the trust register address, which is based on the users genesis */
            TAO::Register::Address hashRegister =
                TAO::Register::Address(std::string("trust"), user->Genesis(), TAO::Register::Address::TRUST);

            /* Get trust account. Any trust account that has completed Genesis will be indexed. */
            TAO::Register::Object trust;

            /* Check for trust index. */
            if(!LLD::Register->ReadTrust(user->Genesis(), trust)
            && !LLD::Register->ReadState(hashRegister, trust, TAO::Ledger::FLAGS::MEMPOOL))
                throw APIException(-70, "Trust account not found");

            /* Parse the object. */
            if(!trust.Parse())
                throw APIException(-71, "Unable to parse trust account.");

            /* Check the object standard. */
            if(trust.Standard() != TAO::Register::OBJECTS::TRUST)
                throw APIException(-72, "Register is not a trust account");

            /* Check the account is a NXS account */
            if(trust.get<uint256_t>("token") != 0)
                throw APIException(-73, "Trust account is not a NXS account.");

            /* Set trust account values for return data */
            ret["address"] = hashRegister.ToString();

            ret["balance"] = (double)trust.get<uint64_t>("balance") / TAO::Ledger::NXS_COIN;

            ret["stake"] = (double)trust.get<uint64_t>("stake") / TAO::Ledger::NXS_COIN;

            /* Trust is returned as a percentage of maximum */
            uint64_t nTrustScore = trust.get<uint64_t>("trust");

            ret["trust"] = nTrustScore;

            /* Check whether or not a stake minter is running for the current session */
            const TAO::Ledger::StakeManager& stakeManager = TAO::Ledger::StakeManager::GetInstance();
            bool fStaking = stakeManager.IsStaking(session.ID());

            /* Indexed trust account has genesis */
            bool fTrustIndexed = LLD::Register->HasTrust(user->Genesis());

            ret["new"] = (bool)(!fTrustIndexed);

            /* Return whether stake minter is started and actively running. */
            ret["staking"] = (bool)(fStaking && trust.hashOwner == user->Genesis());

            /* Flag indicating whether pooled staking is enabled */
            ret["pooled"] = stakeManager.IsPoolStaking();

            /* Need the stake minter running for accessing current staking metrics.
             * Verifying current user ownership of trust account is a sanity check.
             */
            if(fStaking && trust.hashOwner == user->Genesis())
            {
                /* Find the stake minter for current session */
                TAO::Ledger::StakeMinter* pStakeMinter = stakeManager.FindStakeMinter(session.ID());

                if(pStakeMinter) //this should never be nullptr if fStaking true, but check just in case
                {
                    /* The trust account is on hold when it does not have genesis, and is waiting to reach minimum age to stake */
                    bool fOnHold = (!fTrustIndexed && pStakeMinter->IsWaitPeriod());

                    /* When trust account is on hold pending minimum age, also return the time remaining in hold period. */
                    ret["onhold"] = (bool)(fOnHold);
                    if(fOnHold)
                        ret["holdtime"] = (uint64_t)pStakeMinter->GetWaitTime();

                    /* If stake minter is running, get current stake rate it is using. */
                    ret["stakerate"] = pStakeMinter->GetStakeRatePercent();

                    /* Other staking metrics also are available with running minter */
                    ret["trustweight"] = pStakeMinter->GetTrustWeightPercent();
                    ret["blockweight"] = pStakeMinter->GetBlockWeightPercent();

                    /* Raw trust weight and block weight total to 100, so can use the total as a % directly */
                    ret["stakeweight"] = pStakeMinter->GetTrustWeight() + pStakeMinter->GetBlockWeight();
                }
            }
            else
            {
                /* When stake minter not running, return latest known value calculated from trust score (as an annual percent). */
                ret["stakerate"] = TAO::Ledger::StakeRate(nTrustScore, (nTrustScore == 0)) * 100.0;

                /* Other staking metrics not available if stake minter not running */
                ret["trustweight"] = 0.0;
                ret["blockweight"] = 0.0;
                ret["stakeweight"] = 0.0;
            }

            TAO::Ledger::StakeChange stakeChange;
            if(LLD::Local->ReadStakeChange(user->Genesis(), stakeChange) && !stakeChange.fProcessed
            && (stakeChange.nExpires == 0 || stakeChange.nExpires > runtime::unifiedtimestamp()))
            {
                ret["change"] = true;
                ret["amount"] = (double)stakeChange.nAmount / TAO::Ledger::NXS_COIN;
                ret["requested"] = stakeChange.nTime;
                ret["expires"] = stakeChange.nExpires;
            }
            else
                ret["change"] = false;

            if(config::fMultiuser.load())
                ret["session"] = session.ID().ToString();

            /* If the caller has requested to filter on a fieldname then filter out the json response to only include that field */
            FilterResponse(params, ret);

            return ret;
        }
    }
}
