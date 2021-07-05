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

#include <Util/include/string.h>

/* Global TAO namespace. */
namespace TAO::API
{
    /* Extract an address from incoming parameters to derive from name or address field. */
    uint256_t ExtractAddress(const encoding::json& jParams, const std::string& strSuffix, const std::string& strDefault)
    {
        /* Cache a couple keys we will be using. */
        const std::string strName = "name"    + (strSuffix.empty() ? ("") : ("_" + strSuffix));
        const std::string strAddr = "address" + (strSuffix.empty() ? ("") : ("_" + strSuffix));

        /* If name is provided then use this to deduce the register address, */
        if(jParams.find(strName) != jParams.end())
        {
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
                throw Exception(-165, "Invalid " + strAddr);

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

        /* Check for any/all request types. */
        if(jParams.find("request") != jParams.end() && jParams["request"].find("type") != jParams["request"].end())
        {
            /* Grab a copy of our request type. */
            const std::string strType =
                jParams["request"]["type"].get<std::string>();

            /* Check for the ALL name, that debits from all relevant accounts. */
            if(strType == "all")
                return TAO::API::ADDRESS_ALL;

            /* Check for the ANY name, that debits from any account of any token, mixed */
            if(strType == "any")
                return TAO::API::ADDRESS_ANY;
        }

        /* This exception is for name_to/address_to */
        if(strSuffix == "to")
            throw Exception(-64, "Missing recipient account name_to / address_to");

        /* This exception is for name_proof/address_proof */
        if(strSuffix == "proof")
            throw Exception(-54, "Missing name_proof / address_proof to credit");

        /* This exception is for name/address */
        throw Exception(-33, "Missing name / address");
    }


    /* Extract an address from incoming parameters to derive from name or address field. */
    uint256_t ExtractToken(const encoding::json& jParams)
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
                throw Exception(-165, "Invalid token");

            return hashRet;
        }

        return 0;
    }


    /* Extract a genesis-id from input parameters which could be either username or genesis keys. */
    uint256_t ExtractGenesis(const encoding::json& jParams)
    {
        /* Check to see if specific genesis has been supplied */
        if(jParams.find("genesis") != jParams.end())
        {
            /* Check for empty parameter. */
            if(jParams["genesis"].empty())
                throw Exception(-58, "Empty Parameter [genesis]");

            return uint256_t(jParams["genesis"].get<std::string>());
        }

        /* Check if username has been supplied instead. */
        if(jParams.find("username") != jParams.end())
        {
            /* Check for empty parameter. */
            if(jParams["username"].empty())
                throw Exception(-58, "Empty Parameter [username]");

            return TAO::Ledger::SignatureChain::Genesis(jParams["username"].get<std::string>().c_str());
        }

        return Commands::Get<Users>()->GetSession(jParams).GetAccount()->Genesis();
    }


    /* Extract a recipient genesis-id from input parameters which could be either username or genesis keys. */
    uint256_t ExtractRecipient(const encoding::json& jParams)
    {
        /* Check to see if specific genesis has been supplied */
        if(jParams.find("recipient") != jParams.end())
        {
            /* Check for invalid types. */
            if(!jParams["recipient"].is_string())
                throw Exception(-57, "Invalid Parameter [recipient]");

            /* Check for empty parameter. */
            if(jParams["recipient"].empty())
                throw Exception(-58, "Empty Parameter [recipient]");

            /* Check for hex encoding. */
            const std::string strRecipient = jParams["recipient"].get<std::string>();
            if(IsHex(strRecipient))
            {
                /* Copy our genesis-id over to make a check. */
                const uint256_t hashGenesis = uint256_t(jParams["recipient"].get<std::string>());

                /* Check that this is a valid genesis-id. */
                if(hashGenesis.GetType() == TAO::Ledger::GENESIS::UserType())
                    return hashGenesis;
            }

            return TAO::Ledger::SignatureChain::Genesis(jParams["recipient"].get<std::string>().c_str());
        }

        throw Exception(-56, "Missing Parameter [recipient]");
    }


    /* Extract an amount value from either string or integer and convert to its final value. */
    uint64_t ExtractAmount(const encoding::json& jParams, const uint64_t nFigures, const std::string& strPrefix)
    {
        /* Cache our name with prefix calculated. */
        const std::string strAmount = (strPrefix.empty() ? ("") : ("_" + strPrefix)) + "amount";

        /* Check for missing parameter. */
        if(jParams.find(strAmount) != jParams.end())
        {
            /* Watch our numeric limits. */
            const uint64_t nLimit = std::numeric_limits<uint64_t>::max();

            /* Catch parsing exceptions. */
            try
            {
                /* Initialize our return value. */
                double dValue = 0;

                /* Convert to value if in string form. */
                if(jParams[strAmount].is_string())
                    dValue = std::stod(jParams[strAmount].get<std::string>());

                /* Grab value regularly if it is integer. */
                else if(jParams[strAmount].is_number_unsigned())
                    dValue = double(jParams[strAmount].get<uint64_t>());

                /* Check for a floating point value. */
                else if(jParams[strAmount].is_number_float())
                    dValue = jParams[strAmount].get<double>();

                /* Otherwise we have an invalid parameter. */
                else
                    throw Exception(-57, "Invalid Parameter [", strAmount, "]");

                /* Check our minimum range. */
                if(dValue <= 0)
                    throw Exception(-68, "[", strAmount, "] too small [", dValue, "]");

                /* Check our limits and ranges now. */
                if(uint64_t(dValue) > (nLimit / nFigures))
                    throw Exception(-60, "[", strAmount, "] out of range [", nLimit, "]");

                /* Final compute of our figures. */
                return uint64_t(dValue * nFigures);
            }
            catch(const encoding::detail::exception& e) { throw Exception(-57, "Invalid Parameter [", strAmount, "]");           }
            catch(const std::invalid_argument& e)       { throw Exception(-57, "Invalid Parameter [", strAmount, "]");           }
            catch(const std::out_of_range& e)           { throw Exception(-60, "[", strAmount, "] out of range [", nLimit, "]"); }
        }

        throw Exception(-56, "Missing Parameter [", strAmount, "]");
    }


    /* Extract an integer value from input parameters in either string or integer format. */
    uint64_t ExtractValue(const encoding::json& jParams, const std::string& strName)
    {
        /* Check for missing parameter. */
        if(jParams.find(strName) != jParams.end())
        {
            /* Watch our numeric limits. */
            const uint64_t nLimit = std::numeric_limits<uint64_t>::max();

            /* Catch parsing exceptions. */
            try
            {
                /* Initialize our return value. */
                uint64_t nValue = 0;

                /* Convert to value if in string form. */
                if(jParams[strName].is_string())
                    nValue = std::stoull(jParams[strName].get<std::string>());

                /* Grab value regularly if it is integer. */
                else if(jParams[strName].is_number_unsigned())
                    nValue = jParams[strName].get<uint64_t>();

                /* Otherwise we have an invalid parameter. */
                else
                    throw Exception(-57, "Invalid Parameter [", strName, "]");

                /* Check our minimum range. */
                if(nValue == 0)
                    throw Exception(-68, "[", strName, "] too small [", nValue, "]");

                /* Final compute of our figures. */
                return nValue;
            }
            catch(const encoding::detail::exception& e) { throw Exception(-57, "Invalid Parameter [", strName, "]");           }
            catch(const std::invalid_argument& e)       { throw Exception(-57, "Invalid Parameter [", strName, "]");           }
            catch(const std::out_of_range& e)           { throw Exception(-60, "[", strName, "] out of range [", nLimit, "]"); }
        }

        throw Exception(-56, "Missing Parameter [", strName, "]");
    }


    /* Extract a verbose argument from input parameters in either string or integer format. */
    uint32_t ExtractVerbose(const encoding::json& jParams, const uint32_t nMinimum)
    {
        /* If verbose not supplied, use default parameter. */
        if(jParams.find("verbose") == jParams.end())
            return nMinimum;

        /* Extract paramter if in integer form. */
        if(jParams["verbose"].is_number_unsigned())
            return std::max(nMinimum, jParams["verbose"].get<uint32_t>());

        /* Extract parameter if in string form. */
        std::string strVerbose = "default";
        if(jParams["verbose"].is_string())
        {
            /* Grab a copy of the string now. */
            strVerbose = jParams["verbose"].get<std::string>();

            /* Check if it is an integer. */
            if(IsAllDigit(strVerbose))
                return std::max(nMinimum, uint32_t(std::stoull(strVerbose)));
        }

        /* Otherwise check our base verbose levels. */
        if(strVerbose == "default")
            return std::max(nMinimum, uint32_t(1));

        /* Summary maps to verbose level 2. */
        if(strVerbose == "summary")
            return std::max(nMinimum, uint32_t(2));

        /* Detail maps to verbose level 3. */
        else if(strVerbose == "detail")
            return std::max(nMinimum, uint32_t(3));

        throw Exception(-57, "Invalid Parameter [verbose]");
    }


    /* Extracts the paramers applicable to a List API call in order to apply a filter/offset/limit to the result */
    void ExtractList(const encoding::json& jParams, std::string &strOrder, uint32_t &nLimit, uint32_t &nOffset)
    {
        /* Check for page parameter. */
        uint32_t nPage = 0;
        if(jParams.find("page") != jParams.end())
            nPage = std::stoul(jParams["page"].get<std::string>());

        /* Check for offset parameter. */
        nOffset = 0;
        if(jParams.find("offset") != jParams.end())
            nOffset = std::stoul(jParams["offset"].get<std::string>());

        /* Check for limit and offset parameter. */
        nLimit = 100;
        if(jParams.find("limit") != jParams.end())
        {
            /* Grab our limit from parameters. */
            const std::string strLimit = jParams["limit"].get<std::string>();

            /* Check to see whether the limit includes an offset comma separated */
            if(IsAllDigit(strLimit))
            {
                /* No offset included in the limit */
                nLimit = std::stoul(strLimit);
            }
            else if(strLimit.find(","))
            {
                /* Parse the limit and offset */
                std::vector<std::string> vParts;
                ParseString(strLimit, ',', vParts);

                /* Check for expected sizes. */
                if(vParts.size() < 2)
                    throw Exception(-57, "Invalid Parameter [limit] [", strLimit, "]");

                /* Get the limit */
                nLimit = std::stoul(trim(vParts[0]));

                /* Get the offset */
                nOffset = std::stoul(trim(vParts[1]));
            }
            else
            {
                /* Invalid limit */
            }
        }

        /* If no offset explicitly included calculate it from the limit + page */
        if(nOffset == 0 && nPage > 0)
            nOffset = nLimit * nPage;

        /* Get sort order*/
        if(jParams.find("order") != jParams.end())
            strOrder = jParams["order"].get<std::string>();
    }

} // End TAO namespace
