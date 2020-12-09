/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Common/include/random.h>
#include <LLC/hash/SK.h>

#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/types/object.h>
#include <TAO/Register/include/create.h>
#include <TAO/Register/include/names.h>
#include <TAO/Register/include/reserved.h>
#include <TAO/Register/types/object.h>

#include <TAO/Ledger/include/create.h>
#include <TAO/Ledger/types/mempool.h>
#include <TAO/Ledger/types/sigchain.h>

#include <LLD/include/global.h>

#include <Util/templates/datastream.h>

#include <Util/include/convert.h>
#include <Util/include/base64.h>

/* Global TAO namespace. */
namespace TAO
{
    /* API Layer namespace. */
    namespace API
    {
        /* Create an name . */
        json::json Names::Create(const json::json& params, bool fHelp)
        {
            /* Return JSON object */
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

            /* Check caller has provided the name parameter */
            if(params.find("name") == params.end())
                throw APIException(-88, "Missing name.");

            /* The name */
            std::string strName = "";
            
            /* The namespace to create the name in */
            std::string strNamespace = "";

            /* Flag to indicate that this should be a global name */
            bool fGlobal = false;

            /* The register address to create the name for */
            TAO::Register::Address hashRegister ;

            /* Get the name from the params */
            strName = params["name"].get<std::string>();

            /* Check caller has provided the register address parameter */
            if(params.find("register_address") != params.end() )
            {
                /* Check that the register address is a valid address */
                if(!IsRegisterAddress(params["register_address"].get<std::string>()))
                    throw APIException(-89, "Invalid register_address");


                hashRegister.SetBase58(params["register_address"].get<std::string>());
            }

            /* Check to see caller has provided the namespace parameter */
            if(params.find("namespace") != params.end())
                strNamespace = params["namespace"].get<std::string>();
            
            /* Check to see caller has provided the global flag parameter */
            if(params.find("global") != params.end())
                fGlobal = params["global"].get<std::string>() == "1" || params["global"].get<std::string>() == "true";

            /* If the caller has specified the fGlobal flag then the namespace must have been specified as blank */
            if(fGlobal && strNamespace.length() > 0)
                throw APIException(-170, "Global names cannot be created in a namespace");

            /* Check for reserved global names. */
            if(fGlobal && TAO::Register::NAME::Reserved(strName) )
                throw APIException(-201, "Global names can't be created with reserved name");

            /* If the caller has specified the global flag then set the namespace to the reserved global namespace name */
            if(fGlobal)
                strNamespace = TAO::Register::NAMESPACE::GLOBAL;


            /* Create the Name object contract */
            tx[0] = Names::CreateName(session.GetAccount()->Genesis(), strName, strNamespace, hashRegister);

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

            /* The address for the name register is derived from the name / namespace / genesis.  To save working all this out again
               we can just deserialize it from the contract created by Name::CreateName  */
            tx[0].Reset();
            uint8_t OPERATION = 0;
            tx[0] >> OPERATION;
            TAO::Register::Address hashAddress;
            tx[0] >> hashAddress;

            ret["address"] = hashAddress.ToString();

            return ret;
        }


        /* Create an namespace . */
        json::json Names::CreateNamespace(const json::json& params, bool fHelp)
        {
            /* Return JSON object */
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

            /* Check caller has provided the name parameter */
            if(params.find("name") == params.end())
                throw APIException(-88, "Missing name");

            /* Get the namespace name */
            std::string strNamespace = params["name"].get<std::string>();

            /* Check namespace for case/allowed characters */
            if (!std::all_of(strNamespace.cbegin(), strNamespace.cend(), 
                [](char c)
                { 
                    /* Check for lower case or numeric or allowed characters */
                    return std::islower(c) || std::isdigit(c) || c == '.'; 
                }
                )) 
            {
                throw APIException(-162, "Namespace can only contain lowercase letters, numbers, periods (.)");
            }

            /* Check for reserved names. */
            if(TAO::Register::NAMESPACE::Reserved(strNamespace) )
                throw APIException(-200, "Namespaces can't contain reserved names");


            /* Generate register address for namespace, which must be a hash of the name */
            TAO::Register::Address hashRegister = TAO::Register::Address(strNamespace, TAO::Register::Address::NAMESPACE);

            /* check that the namespace object doesn't already exist*/
            TAO::Register::Object namespaceObject;
            
            /* Read the Name Object */
            if(LLD::Register->ReadState(hashRegister, namespaceObject, TAO::Ledger::FLAGS::MEMPOOL))
                throw APIException(-90, "Namespace already exists");

            /* Create the new namespace object */
            namespaceObject = TAO::Register::CreateNamespace(strNamespace);

            /* Submit the payload object. */
            tx[0] << uint8_t(TAO::Operation::OP::CREATE) << hashRegister << uint8_t(TAO::Register::REGISTER::OBJECT) << namespaceObject.GetState();
            
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
