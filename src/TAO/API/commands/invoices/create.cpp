/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <TAO/Register/types/address.h>

#include <LLD/include/global.h>

#include <LLP/include/global.h>

#include <TAO/API/include/global.h>
#include <TAO/API/include/constants.h>

#include <TAO/API/include/build.h>
#include <TAO/API/include/get.h>
#include <TAO/API/types/commands.h>

#include <TAO/API/users/types/users.h>
#include <TAO/API/names/types/names.h>
#include <TAO/API/types/commands/invoices.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>

#include <TAO/Register/include/verify.h>
#include <TAO/Register/include/constants.h>
#include <TAO/Register/include/enum.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/create.h>
#include <TAO/Ledger/include/timelocks.h>
#include <TAO/Ledger/types/mempool.h>
#include <TAO/Ledger/types/sigchain.h>


/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Creates a new invoice. */
        encoding::json Invoices::Create(const encoding::json& params, const bool fHelp)
        {
            /* First ensure that transaction version 2 active, as the conditions required for invoices were not enabled until v2 */
            const uint32_t nCurrent = TAO::Ledger::CurrentTransactionVersion();
            if(nCurrent < 2 || (nCurrent == 2 && !TAO::Ledger::TransactionVersionActive(runtime::unifiedtimestamp(), 2)))
                throw Exception(-254, "Invoices API not yet active.");

            /* The response JSON */
            encoding::json ret;

            /* Authenticate the users credentials */
            if(!Commands::Get<Users>()->Authenticate(params))
                throw Exception(-139, "Invalid credentials");

            /* The JSON representation of the invoice that we store in the register */
            encoding::json invoice;

            /* The genesis hash of the recipient */
            uint256_t hashRecipient = 0;

            /* The account to invoice must be paid to */
            TAO::Register::Address hashAccount;

            /* The invoice total in fractional tokens */
            double dTotal = 0;

            /* Get the PIN to be used for this API call */
            SecureString strPIN = Commands::Get<Users>()->GetPin(params, TAO::Ledger::PinUnlock::TRANSACTIONS);

            /* Get the session to be used for this API call */
            Session& session = Commands::Get<Users>()->GetSession(params);

            /* Check whether the caller has provided the account name parameter. */
            if(params.find("account_name") != params.end() && !params["account_name"].get<std::string>().empty())
                hashAccount = Names::ResolveAddress(params, params["account_name"].get<std::string>());

            /* Otherwise try to find the raw hex encoded address. */
            else if(params.find("account") != params.end())
                hashAccount.SetBase58(params["account"].get<std::string>());

            /* Fail if no required parameters supplied. */
            else
                throw Exception(-227, "Missing payment account name / address");

            /* Validate the payment account */
            /* Get the account object. */
            TAO::Register::Object account;
            if(!LLD::Register->ReadState(hashAccount, account))
                throw Exception(-13, "Object not found");

            /* Parse the account object register. */
            if(!account.Parse())
                throw Exception(-14, "Object failed to parse");

            /* Check the object standard. */
            if(account.Base() != TAO::Register::OBJECTS::ACCOUNT)
                throw Exception(-65, "Object is not an account");

            /* The token that this invoice should be transacted in */
            TAO::Register::Address hashToken = account.get<uint256_t>("token");

            /* The decimals for this token type */
            uint8_t nDecimals = GetDecimals(account);

            /* Check for recipient by username. */
            if(params.find("recipient_username") != params.end() && !params["recipient_username"].get<std::string>().empty())
                hashRecipient = TAO::Ledger::SignatureChain::Genesis(params["recipient_username"].get<std::string>().c_str());

            /* Check for recipient by genesis-id. */
            else if(params.find("recipient") != params.end())
                hashRecipient.SetHex(params["recipient"].get<std::string>());

            else
                throw Exception(-229, "Missing recipient");

            /* If in client mode and the recipient genesis does not exist in our DB we should ask a peer for the genesis */
            if(config::fClient.load() && !LLD::Ledger->HasGenesis(hashRecipient))
            {
                 /* Check tritium server enabled. */
                if(LLP::TRITIUM_SERVER)
                {
                    std::shared_ptr<LLP::TritiumNode> pNode = LLP::TRITIUM_SERVER->GetConnection();
                    if(pNode != nullptr)
                    {
                        /* Request the genesis hash from the peer. */
                        debug::log(1, FUNCTION, "CLIENT MODE: Requesting GET::GENESIS for ", hashRecipient.SubString());

                        LLP::TritiumNode::BlockingMessage(10000, pNode.get(), LLP::TritiumNode::ACTION::GET, uint8_t(LLP::TritiumNode::TYPES::GENESIS), hashRecipient);

                        debug::log(1, FUNCTION, "CLIENT MODE: GET::GENESIS received for ", hashRecipient.SubString());
                    }
                }

            }

            /* Check that the recipient genesis hash exists */
            if(!LLD::Ledger->HasGenesis(hashRecipient))
                throw Exception(-230, "Recipient user does not exist");

            /* Check that the recipient isn't the sender */
            if(hashRecipient == session.GetAccount()->Genesis())
                throw Exception(-244, "Cannot send invoice to self");

            /* Add the mandatroy invoice fields to the invoice JSON */
            invoice["account"]   = hashAccount.ToString();
            invoice["recipient"] = hashRecipient.ToString();

            /* Add all other non-mandatory fields that the caller has provided */
            for(auto it = params.begin(); it != params.end(); ++it)
            {
                /* Skip any incoming parameters that are keywords used by this API method*/
                    if(it.key() == "pin"
                    || it.key() == "PIN"
                    || it.key() == "session"
                    || it.key() == "name"
                    || it.key() == "account"
                    || it.key() == "account_name"
                    || it.key() == "recipient"
                    || it.key() == "recipient_username"
                    || it.key() == "amount"
                    || it.key() == "token"
                    || it.key() == "items")
                    {
                        continue;
                    }

                    /* add the field to the invoice */
                    invoice[it.key()] = it.value();
            }

            /* Parse the invoice items details */
            if(params.find("items") == params.end() || !params["items"].is_array())
                throw Exception(-232, "Missing items");

            /* Check items is not empty */
            encoding::json items = params["items"];
            if(items.empty())
                throw Exception(-233, "Invoice must include at least one item");

            /* Iterate the items to validate them*/
            for(auto it = items.begin(); it != items.end(); ++it)
            {
                /* check that mandatory fields have been provided */
                if(it->find("unit_amount") == it->end())
                    throw Exception(-235, "Missing item unit amount.");

                if(it->find("units") == it->end())
                    throw Exception(-238, "Missing item number of units.");

                /* Parse the values out of the definition json*/
                std::string strUnitAmount =  (*it)["unit_amount"].get<std::string>();
                std::string strUnits =  (*it)["units"].get<std::string>();

                /* The item Unit Amount */
                double dUnitAmount = 0;

                /* The item number of units */
                uint64_t nUnits = 0;

                /* Attempt to convert the supplied value to a double, catching argument/range exceptions */
                try
                {
                    dUnitAmount = std::stod(strUnitAmount);
                }
                catch(const std::exception& e)
                {
                    throw Exception(-236, "Invalid item unit amount.");
                }

                if(dUnitAmount == 0)
                    throw Exception(-237, "Item unit amount must be greater than 0.");

                /* Attempt to convert the supplied value to a 64-bit unsigned integer, catching argument/range exceptions */
                try
                {
                    nUnits = std::stoull(strUnits);
                }
                catch(const std::exception& e)
                {
                    throw Exception(-239, "Invalid item number of units.");
                }

                if(nUnits == 0)
                    throw Exception(-240, "Item units must be greater than 0.");

                /* work out the total for this item */
                double dItemTotal = dUnitAmount * nUnits;

                /* add this to the invoice total */
                dTotal += dItemTotal;

                /* Once we have validated the mandatory fields, add this item to the invoice JSON */
                invoice["items"].push_back(*it);
            }

            /* Add the invoice amount and token */
            invoice["amount"] = dTotal;
            invoice["token"] = hashToken.ToString();

            /* Once validated, add the items to the invoice */
            invoice["items"] = items;

            /* Calculate the amount to pay in token units */
            uint64_t nTotal = dTotal * pow(10, nDecimals);


            /* Lock the signature chain. */
            LOCK(session.CREATE_MUTEX);

            /* Create the transaction. */
            TAO::Ledger::Transaction tx;
            if(!Users::CreateTransaction(session.GetAccount(), strPIN, tx))
                throw Exception(-17, "Failed to create transaction");

            /* Generate a random hash for this objects register address */
            TAO::Register::Address hashRegister = TAO::Register::Address(TAO::Register::Address::READONLY);

            /* DataStream to help us serialize the data. */
            DataStream ssData(SER_REGISTER, 1);

            /* First add the leading 2 bytes to identify the state data */
            ssData << (uint16_t) USER_TYPES::INVOICE;
            ssData << invoice.dump();

            /* Check the data size */
            if(ssData.size() > TAO::Register::MAX_REGISTER_SIZE)
                throw Exception(-242, "Data exceeds max register size");

            /* Add the invoice creation contract. */
            uint32_t nContract = 0;
            tx[nContract++] << (uint8_t)TAO::Operation::OP::CREATE << hashRegister << (uint8_t)TAO::Register::REGISTER::READONLY << ssData.Bytes();

            /* Check for name parameter. If one is supplied then we need to create a Name Object register for it. */
            if(params.find("name") != params.end() && !params["name"].is_null() && !params["name"].get<std::string>().empty())
                tx[nContract++] = Names::CreateName(session.GetAccount()->Genesis(), params["name"].get<std::string>(), "", hashRegister);

            /* Add the transfer contract */
            tx[nContract] << uint8_t(TAO::Operation::OP::CONDITION) << (uint8_t)TAO::Operation::OP::TRANSFER << hashRegister << hashRecipient << uint8_t(TAO::Operation::TRANSFER::CLAIM);

            /* Add the payment conditions.  The condition is essentially that the claim must include a conditional debit for the
               invoice total being made to the payment account */
            TAO::Operation::Stream ssCompare;
            ssCompare << uint8_t(TAO::Operation::OP::DEBIT) << uint256_t(0) << hashAccount << nTotal << uint64_t(0);

            /* The asset transfer condition requiring pay from specified token and value. */
            tx[nContract] <= uint8_t(TAO::Operation::OP::GROUP);
            tx[nContract] <= uint8_t(TAO::Operation::OP::CALLER::OPERATIONS) <= uint8_t(TAO::Operation::OP::CONTAINS);
            tx[nContract] <= uint8_t(TAO::Operation::OP::TYPES::BYTES) <= ssCompare.Bytes();
            tx[nContract] <= uint8_t(TAO::Operation::OP::AND);
            tx[nContract] <= uint8_t(TAO::Operation::OP::CALLER::PRESTATE::VALUE) <= std::string("token");
            tx[nContract] <= uint8_t(TAO::Operation::OP::EQUALS);
            tx[nContract] <= uint8_t(TAO::Operation::OP::TYPES::UINT256_T) <= hashToken; // ensure the token paid matches the invoice
            tx[nContract] <= uint8_t(TAO::Operation::OP::UNGROUP);

            tx[nContract] <= uint8_t(TAO::Operation::OP::OR);

            /* The clawback clause that allows sender to cancel invoice. */
            tx[nContract] <= uint8_t(TAO::Operation::OP::GROUP);
            tx[nContract] <= uint8_t(TAO::Operation::OP::CALLER::GENESIS);
            tx[nContract] <= uint8_t(TAO::Operation::OP::EQUALS);
            tx[nContract] <= uint8_t(TAO::Operation::OP::CONTRACT::GENESIS);
            tx[nContract] <= uint8_t(TAO::Operation::OP::UNGROUP);

            /* Add the fee */
            AddFee(tx);

            /* Execute the operations layer. */
            if(!tx.Build())
                throw Exception(-30, "Operations failed to execute");

            /* Sign the transaction. */
            if(!tx.Sign(session.GetAccount()->Generate(tx.nSequence, strPIN)))
                throw Exception(-31, "Ledger failed to sign transaction");

            /* Execute the operations layer. */
            if(!TAO::Ledger::mempool.Accept(tx))
                throw Exception(-32, "Failed to accept");

            /* Build a JSON response object. */
            ret["txid"]  = tx.GetHash().ToString();
            ret["address"] = hashRegister.ToString();

            return ret;
        }
    }
}