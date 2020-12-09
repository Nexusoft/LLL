/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Common/include/random.h>
#include <LLC/hash/SK.h>

#include <LLD/include/global.h>

#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>

#include <TAO/Register/include/enum.h>

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
        json::json DEX::Buy(const json::json& params, bool fHelp)
        {
            json::json ret;

            /* Authenticate the users credentials */
            if(!users->Authenticate(params))
                throw APIException(-139, "Invalid credentials");

            /* Get the PIN to be used for this API call */
            SecureString strPIN = users->GetPin(params, TAO::Ledger::PinUnlock::TRANSACTIONS);

            /* Get the session to be used for this API call */
            Session& session = users->GetSession(params);

            /* Check for txid parameter. */
            if(params.find("txid") == params.end())
                throw APIException(-50, "Missing txid.");

            /* Check for from parameter. */
            TAO::Register::Address hashFrom;
            if(params.find("name_from") != params.end() && !params["name_from"].get<std::string>().empty())
                hashFrom = Names::ResolveAddress(params, params["name_from"].get<std::string>());
            else if(params.find("address_from") != params.end())
                hashFrom.SetBase58(params["address_from"].get<std::string>());
            else
                throw APIException(-39, "Missing name_from / address_from");

            /* Lock the signature chain. */
            LOCK(session.CREATE_MUTEX);

            /* Create the transaction. */
            TAO::Ledger::Transaction tx;
            if(!Users::CreateTransaction(session.GetAccount(), strPIN, tx))
                throw APIException(-17, "Failed to create transaction");

            /* Get the transaction id. */
            uint512_t hashTx;
            hashTx.SetHex(params["txid"].get<std::string>());

            /* Read the previous transaction. */
            TAO::Ledger::Transaction txPrev;
            if(!LLD::Ledger->ReadTx(hashTx, txPrev))
                throw APIException(-40, "Previous transaction not found.");

            /* Loop through all transactions. */
            bool fFound = false;
            for(uint32_t nContract = 0; nContract < txPrev.Size(); ++nContract)
            {
                /* Get the contract. */
                const TAO::Operation::Contract& contract = txPrev[nContract];

                /* Get the operation byte. */
                uint8_t nType = 0;
                contract >> nType;

                /* Switch based on type. */
                switch(nType)
                {
                    /* Check that transaction has a condition. */
                    case TAO::Operation::OP::CONDITION:
                    {
                        /* Get the next OP. */
                        contract.Seek(4, TAO::Operation::Contract::CONDITIONS);

                        /* Get the comparison bytes. */
                        std::vector<uint8_t> vBytes;
                        contract >= vBytes;

                        /* Extract the data from the bytes. */
                        TAO::Operation::Stream ssCompare(vBytes);
                        ssCompare.seek(33);

                        /* Get the address to. */
                        uint256_t hashTo;
                        ssCompare >> hashTo;

                        /* Get the amount requested. */
                        uint64_t nPrice = 0;
                        ssCompare >> nPrice;

                        /* Build the transaction. */
                        tx[0] << uint8_t(TAO::Operation::OP::VALIDATE) << hashTx << uint32_t(0);
                        tx[0] << uint8_t(TAO::Operation::OP::DEBIT) << hashFrom << hashTo << nPrice << uint64_t(0);

                        fFound = true;

                        break;
                    }

                    default:
                        throw APIException(-42, "No valid debits to buy from");
                }
            }

            /* Check that output was found. */
            if(!fFound)
                throw APIException(-43, "No valid contracts in debit tx.");

            /* Add the fee */
            AddFee(tx);

            /* Execute the operations layer. */
            if(!tx.Build())
                throw APIException(-44, "Transaction failed to build");

            /* Sign the transaction. */
            if(!tx.Sign(session.GetAccount()->Generate(tx.nSequence, strPIN)))
                throw APIException(-31, "Ledger failed to sign transaction.");

            /* Execute the operations layer. */
            if(!TAO::Ledger::mempool.Accept(tx))
                throw APIException(-32, "Failed to accept.");

            /* Build a JSON response object. */
            ret["txid"]  = tx.GetHash().ToString();

            return ret;
        }
    }
}
