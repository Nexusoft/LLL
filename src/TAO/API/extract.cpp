/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLD/include/global.h>

#include <TAO/Register/types/address.h>

#include <TAO/API/include/extract.h>
#include <TAO/API/include/constants.h>
#include <TAO/API/types/exception.h>
#include <TAO/API/types/session.h>
#include <TAO/API/types/commands.h>

#include <TAO/API/users/types/users.h>
#include <TAO/API/names/types/names.h>

#include <TAO/Register/include/names.h>

/* Global TAO namespace. */
namespace TAO::API
{
    /* Extract an address from incoming parameters to derive from name or address field. */
    uint256_t ExtractAddress(const json::json& jParams, const std::string& strSuffix, const std::string& strDefault)
    {
        /* Cache a couple keys we will be using. */
        const std::string strName = "name"    + (strSuffix.empty() ? ("") : ("_" + strSuffix));
        const std::string strAddr = "address" + (strSuffix.empty() ? ("") : ("_" + strSuffix));

        /* If name is provided then use this to deduce the register address, */
        if(jParams.find(strName) != jParams.end())
        {
            /* Check for the ALL name, that debits from all relevant accounts. */
            if(jParams[strName] == "all")
            {
                /* Check for send to all */
                if(strSuffix == "to")
                    throw APIException(-310, "Cannot sent to ANY/ALL accounts");

                return TAO::API::ADDRESS_ALL;
            }


            /* Check for the ANY name, that debits from any account of any token, mixed */
            if(jParams[strName] == "any")
            {
                /* Check for send to all */
                if(strSuffix == "to")
                    throw APIException(-310, "Cannot sent to ANY/ALL accounts");

                return TAO::API::ADDRESS_ANY;
            }

            return Names::ResolveAddress(jParams, jParams[strName].get<std::string>());
        }

        /* Otherwise let's check for the raw address format. */
        else if(jParams.find(strAddr) != jParams.end())
        {
            /* Declare our return value. */
            const TAO::Register::Address hashRet =
                TAO::Register::Address(jParams[strAddr].get<std::string>());

            /* Check that it is valid */
            if(!hashRet.IsValid())
                throw APIException(-165, "Invalid " + strAddr);

            return hashRet;
        }

        /* Check for our default values. */
        else if(!strDefault.empty())
        {
            /* Declare our return value. */
            const TAO::Register::Address hashRet =
                TAO::Register::Address(strDefault);

            /* Check that this is valid address, invalid will be if default value is a name. */
            if(hashRet.IsValid())
                return hashRet;

            /* Get our session to get the name object. */
            const Session& session = Commands::Get<Users>()->GetSession(jParams);

            /* Grab the name object register now. */
            TAO::Register::Object object;
            if(TAO::Register::GetNameRegister(session.GetAccount()->Genesis(), strDefault, object))
                return object.get<uint256_t>("address");
        }

        /* This exception is for name_to/address_to */
        else if(strSuffix == "to")
            throw APIException(-64, "Missing recipient account name_to / address_to");

        /* This exception is for name_proof/address_proof */
        else if(strSuffix == "proof")
            throw APIException(-54, "Missing name_proof / address_proof to credit");

        /* This exception is for name/address */
        throw APIException(-33, "Missing name / address");
    }


    /* Extract an address from incoming parameters to derive from name or address field. */
    uint256_t ExtractToken(const json::json& jParams)
    {
        /* If name is provided then use this to deduce the register address, */
        if(jParams.find("token_name") != jParams.end())
        {
            /* Check for default NXS token or empty name fields. */
            if(jParams["token_name"] == "NXS" || jParams["token_name"].empty())
                return 0;

            return Names::ResolveAddress(jParams, jParams["token_name"].get<std::string>());
        }

        /* Otherwise let's check for the raw address format. */
        else if(jParams.find("token") != jParams.end())
        {
            /* Declare our return value. */
            const TAO::Register::Address hashRet =
                TAO::Register::Address(jParams["token"].get<std::string>());

            /* Check that it is valid */
            if(!hashRet.IsValid())
                throw APIException(-165, "Invalid token");

            return hashRet;
        }

        return 0;
    }
} // End TAO namespace