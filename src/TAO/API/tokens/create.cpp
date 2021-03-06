/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <TAO/Register/types/address.h>
#include <LLC/hash/SK.h>

#include <LLD/include/global.h>

#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/include/create.h>
#include <TAO/Register/types/object.h>

#include <TAO/Ledger/include/create.h>
#include <TAO/Ledger/types/mempool.h>
#include <TAO/Ledger/types/sigchain.h>

#include <Util/templates/datastream.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Create an asset or digital item. */
        json::json Tokens::Create(const json::json& params, bool fHelp)
        {
            json::json ret;

            /* Authenticate the users credentials */
            if(!users->Authenticate(params))
                throw APIException(-139, "Invalid credentials");

            /* Get the PIN to be used for this API call */
            SecureString strPIN = users->GetPin(params, TAO::Ledger::PinUnlock::TRANSACTIONS);

            /* Get the session to be used for this API call */
            Session& session = users->GetSession(params);

            /* Lock the signature chain. */
            LOCK(session.CREATE_MUTEX);

            /* Create the transaction. */
            TAO::Ledger::Transaction tx;
            if(!Users::CreateTransaction(session.GetAccount(), strPIN, tx))
                throw APIException(-17, "Failed to create transaction");

            /* Generate a random hash for this objects register address */
            TAO::Register::Address hashRegister ;

            /* name of the object, default to blank */
            std::string strName = "";

            
            /* Create the proper register format. */
            hashRegister = TAO::Register::Address(TAO::Register::Address::TOKEN);

            /* Check for supply parameter. */
            if(params.find("supply") == params.end())
                throw APIException(-119, "Missing supply");

            /* Extract the supply parameter */
            uint64_t nSupply = 0;
            if(params.find("supply") != params.end())
            {
                /* Attempt to convert the supplied value to a 64-bit unsigned integer, catching argument/range exceptions */
                try
                {
                    nSupply = std::stoull(params["supply"].get<std::string>());
                }
                catch(const std::invalid_argument& e)
                {
                    throw APIException(-175, "Invalid supply amount.  Supply must be whole number value");
                }
                catch(const std::out_of_range& e)
                {
                    throw APIException(-176, "Invalid supply amount.  The maximum token supply is 18446744073709551615");
                }

            }

            /* For tokens being created without a global namespaced name, the identifier is equal to the register address */
            TAO::Register::Address hashIdentifier = hashRegister;

            /* Check for nDecimals parameter. */
            uint8_t nDecimals = 0;
            if(params.find("decimals") != params.end())
            {
                bool fValid = false;
                /* Attempt to convert the supplied value to a 8-bit unsigned integer, catching argument/range exceptions */
                try
                {
                    nDecimals = std::stoul(params["decimals"].get<std::string>());
                    fValid = nDecimals <= 8;
                }
                catch(const std::invalid_argument& e)
                {
                    fValid = false;
                }
                catch(const std::out_of_range& e)
                {
                    fValid = false;
                }

                if(!fValid)
                    throw APIException(-177, "Invalid decimals amount.  Decimals must be whole number value between 0 and 8");
                
            }

            /* Sanitize the supply/decimals combination for uint64 overflow */
            if(nDecimals > 0 && nSupply > std::numeric_limits<uint64_t>::max() / pow(10, nDecimals))
                throw APIException(-178, "Invalid supply / decimals.  The maximum combination of supply and decimals (supply * 10^decimals) cannot exceed 18446744073709551615");

            /* Multiply the supply by 10^Decimals to give the supply in the divisible units */
            nSupply = nSupply * pow(10, nDecimals);
                
            /* Create a token object register. */
            TAO::Register::Object token = TAO::Register::CreateToken(hashIdentifier,
                                                                        nSupply,
                                                                        nDecimals);

            /* Submit the payload object. */
            tx[0] << uint8_t(TAO::Operation::OP::CREATE) << hashRegister << uint8_t(TAO::Register::REGISTER::OBJECT) << token.GetState();


            /* Check for name parameter. If one is supplied then we need to create a Name Object register for it. */
            if(params.find("name") != params.end() && !params["name"].is_null() && !params["name"].get<std::string>().empty())
                tx[1] = Names::CreateName(session.GetAccount()->Genesis(), params["name"].get<std::string>(), "", hashRegister);

            /* Add the fee */
            AddFee(tx);

            /* Execute the operations layer. */
            if(!tx.Build())
                throw APIException(-30, "Operations failed to execute");

            /* Sign the transaction. */
            if(!tx.Sign(session.GetAccount()->Generate(tx.nSequence, strPIN)))
                throw APIException(-31, "Ledger failed to sign transaction");

            /* Execute the operations layer. */
            if(!TAO::Ledger::mempool.Accept(tx))
                throw APIException(-32, "Failed to accept");

            /* Build a JSON response object. */
            ret["txid"]  = tx.GetHash().ToString();
            ret["address"] = hashRegister.ToString();

            return ret;
        }
    }
}
