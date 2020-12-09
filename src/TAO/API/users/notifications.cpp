/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <tuple>

#include <LLD/include/global.h>

#include <LLP/include/global.h>

#include <TAO/API/users/types/users.h>
#include <TAO/API/include/conditions.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/utils.h>
#include <TAO/API/include/json.h>
#include <TAO/API/types/sessionmanager.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/include/create.h>

#include <TAO/Ledger/types/transaction.h>
#include <TAO/Ledger/types/mempool.h>
#include <TAO/Ledger/types/sigchain.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>
#include <TAO/Operation/types/condition.h>

#include <TAO/Register/include/constants.h>
#include <TAO/Register/include/unpack.h>
#include <TAO/Register/include/build.h>
#include <TAO/Register/include/names.h>
#include <TAO/Register/types/object.h>

#include <Legacy/include/trust.h>
#include <Legacy/types/trustkey.h>

#include <Util/include/hex.h>
#include <Util/include/debug.h>

#include <Legacy/include/evaluate.h>

#include <queue>
#include <unordered_set>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {
        /* Helper struct to allow a std::pair to be used as the type in an unordered_set */
        struct pair_hash
        {
            template <class T1, class T2>
            std::size_t operator () (std::pair<T1, T2> const &pair) const
            {
                std::size_t h1 = std::hash<T1>()(pair.first);
                std::size_t h2 = std::hash<T2>()(pair.second);

                return h1 ^ h2;
            }
        };

        /*  Gets the currently outstanding contracts that have not been matched with a credit or claim. */
        bool Users::GetOutstanding(const uint256_t& hashGenesis,
                const bool& fIncludeSuppressed,
                std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Get the last transaction in the sig chain from disk. */
            uint512_t hashLast = 0;
            if(LLD::Ledger->ReadLast(hashGenesis, hashLast))
            {
                /* Get the coinbase transactions. */
                get_coinbases(hashGenesis, hashLast, vContracts);

                /* Get split dividend payments to assets tokenized by tokens we hold */
                get_tokenized_debits(hashGenesis, vContracts);

                /* Get the debit and transfer events. */
                get_events(hashGenesis, vContracts);

            }

            /* Remove any suppressed if flagged to do so */
            if(!fIncludeSuppressed)
            {
                vContracts.erase
                (
                    /* Use std remove_if function to return the iterator to erase. This allows us to pass in a lambda function,
                    which itself can check to see if a notification suppression exists and, if so, whether it has expired */
                    std::remove_if(vContracts.begin(), vContracts.end(), 
                    [](const std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>& entry)
                    {
                        uint64_t nTimestamp;
                        if(!LLD::Local->ReadSuppressNotification(std::get<0>(entry).Hash(), std::get<1>(entry), nTimestamp))
                            return false;
                        else
                            return nTimestamp > runtime::unifiedtimestamp(); // remove notification if timeout not expired
                    }), 
                    vContracts.end()
                );
            }

            /* Sort transactions by timestamp from oldest to newest. */
            std::sort(vContracts.begin(), vContracts.end(),
                [](const std::tuple<TAO::Operation::Contract, uint32_t, uint256_t> &a,
                   const std::tuple<TAO::Operation::Contract, uint32_t, uint256_t> &b)
                {
                    return ( std::get<0>(a).Timestamp() < std::get<0>(b).Timestamp() );
                });

            return true;
        }

        /*  Gets the any debit or transfer transactions that have expired and can be voided. */
        bool Users::GetExpired(const uint256_t& hashGenesis,
                const bool& fIncludeSuppressed,
                std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Get the last transaction in the sig chain from disk. */
            uint512_t hashLast = 0;
            if(LLD::Ledger->ReadLast(hashGenesis, hashLast))
            {
                /* Get any expired debit and transfer transactions we made */
               get_expired(hashGenesis, hashLast, vContracts);
            }

            /* Remove any suppressed if flagged to do so */
            if(!fIncludeSuppressed)
            {
                vContracts.erase
                (
                    /* Use std remove_if function to return the iterator to erase. This allows us to pass in a lambda function,
                    which itself can check to see if a notification suppression exists and, if so, whether it has expired */
                    std::remove_if(vContracts.begin(), vContracts.end(), 
                    [](const std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>& entry)
                    {
                        uint64_t nTimestamp;
                        if(!LLD::Local->ReadSuppressNotification(std::get<0>(entry).Hash(), std::get<1>(entry), nTimestamp))
                            return false;
                        else
                            return nTimestamp > runtime::unifiedtimestamp(); // remove notification if timeout not expired
                    }), 
                    vContracts.end()
                );
            }

            return true;
        }


        /*  Gets the currently outstanding legacy UTXO to register transactions that have not been matched with a credit */
        bool Users::GetOutstanding(const uint256_t& hashGenesis,
                const bool& fIncludeSuppressed,
                std::vector<std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t>> &vContracts)
        {
            get_events(hashGenesis, vContracts);

            /* Remove any suppressed if flagged to do so */
            if(!fIncludeSuppressed)
            {
                vContracts.erase
                (
                    /* Use std remove_if function to return the iterator to erase. This allows us to pass in a lambda function,
                    which itself can check to see if a notification suppression exists and, if so, whether it has expired */
                    std::remove_if(vContracts.begin(), vContracts.end(), 
                    [](const std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t>& entry)
                    {
                        uint64_t nTimestamp;
                        if(!LLD::Local->ReadSuppressNotification(std::get<0>(entry)->GetHash(), std::get<1>(entry), nTimestamp))
                            return false;
                        else
                            return nTimestamp > runtime::unifiedtimestamp(); // remove notification if timeout not expired
                    }), 
                    vContracts.end()
                );
            }

            /* Sort transactions by timestamp from oldest to newest. */
            std::sort(vContracts.begin(), vContracts.end(),
                [](const std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t> &a,
                const std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t> &b)
                {
                    return ( std::get<0>(a)->nTime < std::get<0>(b)->nTime );
                });

            return true;
        }


        /* Get the outstanding debits and transfer transactions. */
        bool Users::get_events(const uint256_t& hashGenesis,
                std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Get notifications for personal genesis indexes. */
            TAO::Ledger::Transaction tx;

            /* Keep track of unique contracts. */
            std::set<std::pair<uint512_t, uint32_t>> setUnique;

            /* Counter of consecutive processed events. */
            uint32_t nConsecutive = 0;

            /* The event sequence number */
            uint32_t nSequence = 0;

            /* Get the last event */
            LLD::Ledger->ReadSequence(hashGenesis, nSequence);

            /* Decrement the current sequence number to get the last event sequence number */
            --nSequence;

            /* Look back through all events to find those that are not yet processed. */
            while(LLD::Ledger->ReadEvent(hashGenesis, nSequence, tx))
            {
                /* Check to see if we have 100 (or the user configured amount) consecutive processed events.  If we do then we
                   assume all prior events are also processed.  This saves us having to scan the entire chain of events */
                if(nConsecutive >= config::GetArg("-eventsdepth", 100))
                    break;

                /* Check that the transaction is mature */
                if(!LLD::Ledger->ReadMature(tx.GetHash()))
                {
                    /* If not, decrement the sequence id and continue to the next event. */
                    --nSequence;
                    continue;
                }

                /* Loop through transaction contracts. */
                uint32_t nContracts = tx.Size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    /* Reference to contract to check */
                    const TAO::Operation::Contract& contract = tx[nContract];

                    /* The proof to check for this contract */
                    uint256_t hashProof = 0;

                    /* Reset the contract to the position of the primitive. */
                    contract.SeekToPrimitive();

                    /* The operation */
                    uint8_t nOP;
                    contract >> nOP;

                    /* Check for that the debit is meant for us. */
                    switch(nOP)
                    {
                        /* Check for debit events. */
                        case TAO::Operation::OP::DEBIT:
                        {
                            /* Get the source address which is the proof for the debit */
                            contract >> hashProof;

                            /* Get the recipient account */
                            uint256_t hashTo;
                            contract >> hashTo;

                            /* Retrieve the account. */
                            TAO::Register::State state;
                            if(!LLD::Register->ReadState(hashTo, state))
                                continue;

                            /* Check owner that we are the owner of the recipient account  */
                            if(state.hashOwner != hashGenesis)
                                continue;

                            break;
                        }

                        /* Check for transfer events. */
                        case TAO::Operation::OP::TRANSFER:
                        {
                            /* The register address being transferred */
                            uint256_t hashRegister;
                            contract >> hashRegister;

                            /* Get recipient genesis hash */
                            contract >> hashProof;

                            /* Read the force transfer flag */
                            uint8_t nType = 0;
                            contract >> nType;

                            /* Ensure this wasn't a forced transfer (which requires no Claim) */
                            if(nType == TAO::Operation::TRANSFER::FORCE)
                                continue;

                            /* Check that we are the recipient */
                            if(hashGenesis != hashProof)
                                continue;

                            /* Check that the sender has not claimed it back (voided).  We can skip this in client mode and just 
                               rely on whether a proof exists for it instead. */
                            if(!config::fClient.load())
                            {
                                TAO::Register::State state;
                                if(!LLD::Register->ReadState(hashRegister, state))
                                    continue;

                                /* Make sure the register claim is in SYSTEM pending from a transfer.  */
                                if(state.hashOwner.GetType() != TAO::Ledger::GENESIS::SYSTEM)
                                    continue;
                            }

                            /* Make sure we haven't already claimed it */
                            if(LLD::Ledger->HasProof(hashRegister, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                            {
                                nConsecutive++;
                                continue;
                            }

                            break;
                        }

                        /* Check for coinbase events. */
                        case TAO::Operation::OP::COINBASE:
                        {
                            /* Unpack the miners genesis from the contract */
                            contract >> hashProof;

                            /* Check that we mined it */
                            if(hashGenesis != hashProof)
                                continue;

                            break;
                        }

                        /* Default continue. */
                        default:
                            continue;
                    }

                    /* Check to see if we have already credited this debit. */
                    uint512_t hashTx = tx.GetHash();
                    if(LLD::Ledger->HasProof(hashProof, hashTx, nContract, TAO::Ledger::FLAGS::MEMPOOL))
                    {
                        nConsecutive++;
                        continue;
                    }

                    /* Check that we haven't already added this contract to the vContracts list.  Since events are written by 
                       transaction hash only, if two or more contracts exist in the same transaction for the same sig chain, then
                       there will be duplicate events written for the same transaction. */
                    if(setUnique.count(std::make_pair(hashTx, nContract)) == 0)
                    {
                        setUnique.insert(std::make_pair(hashTx, nContract));

                        /* Add the contract to the list. */
                        vContracts.push_back(std::make_tuple(contract, nContract, 0));

                        /* Reset the consecutive counter since this has not been processed */
                        nConsecutive = 0;
                    }

                }

                /* Iterate the sequence id backwards. */
                --nSequence;
            }

            return true;
        }

        /* Get the outstanding legacy UTXO to register transactions. */
        bool Users::get_events(const uint256_t& hashGenesis,
                std::vector<std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t>> &vContracts)
        {
            /* Check the database instance. */
            if(!LLD::Legacy)
                return false;
                
            /* Get notifications for personal genesis indexes. */
            Legacy::Transaction tx;

            /* Keep track of unique contracts. */
            std::set<std::pair<uint512_t, uint32_t>> setUnique;

            /* Counter of consecutive processed events. */
            uint32_t nConsecutive = 0;

            /* The event sequence number */
            uint32_t nSequence = 0;

            /* Get the last event */
            LLD::Legacy->ReadSequence(hashGenesis, nSequence);

            /* Decrement the current sequence number to get the last event sequence number */
            --nSequence;

            /* Look back through all events to find those that are not yet processed. */
            while(LLD::Legacy->ReadEvent(hashGenesis, nSequence, tx))
            {
                /* Check to see if we have 100 (or the user configured amount) consecutive processed events.  If we do then we
                   assume all prior events are also processed.  This saves us having to scan the entire chain of events */
                if(nConsecutive >= config::GetArg("-eventsdepth", 100))
                    break;

                /* Make a shared pointer to the transaction so that we can keep it alive until the caller
                   is done processing the contracts */
                std::shared_ptr<Legacy::Transaction> ptx(new Legacy::Transaction(tx));

                /* Loop through transaction outputs. */
                uint32_t nContracts = ptx->vout.size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    /* check the script to see if it contains a register address */
                    TAO::Register::Address hashTo;
                    if(!Legacy::ExtractRegister(ptx->vout[nContract].scriptPubKey, hashTo))
                        continue;

                    /* Read the hashTo account */
                    TAO::Register::State state;
                    if(!LLD::Register->ReadState(hashTo, state))
                        continue;

                    /* Check owner. */
                    if(state.hashOwner != hashGenesis)
                        continue;

                    /* Check if proofs are spent. NOTE the proof is the wildcard address since this is a legacy transaction*/
                    if(LLD::Ledger->HasProof(TAO::Register::WILDCARD_ADDRESS, ptx->GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                    {
                        nConsecutive++;
                        continue;
                    }

                    /* Check that we haven't already added this contract to the vContracts list.  Since events are written by 
                       transaction hash only, if two or more contracts exist in the same transaction for the same sig chain, then
                       there will be duplicate events written for the same transaction. */
                    uint512_t hashTx = tx.GetHash();
                    if(setUnique.count(std::make_pair(hashTx, nContract)) == 0)
                    {
                        setUnique.insert(std::make_pair(hashTx, nContract));

                        /* Add the contract to the list. */
                        vContracts.push_back(std::make_pair(ptx, nContract));

                        /* Reset the consecutive counter since this has not been processed */
                        nConsecutive = 0;
                    }
                }

                /* Iterate the sequence id backwards. */
                --nSequence;
            }

            return (vContracts.size() > 0);
        }


        /* Get any unclaimed transfer transactions. */
        bool Users::get_unclaimed(const uint256_t& hashGenesis,
                std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Get notifications for personal genesis indexes. */
            TAO::Ledger::Transaction tx;

            /* Read back all the events. */
            uint32_t nSequence = 0;
            while(LLD::Ledger->ReadEvent(hashGenesis, nSequence, tx))
            {
                /* Loop through transaction contracts. */
                uint32_t nContracts = tx.Size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    /* Reference to contract to check */
                    const TAO::Operation::Contract& contract = tx[nContract];

                    /* The proof to check for this contract */
                    uint256_t hashProof = 0;

                    /* Reset the contract to the position of the primitive. */
                    contract.SeekToPrimitive();

                    /* The operation */
                    uint8_t nOP;
                    contract >> nOP;

                    /* Check for that the debit is meant for us. */
                    switch(nOP)
                    {
                        /* Check for transfer events. */
                        case TAO::Operation::OP::TRANSFER:
                        {
                            /* The register address being transferred */
                            uint256_t hashRegister;
                            contract >> hashRegister;

                            /* Get recipient genesis hash */
                            contract >> hashProof;

                            /* Read the force transfer flag */
                            uint8_t nType = 0;
                            contract >> nType;

                            /* Ensure this wasn't a forced transfer (which requires no Claim) */
                            if(nType == TAO::Operation::TRANSFER::FORCE)
                                continue;

                            /* Check that we are the recipient */
                            if(hashGenesis != hashProof)
                                continue;

                            /* Check that the sender has not claimed it back (voided).  We can skip this in client mode and just 
                               rely on whether a proof exists for it instead. */
                            if(!config::fClient.load())
                            {
                                TAO::Register::State state;
                                if(!LLD::Register->ReadState(hashRegister, state))
                                    continue;

                                /* Make sure the register claim is in SYSTEM pending from a transfer.  */
                                if(state.hashOwner.GetType() != TAO::Ledger::GENESIS::SYSTEM)
                                    continue;
                            }

                            /* Make sure we haven't already claimed it */
                            if(LLD::Ledger->HasProof(hashRegister, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                                continue;

                            /* Add the contract and register address to the list */
                            vContracts.push_back(std::make_tuple(contract, nContract, hashRegister));

                            break;
                        }

                        /* Default continue. */
                        default:
                            break;
                    }
                }

                /* Iterate the sequence id forward. */
                ++nSequence;
            }

            return true;
        }


        /*  Get the outstanding coinbases. */
        bool Users::get_coinbases(const uint256_t& hashGenesis,
                uint512_t hashLast, std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Counter of consecutive claimed coinbases.  If this reaches -coinbasedepth then assume there are none older to process */
            uint32_t nConsecutive = 0;

            /* Reverse iterate until genesis (newest to oldest). */
            while(hashLast != 0)
            {
                /* Get the transaction from disk. */
                TAO::Ledger::Transaction tx;
                if(!LLD::Ledger->ReadTx(hashLast, tx, TAO::Ledger::FLAGS::MEMPOOL))
                    return debug::error(FUNCTION, "Failed to read transaction");
                    
                /* Skip this transaction if it not a coinbase or is immature. */
                if(!tx.IsCoinBase() || !LLD::Ledger->ReadMature(tx))
                {
                    /* Set the next last. */
                    hashLast = !tx.IsFirst() ? tx.hashPrevTx : 0;
                    continue;
                }

                /* Loop through all contracts and add coinbase contracts to vector. */
                uint32_t nContracts = tx.Size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    /* Reference to contract to check */
                    const TAO::Operation::Contract& contract = tx[nContract];

                    /* Check for coinbase opcode */
                    if(TAO::Register::Unpack(contract, Operation::OP::COINBASE))
                    {
                        /* Seek past operation. */
                        contract.Seek(1);

                        /* Get the proof to check coinbase. */
                        TAO::Register::Address hashProof;
                        contract >> hashProof;

                        /* Check that the proof is to your genesis. */
                        if(hashProof != hashGenesis)
                            continue;

                        /* Check if proofs are spent. */
                        if(LLD::Ledger->HasProof(hashGenesis, hashLast, nContract, TAO::Ledger::FLAGS::MEMPOOL))
                        {
                            /* Increment the counter of consecutive claims */
                            ++nConsecutive;
                            continue;
                        }

                        /* Reset the counter since this one has not been claimed */
                        nConsecutive = 0;

                        /* Add the coinbase transaction and skip rest of contracts. */
                        vContracts.push_back(std::make_tuple(contract, nContract, 0));
                    }
                }

                /* Check to see if we have reached 10 consecutive claims.  If so then we can assume that all previous coinbases
                    are also claimed and therefore break out early.  This avoids us having to scan the entire sig chain each time
                    which can be very expensive if you have many transactions (such as miners/stakers) */
                if(nConsecutive == config::GetArg("-coinbasedepth", 101))
                    break;

                /* Set the next last. */
                hashLast = !tx.IsFirst() ? tx.hashPrevTx : 0;
            }

            return true;
        }


        /* Get the outstanding debit transactions made to assets owned by tokens you hold. */
        bool Users::get_tokenized_debits(const uint256_t& hashGenesis,
                std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Get the list of registers owned by this sig chain */
            std::vector<TAO::Register::Address> vRegisters;
            if(!ListRegisters(hashGenesis, vRegisters))
                throw APIException(-74, "No registers found");

            /* Iterate registers to find all token accounts. */
            for(const auto& hashRegister : vRegisters)
            {
                /* Initial check that it is a token or account, before we hit the DB to check the token type */
                if(!hashRegister.IsAccount() && !hashRegister.IsToken())
                    continue;

                /* Read the object register. */
                TAO::Register::Object object;
                if(!LLD::Register->ReadState(hashRegister, object, TAO::Ledger::FLAGS::LOOKUP))
                    continue;

                /* Parse the object register. */
                if(!object.Parse())
                    continue;

                /* Check that this is an account */
                if(object.Base() != TAO::Register::OBJECTS::ACCOUNT)
                    continue;

                /* Get the token address */
                TAO::Register::Address hashToken = object.get<uint256_t>("token");

                /* Get the balance  */
                uint64_t nBalance = object.get<uint64_t>("balance");

                /* Check that we have some tokens in the account, otherwise there is nothing else to check for this account */
                if(nBalance == 0)
                    continue;

                /* Read the token register. */
                TAO::Register::Object token;
                if(!LLD::Register->ReadState(hashToken, token, TAO::Ledger::FLAGS::MEMPOOL))
                    continue;

                /* Parse the object register. */
                if(!token.Parse())
                    continue;

                /* The last modified time the balance of this token account changed */
                uint64_t nModified = object.nModified;


                /* Loop through all events for the token (split payments). */
                TAO::Ledger::Transaction tx;

                uint32_t nSequence = 0;

                    /* Get the last event for the token*/
                LLD::Ledger->ReadSequence(hashToken, nSequence);

                /* Decrement the current sequence number to get the last event sequence number */
                --nSequence;
                
                while(LLD::Ledger->ReadEvent(hashToken, nSequence, tx))
                {
                    /* Iterate sequence backwards. */
                    nSequence--;

                    /* We can break out if an event occurred before our token account was last modified, as only
                       the balance at the time of the transaction can be used as proof */
                    if(tx.nTimestamp < nModified)
                        break;

                    /* Loop through transaction contracts. */
                    uint32_t nContracts = tx.Size();
                    for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                    {
                        /* Reference to contract to check */
                        TAO::Operation::Contract& contract = tx[nContract];

                        /* Check that this is a debit contract */
                        if(!TAO::Register::Unpack(contract, Operation::OP::DEBIT))
                            continue;


                        /* Get the token supply so that we an determine our share */
                        uint64_t nSupply = token.get<uint64_t>("supply");

                        /* Get the amount from the debit contract*/
                        uint64_t nAmount = 0;
                        TAO::Register::Unpack(contract, nAmount);

                        /* Calculate the partial debit amount that this token holder is entitled to. */
                        uint64_t nPartial = (nAmount * nBalance) / nSupply;

                        /* If the NXS amount cannot be divided by the percentage of tokens that they own then the nPartial amount
                           will be zero.  In which case we can ignore this notification as there is nothing to credit. */
                        if(nPartial == 0)
                            continue;

                        /* The account/token the debit came from  */
                        TAO::Register::Address hashFrom;
                        TAO::Register::Unpack(contract, hashFrom);

                        /* Check to see if we have already claimed our credit. */
                        if(LLD::Ledger->HasProof(hashRegister, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                            continue;

                        /* Now check to see whether the sender has voided (credited back to themselves) */
                        if(LLD::Ledger->HasProof(hashFrom, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                            continue;

                        /* Add the contract to the return list  . */
                        vContracts.push_back(std::make_tuple(contract, nContract, hashRegister));
                    }
                }
            }

            return true;
        }


        /*  Get any debit or transfer contracts that have expired. */
        bool Users::get_expired(const uint256_t& hashGenesis,
                uint512_t hashLast, std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> &vContracts)
        {
            /* Don't process expired contracts in client mode, as we will not be able to determine whether the recipient has
               already claimed / credited */
            if(config::fClient.load())
                return false;

            /* Cache of contracts by genesis hash for all contracts that we have already determined either do not have 
               any conditions or have already been claimed/credited.  If any contract is already in this vector then we can skip
               it for all future invocations of the get_expired method. */
            static std::unordered_set<std::pair<uint512_t, uint32_t>, pair_hash> cacheProcessed(256);
                
            /* Temporary transaction to use to evaluate the conditions */
            TAO::Ledger::Transaction voidTx;

            /* Set the time and caller on the voidTx to simulate executing it now */
            voidTx.hashGenesis = hashGenesis;

            /* Temporary voiding contract to use to evaluate the conditions */
            TAO::Operation::Contract voidContract;

            /* Bind the temp void contract to the void tx so that the genesis and timestamp are bound */
            voidContract.Bind(&voidTx);

            /* Reverse iterate until genesis (newest to oldest). */
            while(hashLast != 0)
            {
                /* Get the transaction from disk. NOTE we do not include mempool transactions here as you cannot void a transaction
                   until it has been at least one confirmation */
                TAO::Ledger::Transaction tx;
                if(!LLD::Ledger->ReadTx(hashLast, tx))
                    return debug::error(FUNCTION, "Failed to read transaction");

                /* Loop through all contracts and test any debits or transfers. */
                uint32_t nContracts = tx.Size();
                for(uint32_t nContract = 0; nContract < nContracts; ++nContract)
                {
                    /* Check to see whether this contract has already been processed */
                    if(cacheProcessed.find(std::make_pair(hashLast, nContract)) != cacheProcessed.end() )
                        continue;

                    /* Reference to contract to check */
                    TAO::Operation::Contract& contract = tx[nContract];

                    /* First check to see if there are any conditions.  If not then they can't have expired! */
                    if(contract.Empty(TAO::Operation::Contract::CONDITIONS))
                    {
                        /* Add this contract to the processed list so we don't ever process it again and continue */
                        cacheProcessed.insert(std::make_pair(hashLast, nContract));
                        continue;
                    }

                    /* Next check to see if there is an expiration condition */
                    if(!HasExpires(contract))
                    {
                        /* Add this contract to the processed list so we don't ever process it again and continue */
                        cacheProcessed.insert(std::make_pair(hashLast, nContract));
                        continue;
                    }

                    /* The proof to check for this contract */
                    TAO::Register::Address hashProof;

                    /* Reset the contract to the position of the primitive. */
                    contract.SeekToPrimitive();

                    /* The operation */
                    uint8_t nOP;
                    contract >> nOP;

                    /* Check for that the debit is meant for us. */
                    if(nOP == TAO::Operation::OP::DEBIT)
                    {
                        /* Get the source address which is the proof for the debit */
                        contract >> hashProof;

                        /* Get the hashTo from the debit transaction. */
                        TAO::Register::Address hashTo;
                        contract >> hashTo;

                        /* Get the amount to respond to. */
                        uint64_t nAmount = 0;
                        contract >> nAmount;

                        /* Check to see if this debit has already been sent */
                        if(LLD::Ledger->HasProof(hashProof, hashLast, nContract, TAO::Ledger::FLAGS::MEMPOOL))
                        {
                            /* Add this contract to the processed list so we don't ever process it again and continue */
                            cacheProcessed.insert(std::make_pair(hashLast, nContract));
                            continue;
                        }

                         /* Check to see whether there are any partial credits already claimed against the debit */
                        uint64_t nClaimed = 0;
                        if(!LLD::Ledger->ReadClaimed(hashLast, nContract, nClaimed, TAO::Ledger::FLAGS::MEMPOOL))
                            nClaimed = 0;

                        /* Check that there is something to be claimed */
                        if(nClaimed == nAmount)
                        {
                            /* Add this contract to the processed list so we don't ever process it again and continue */
                            cacheProcessed.insert(std::make_pair(hashLast, nContract));
                            continue;
                        }

                        /* Reduce the amount to credit by the amount already claimed */
                        nAmount -= nClaimed;

                        /* Populate the temp void contract */
                        voidContract.Clear();
                        voidContract << uint8_t(TAO::Operation::OP::CREDIT) << hashLast << uint32_t(nContract) << hashProof <<  hashProof << nAmount;
                    }
                    else if(nOP == TAO::Operation::OP::TRANSFER)
                    {
                        /* The register address being transferred */
                        TAO::Register::Address hashRegister;
                        contract >> hashRegister;

                        /* Get recipient genesis hash */
                        contract >> hashProof;

                        /* Read the force transfer flag */
                        uint8_t nType = 0;
                        contract >> nType;

                        /* Ensure this wasn't a forced transfer (which requires no Claim) */
                        if(nType == TAO::Operation::TRANSFER::FORCE)
                        {
                            /* Add this contract to the processed list so we don't ever process it again and continue */
                            cacheProcessed.insert(std::make_pair(hashLast, nContract));
                            continue;
                        }

                        /* Check that the sender has not claimed it back (voided) */
                        TAO::Register::State state;
                        if(!LLD::Register->ReadState(hashRegister, state, TAO::Ledger::FLAGS::MEMPOOL))
                        {
                            /* Add this contract to the processed list so we don't ever process it again and continue */
                            cacheProcessed.insert(std::make_pair(hashLast, nContract));
                            continue;
                        }

                        /* Make sure the register claim is in SYSTEM pending from a transfer.  */
                        if(state.hashOwner.GetType() != TAO::Ledger::GENESIS::SYSTEM)
                        {
                            /* Add this contract to the processed list so we don't ever process it again and continue */
                            cacheProcessed.insert(std::make_pair(hashLast, nContract));
                            continue;
                        }

                        /* Check to see if this transfer has already been claimed by the recipient or us */
                        if(LLD::Ledger->HasProof(hashRegister, tx.GetHash(), nContract, TAO::Ledger::FLAGS::MEMPOOL))
                        {
                            /* Add this contract to the processed list so we don't ever process it again and continue */
                            cacheProcessed.insert(std::make_pair(hashLast, nContract));
                            continue;
                        }

                        /* Populate the temp void contract */
                        voidContract.Clear();
                        voidContract << (uint8_t)TAO::Operation::OP::CLAIM << hashLast << uint32_t(nContract) << hashRegister;

                    }

                    /* If we have gotten this far then check the conditions */
                    TAO::Operation::Condition condition = TAO::Operation::Condition(contract, voidContract);
                    if(!condition.Execute())
                        continue;

                    /* Add the contract to the return list  . */
                    vContracts.push_back(std::make_tuple(contract, nContract, 0));
                }

                /* Set the next last. */
                hashLast = !tx.IsFirst() ? tx.hashPrevTx : 0;
            }

            return true;
        }



        /* Get a user's notifications. */
        json::json Users::Notifications(const json::json& params, bool fHelp)
        {
            /* JSON return value. */
            json::json ret = json::json::array();

            /* Get the Genesis ID. */
            uint256_t hashGenesis = 0;

            /* Get genesis by raw hex. */
            if(params.find("genesis") != params.end())
                hashGenesis.SetHex(params["genesis"].get<std::string>());

            /* Get genesis by username. */
            else if(params.find("username") != params.end())
                hashGenesis = TAO::Ledger::SignatureChain::Genesis(params["username"].get<std::string>().c_str());

            /* Check for logged in user.  NOTE: we rely on the GetSession method to check for the existence of a valid session ID
               in the parameters in multiuser mode, or that a user is logged in for single user mode. Otherwise the GetSession 
               method will throw an appropriate error. */
            else
                hashGenesis = users->GetSession(params).GetAccount()->Genesis();

            /* The genesis hash of the API caller, if logged in */
            uint256_t hashCaller = users->GetCallersGenesis(params);

            if(config::fClient.load() && hashGenesis != hashCaller)
                throw APIException(-300, "API can only be used to lookup data for the currently logged in signature chain when running in client mode");

            /* Number of results to return. */
            uint32_t nLimit = 100;

            /* Offset into the result set to return results from */
            uint32_t nOffset = 0;

            /* Sort order to apply */
            std::string strOrder = "desc";

            /* Vector of where clauses to apply to filter the results */
            std::map<std::string, std::vector<Clause>> vWhere;

            /* Get the params to apply to the response. */
            GetListParams(params, strOrder, nLimit, nOffset, vWhere);

            /* Flag indicating there are top level filters  */
            bool fHasFilter = vWhere.count("") > 0;

            /* fields to ignore in the where clause.  This is necessary so that the suppressed param is not treated as 
               standard where clauses to filter the json */
            std::vector<std::string> vIgnore = {"suppressed"};

            /* Check for suppressed parameter. */
            bool fIncludeSuppressed = false;
            if(params.find("suppressed") != params.end())
                fIncludeSuppressed = params["suppressed"].get<std::string>() == "true" || params["suppressed"].get<std::string>() == "1";

            /* The total number of notifications. */
            uint32_t nTotal = 0;

            /* Get the outstanding contracts not yet credited or claimed. */
            std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> vContracts;
            GetOutstanding(hashGenesis, fIncludeSuppressed, vContracts);

            /* Get any expired contracts not yet voided. */
            GetExpired(hashGenesis, fIncludeSuppressed, vContracts);

            /* Get notifications for foreign token registers. */
            for(const auto& contract : vContracts)
            {
                /* Get a reference to the contract */
                const TAO::Operation::Contract& refContract = std::get<0>(contract);

                /* Get contract JSON data. */
                json::json obj = ContractToJSON(hashCaller, refContract, std::get<1>(contract), 1);

                obj["txid"]      = refContract.Hash().ToString();
                obj["time"]      = refContract.Timestamp();

                /* If caller has chosen to include suppressed notifications, then include an extra field in the response to 
                   indicate which ones are suppressed */
                if(fIncludeSuppressed)
                {
                    uint64_t nTimeout = 0;
                    obj["suppressed"] = (LLD::Local->ReadSuppressNotification(refContract.Hash(), std::get<1>(contract), nTimeout) && nTimeout > runtime::unifiedtimestamp());
                }

                /* Check to see if there is a proof for the contract, indicating this is a split dividend payment and the
                   hashProof is the account the proves the ownership of it*/
                TAO::Register::Address hashProof = std::get<2>(contract);

                if(hashProof != 0)
                {
                    /* Read the object register, which is the token . */
                    TAO::Register::Object account;
                    if(!LLD::Register->ReadState(hashProof, account, TAO::Ledger::FLAGS::MEMPOOL))
                        throw APIException(-13, "Account not found");

                    /* Parse the object register. */
                    if(!account.Parse())
                        throw APIException(-36, "Failed to parse object register");

                    /* Check that this is an account */
                    if(account.Base() != TAO::Register::OBJECTS::ACCOUNT )
                        throw APIException(-65, "Object is not an account");

                    /* Get the token address */
                    TAO::Register::Address hashToken = account.get<uint256_t>("token");

                    /* Read the token register. */
                    TAO::Register::Object token;
                    if(!LLD::Register->ReadState(hashToken, token, TAO::Ledger::FLAGS::LOOKUP))
                        throw APIException(-125, "Token not found");

                    /* Parse the object register. */
                    if(!token.Parse())
                        throw APIException(-36, "Failed to parse object register");

                    /* Get the token supply so that we an determine our share */
                    uint64_t nSupply = token.get<uint64_t>("supply");

                    /* Get the balance of our token account */
                    uint64_t nBalance = account.get<uint64_t>("balance");

                    /* Get the amount from the debit contract*/
                    uint64_t nAmount = 0;
                    TAO::Register::Unpack(refContract, nAmount);

                    /* Get the from account the debit contract*/
                    TAO::Register::Address hashFrom;
                    TAO::Register::Unpack(refContract, hashFrom);

                    /* Get the account that made the debit, so that we can determine the decimals */
                    TAO::Register::Object accountFrom;
                    if(!LLD::Register->ReadState(hashFrom, accountFrom, TAO::Ledger::FLAGS::LOOKUP))
                        throw APIException(-13, "Account not found");

                    /* Parse the object register. */
                    if(!accountFrom.Parse())
                        throw APIException(-36, "Failed to parse object register");

                    /* Calculate the partial debit amount that this token holder is entitled to. */
                    uint64_t nPartial = (nAmount * nBalance) / nSupply;

                    /* Update the JSON with the partial amount */
                    obj["amount"] = (double) nPartial / pow(10, GetDecimals(accountFrom));

                    /* Add the token account to the notification */
                    obj["proof"] = hashProof.ToString();

                    std::string strProof = Names::ResolveName(hashCaller, hashProof);
                    if(!strProof.empty())
                        obj["proof_name"] = strProof;

                    /* Also flag this notification as a split dividend */
                    obj["dividend_payment"] = true;

                }

                /* Check to see that it matches the where clauses */
                if(fHasFilter)
                {
                    /* Skip this top level record if not all of the filters were matched */
                    if(!MatchesWhere(obj, vWhere[""], vIgnore))
                        continue;
                }

                ++nTotal;

                /* Check the offset. */
                if(nTotal <= nOffset)
                    continue;
                
                /* Check the limit */
                if(nTotal - nOffset > nLimit)
                    break;

                /* Add to return object. */
                ret.push_back(obj);


            }

            /* Get the outstanding legacy transactions not yet credited. */
            std::vector<std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t>> vLegacy;
            GetOutstanding(hashGenesis, fIncludeSuppressed, vLegacy);

            /* Get notifications for foreign token registers. */
            for(const auto& tx : vLegacy)
            {
    
                /* The register address of the recipient account */
                TAO::Register::Address hashTo;
                Legacy::ExtractRegister(tx.first->vout[tx.second].scriptPubKey, hashTo);

                /* Get transaction JSON data. */
                json::json obj;
                obj["OP"]       = "LEGACY";
                obj["address"]  = hashTo.ToString();

                /* Resolve the name of the token/account/register that the debit is to */
                std::string strTo = Names::ResolveName(hashCaller, hashTo);
                if(!strTo.empty())
                    obj["account_name"] = strTo;

                obj["amount"]   = (double)tx.first->vout[tx.second].nValue / TAO::Ledger::NXS_COIN;
                obj["txid"]     = tx.first->GetHash().GetHex();
                obj["time"]     = tx.first->nTime;


                /* Check to see that it matches the where clauses */
                if(fHasFilter)
                {
                    /* Skip this top level record if not all of the filters were matched */
                    if(!MatchesWhere(obj, vWhere[""], vIgnore))
                        continue;
                }

                ++nTotal;

                /* Check the offset. */
                if(nTotal <= nOffset)
                    continue;
                
                /* Check the limit */
                if(nTotal - nOffset > nLimit)
                    break;

                /* Add to return object. */
                ret.push_back(obj);
            }

            return ret;
        }


        /* Process any outstanding notifications for a particular sig chain */
        json::json Users::ProcessNotifications(const json::json& params, bool fHelp)
        {
            /* JSON return value. */
            json::json ret;

            /* Track how many contracts were processed */
            uint8_t nProcessed = 0;

            /* Store the transaction ID's of all transactions that were created (in case there are more than 99 notifications ) */
            std::vector<uint512_t> vTxIDs; 

            /* default the processed in the return JSON to 0 */
            ret["processed"] = 0;

            /* Testnet is considered local if no dns is being used or if using a private network */
            bool fLocalTestnet = config::fTestNet.load()
                && (!config::GetBoolArg("-dns", true) || config::GetBoolArg("-private"));

            /* Make sure the tritium server has a connection. (skip check if running local testnet) */
            if(!fLocalTestnet && LLP::TRITIUM_SERVER && LLP::TRITIUM_SERVER->GetConnectionCount() == 0)
                throw APIException(-255, "Cannot process notifications until peers are connected");

            /* Don't process events while synchronizing. */
            if(TAO::Ledger::ChainState::Synchronizing())
                throw APIException(-256, "Cannot process notifications whilst synchronizing");

            /* Flag indicating that this call should log this call in the session activity */
            bool fLogActivity = true;
            if(params.find("logactivity") != params.end())
                fLogActivity = params["logactivity"].get<std::string>() == "true" || params["logactivity"].get<std::string>() == "1";
            
            /* Get the session to be used for this API call */
            Session& session = users->GetSession(params, true, fLogActivity);

            /* Get the account. */
            const memory::encrypted_ptr<TAO::Ledger::SignatureChain>& user = session.GetAccount();
            if(!user)
                throw APIException(-10, "Invalid session ID");

            /* Set the hash genesis for this user. */
            uint256_t hashGenesis = user->Genesis();

            /* Get the PIN to be used for this API call */
            SecureString strPIN = users->GetPin(params, TAO::Ledger::PinUnlock::NOTIFICATIONS);

            /* Retrieve user's default NXS account. */
            std::string strAccount = config::GetArg("-events_account", "default");
            TAO::Register::Object defaultAccount;
            if(!TAO::Register::GetNameRegister(hashGenesis, strAccount, defaultAccount))
                throw APIException(-63, "Could not retrieve default NXS account to credit");

            /* Check for suppressed parameter. */
            bool fIncludeSuppressed = false;
            if(params.find("suppressed") != params.end())
                fIncludeSuppressed = params["suppressed"].get<std::string>() == "true" || params["suppressed"].get<std::string>() == "1";


            /* Get the list of outstanding contracts. */
            std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> vContracts;
            GetOutstanding(hashGenesis, fIncludeSuppressed, vContracts);

            /* Get any expired contracts not yet voided. */
            std::vector<std::tuple<TAO::Operation::Contract, uint32_t, uint256_t>> vExpired;
            GetExpired(hashGenesis, fIncludeSuppressed, vExpired);

            /* Get the list of outstanding legacy transactions . */
            std::vector<std::pair<std::shared_ptr<Legacy::Transaction>, uint32_t>> vLegacyTx;
            GetOutstanding(hashGenesis, fIncludeSuppressed, vLegacyTx);

            /* Check if there is anything to process */
            if(vContracts.size() == 0 && vLegacyTx.size() == 0 && vExpired.size() == 0)
                return ret;

            /* Ensure that the signature is mature. This is an expensive check, so we only check this after we know 
               that there is something to process */
            CheckMature(hashGenesis);            

            /* The transaction hash. */
            uint512_t hashTx;

            /* The current contract ID being processed */
            uint32_t nContract = 0;

            /* hash from, hash to, and amount for operations. */
            TAO::Register::Address hashFrom;
            TAO::Register::Address hashTo;

            uint64_t nAmount = 0;
            uint64_t nTimestamp = runtime::unifiedtimestamp() + 1;

            /* Temporary map for pre-states to be passed into the sanitization Build() for each contract. */
            std::map<uint256_t, TAO::Register::State> mapStates;

            /* Loop through each contract in the notification queue. */
            std::queue<TAO::Operation::Contract> vProcessQueue;
            
            try
            {
            
                for(const auto& contract : vContracts)
                {
                    /* Reset the receiving address */
                    hashTo = uint256_t(0);

                    /* Get a reference to the contract */
                    const TAO::Operation::Contract& refContract = std::get<0>(contract);

                    /* Set the transaction hash. */
                    hashTx = refContract.Hash();

                    /* Get the contract ID */
                    nContract = std::get<1>(contract);

                    /* Get the maturity for this transaction. */
                    bool fMature = LLD::Ledger->ReadMature(hashTx);

                    /* Reset the contract to the position of the primitive. */
                    refContract.SeekToPrimitive();

                    /* Get the opcode. */
                    uint8_t OPERATION;
                    refContract >> OPERATION;

                    /* Check the opcodes for debit, coinbase or transfers. */
                    switch (OPERATION)
                    {
                        /* Check for Debits. */
                        case Operation::OP::DEBIT:
                        {
                            /* Check to see if there is a proof for the contract, indicating this is a split dividend payment
                            and the hashProof is the account the proves the ownership of it*/
                            TAO::Register::Address hashProof;
                            hashProof = std::get<2>(contract);

                            if(hashProof != 0)
                            {
                                /* If this is a split dividend payment then we can only (currently) process it if it is NXS.
                                Therefore we need to retrieve the account/token the debit is from so that we can check */

                                /* Get the token/account we are debiting from */
                                refContract >> hashFrom;
                                TAO::Register::Object from;
                                if(!LLD::Register->ReadState(hashFrom, from))
                                    continue;

                                /* Parse the object register. */
                                if(!from.Parse())
                                    continue;

                                /* Get the token type */
                                uint256_t hashToken = from.get<uint256_t>("token");

                                /* If this is a NXS debit then process the credit to the default account */
                                if(hashToken == 0)
                                {
                                    hashTo = defaultAccount.get<uint256_t>("address");
                                }
                                else
                                {
                                    /* Search for an account to credit the tokens to */
                                    if(!GetAccountByToken(user->Genesis(), hashToken, hashTo))
                                    /* If no account has been found for the token credit then skip the notification */
                                        continue;
                                }

                                /* Read the object register, which is the proof account . */
                                TAO::Register::Object account;
                                if(!LLD::Register->ReadState(hashProof, account, TAO::Ledger::FLAGS::MEMPOOL))
                                    continue;

                                /* Parse the object register. */
                                if(!account.Parse())
                                    continue;

                                /* Check that this is an account */
                                if(account.Standard() != TAO::Register::OBJECTS::ACCOUNT )
                                    continue;

                                /* Get the token address of for the proof account*/
                                TAO::Register::Address hashProofToken = account.get<uint256_t>("token");

                                /* Read the token register. */
                                TAO::Register::Object token;
                                if(!LLD::Register->ReadState(hashProofToken, token, TAO::Ledger::FLAGS::MEMPOOL))
                                    continue;

                                /* Parse the object register. */
                                if(!token.Parse())
                                    continue;

                                /* Get the token supply so that we an determine our share */
                                uint64_t nSupply = token.get<uint64_t>("supply");

                                /* Get the balance of our token account */
                                uint64_t nBalance = account.get<uint64_t>("balance");

                                /* Get the amount from the debit contract*/
                                uint64_t nAmount = 0;
                                TAO::Register::Unpack(refContract, nAmount);

                                /* Calculate the partial debit amount that this token holder is entitled to. */
                                uint64_t nPartial = (nAmount * nBalance) / nSupply;

                                /* Submit the payload object for the split dividend. Notice we use the hashProof */
                                TAO::Operation::Contract credit;
                                credit << uint8_t(TAO::Operation::OP::CREDIT);
                                credit << hashTx << std::get<1>(contract);
                                credit << hashTo << hashProof;
                                credit << nPartial;

                                /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                                credit.Bind(nTimestamp, hashGenesis);

                                /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                                if(!sanitize_contract(credit, mapStates))
                                    continue;

                                /* Add the contract to the transaction */
                                vProcessQueue.push(credit);

                            }
                            else
                            {
                                /* Set to and from hashes and amount. */
                                refContract >> hashFrom;
                                refContract >> hashTo;
                                refContract >> nAmount;

                                /* Submit the payload object. */
                                TAO::Operation::Contract credit;
                                credit << uint8_t(TAO::Operation::OP::CREDIT);
                                credit << hashTx << std::get<1>(contract);
                                credit << hashTo << hashFrom;
                                credit << nAmount;

                                /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                                credit.Bind(nTimestamp, hashGenesis);

                                /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                                if(!sanitize_contract(credit, mapStates))
                                    continue;

                                /* Add the contract to the transaction */
                                vProcessQueue.push(credit);
                            }

                            /* Log debug message. */
                            debug::log(2, FUNCTION, "Matching DEBIT with CREDIT");

                            break;
                        }

                        /* Check for Coinbases. */
                        case Operation::OP::COINBASE:
                        {
                            /* Check that the coinbase is mature and ready to be credited. */
                            if(!fMature)
                            {
                                //debug::error(FUNCTION, "Immature coinbase.");
                                continue;
                            }

                            /* Set the genesis hash and the amount. */
                            refContract >> hashFrom;
                            refContract >> nAmount;

                            /* Get the address that this name register for default account is pointing to. */
                            hashTo = defaultAccount.get<uint256_t>("address");

                            /* Submit the payload object. */
                            TAO::Operation::Contract credit;
                            credit<< uint8_t(TAO::Operation::OP::CREDIT);
                            credit << hashTx << std::get<1>(contract);
                            credit << hashTo << hashFrom;
                            credit << nAmount;

                            /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                            credit.Bind(nTimestamp, hashGenesis);

                            /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                            if(!sanitize_contract(credit, mapStates))
                                continue;

                            /* Add the contract to the transaction */
                            vProcessQueue.push(credit);

                            /* Log debug message. */
                            debug::log(2, FUNCTION, "Matching COINBASE with CREDIT");

                            break;
                        }

                        /* Check for Transfers. */
                        case Operation::OP::TRANSFER:
                        {
                            /* Get the address of the asset being transfered from the transaction. */
                            refContract >> hashFrom;

                            /* Get the genesis hash (recipient) of the transfer. */
                            refContract >> hashTo;

                            /* Read the force transfer flag */
                            uint8_t nType = 0;
                            refContract >> nType;

                            /* Ensure this wasn't a forced transfer (which requires no Claim) */
                            if(nType == TAO::Operation::TRANSFER::FORCE)
                                continue;

                            /* Create a name object for the claimed object unless this is a Name or Namespace already */
                            if(!hashFrom.IsName() && !hashFrom.IsNamespace())
                            {
                                /* Create a new name from the previous owners name */
                                TAO::Operation::Contract nameContract = Names::CreateName(user->Genesis(), hashTx);

                                /* If the Name contract operation was created then add it to the transaction */
                                if(!nameContract.Empty())
                                {
                                    vProcessQueue.push(nameContract);
                                }
                            }

                            /* Add the CLAIM operation */
                            TAO::Operation::Contract claim;
                            claim << uint8_t(TAO::Operation::OP::CLAIM); // the op code
                            claim << hashTx << std::get<1>(contract); // the transaction hash
                            claim << hashFrom; // the proof

                            /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                            claim.Bind(nTimestamp, hashGenesis);

                            /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                            if(!sanitize_contract(claim, mapStates))
                                continue;

                            /* Add the contract to the transaction */
                            vProcessQueue.push(claim);

                            /* Log debug message. */
                            debug::log(2, FUNCTION, "Matching TRANSFER with CLAIM");

                            break;
                        }
                        default:
                            break;
                    }
                }
            
                /* Now process the legacy transactions */
                for(const auto& contract : vLegacyTx)
                {
                    /* Set the transaction hash. */
                    hashTx = contract.first->GetHash();

                    /* The index of the output in the legacy transaction */
                    nContract = contract.second;

                    /* The TxOut to be checked */
                    const Legacy::TxOut& txLegacy = contract.first->vout[nContract];

                    /* The hash of the receiving account. */
                    TAO::Register::Address hashAccount;

                    /* Extract the sig chain account register address from the legacy script */
                    if(!Legacy::ExtractRegister(txLegacy.scriptPubKey, hashAccount))
                        continue;

                    /* Get the token / account object that the debit was made to. */
                    TAO::Register::Object debit;
                    if(!LLD::Register->ReadState(hashAccount, debit))
                        continue;

                    /* Parse the object register. */
                    if(!debit.Parse())
                        throw APIException(-41, "Failed to parse object from debit transaction");

                    /* Check for the owner to make sure this was a send to the current users account */
                    if(debit.hashOwner == user->Genesis())
                    {
                        /* Identify trust migration to create OP::MIGRATE instead of OP::CREDIT */

                        /* Check if output is new trust account (no stake or balance) */
                        if(debit.Standard() == TAO::Register::OBJECTS::TRUST
                                && debit.get<uint64_t>("stake") == 0 && debit.get<uint64_t>("trust") == 0)
                        {
                            /* Need to check for migration.
                            * Trust migration converts a legacy trust key to a trust account register.
                            * It will send all inputs from an existing trust key, with one output to a new trust account.
                            */
                            bool fMigration = false; //if this stays false, not a migration, fall through to OP::CREDIT

                            /* Trust key data we need for OP::MIGRATE */
                            uint32_t nScore;
                            uint576_t hashTrust;
                            uint512_t hashLast;
                            Legacy::TrustKey trustKey;

                            /* This loop will only have one iteration. If it breaks out before end, fMigration stays false */
                            while(1)
                            {
                                /* Trust account output must be only output for the transaction */
                                if(nContract != 0 || contract.first->vout.size() > 1)
                                    break;

                                /* Trust account must be new (not indexed) */
                                if(LLD::Register->HasTrust(hashGenesis))
                                    break;

                                /* Retrieve the trust key being converted */
                                if(!Legacy::FindMigratedTrustKey(*contract.first, trustKey))
                                    break;

                                /* Verify trust key not already converted */
                                hashTrust.SetBytes(trustKey.vchPubKey);
                                if(LLD::Legacy->HasTrustConversion(hashTrust))
                                    break;

                                /* Get last trust for the legacy trust key */
                                TAO::Ledger::BlockState stateLast;
                                if(!LLD::Ledger->ReadBlock(trustKey.hashLastBlock, stateLast))
                                    break;

                                /* Last stake block must be at least v5 and coinstake must be a legacy transaction */
                                if(stateLast.nVersion < 5 || stateLast.vtx[0].first != TAO::Ledger::TRANSACTION::LEGACY)
                                    break;

                                /* Extract the coinstake from the last trust block */
                                Legacy::Transaction txLast;
                                if(!LLD::Legacy->ReadTx(stateLast.vtx[0].second, txLast))
                                    break;

                                /* Verify that the transaction is a coinstake */
                                if(!txLast.IsCoinStake())
                                    break;

                                hashLast = txLast.GetHash();

                                /* Extract the trust score from the coinstake */
                                uint1024_t hashLastBlock;
                                uint32_t nSequence;

                                if(txLast.IsGenesis())
                                    nScore = 0;

                                else if(!txLast.ExtractTrust(hashLastBlock, nSequence, nScore))
                                    break;

                                fMigration = true;
                                break;
                            }

                            /* Everything verified for migration and we have the data we need. Set up OP::MIGRATE */
                            if(fMigration)
                            {
                                /* The amount to migrate */
                                const int64_t nLegacyAmount = txLegacy.nValue;
                                uint64_t nAmount = 0;
                                if(nLegacyAmount > 0)
                                    nAmount = nLegacyAmount;

                                /* Set up the OP::MIGRATE */
                                TAO::Operation::Contract migrate;
                                migrate << uint8_t(TAO::Operation::OP::MIGRATE) << hashTx << hashAccount << hashTrust
                                            << nAmount << nScore << hashLast;

                                /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                                migrate.Bind(nTimestamp, hashGenesis);

                                /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                                if(!sanitize_contract(migrate, mapStates))
                                    continue;

                                /* Add the contract to the transaction */
                                vProcessQueue.push(migrate);

                                /* Log debug message. */
                                debug::log(2, FUNCTION, "Matching LEGACY SEND with trust key MIGRATE",
                                    "\n    Migrated amount: ", std::fixed, (nAmount / (double)TAO::Ledger::NXS_COIN),
                                    "\n    Migrated trust: ", nScore,
                                    "\n    To trust account: ", hashAccount.ToString(),
                                    "\n    Last stake block: ", trustKey.hashLastBlock.SubString(),
                                    "\n    Last stake tx: ", hashLast.SubString());

                                continue;
                            }
                        }

                        /* No migration. Use normal credit process */

                        /* Check the object base to see whether it is an account. */
                        if(debit.Base() == TAO::Register::OBJECTS::ACCOUNT)
                        {
                            if(debit.get<uint256_t>("token") != 0)
                                throw APIException(-51, "Debit transaction is not for a NXS account.  Please use the tokens API for crediting token accounts.");

                            /* The amount to credit */
                            const uint64_t nAmount = txLegacy.nValue;

                            /* if we passed these checks then insert the credit contract into the tx */
                            TAO::Operation::Contract credit;
                            credit << uint8_t(TAO::Operation::OP::CREDIT) << hashTx << uint32_t(nContract) << hashAccount <<  TAO::Register::WILDCARD_ADDRESS << nAmount;

                            /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                            credit.Bind(nTimestamp, hashGenesis);

                            /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                            if(!sanitize_contract(credit, mapStates))
                                continue;

                            /* Add the contract to the transaction */
                            vProcessQueue.push(credit);

                            /* Log debug message. */
                            debug::log(2, FUNCTION, "Matching LEGACY SEND with CREDIT");
                        }
                        else
                            continue;
                    }
                }


                /* Finally process any expired transactions that can be voided. */
                for(const auto& contract : vExpired)
                {
                    /* Get a reference to the contract */
                    const TAO::Operation::Contract& refContract = std::get<0>(contract); 

                    /* Get the transaction hash */
                    hashTx = refContract.Hash();

                    /* Get the current contract ID */
                    nContract = std::get<1>(contract);

                    /* Attempt to add the void contract */
                    TAO::Operation::Contract voidContract;
                    if(VoidContract(refContract, nContract, voidContract))
                    {
                        /* Bind the contract to the tx so that the genesis and timestamp are bound prior to sanitizing */
                        voidContract.Bind(nTimestamp, hashGenesis);

                        /* Sanitize the contract to make sure it builds and executes before we add it to the transaction */
                        if(!sanitize_contract(voidContract, mapStates))
                            continue;

                        /* Add the void contract */
                        vProcessQueue.push(voidContract);
                    }
                }
            }
            catch(const APIException& ex)
            {
                /* Suppress this notification for 10 x the notifications interval, so by default it will be retried after 50s */
                uint64_t nSuppress = config::GetArg("-notificationsinterval", 5) * 10;

                debug::log(1, FUNCTION, "Failed to process notification: ", hashTx.SubString(), ". Supressing notification for ", nSuppress, " seconds.");

                /* Suppress this notification for 1 hour or until manually attempted  */
                LLD::Local->WriteSuppressNotification(hashTx, nContract, runtime::unifiedtimestamp() + nSuppress);

                /* Rethrow the caught exception once it has been suppressed */
                throw ex;
            }

            /* If any of the notifications have been matched, execute the operations layer and sign the transaction. */
            while(!vProcessQueue.empty())
            {
                /* Lock the signature chain. */
                LOCK(session.CREATE_MUTEX);

                /* Create the transaction output. */
                TAO::Ledger::Transaction txout;
                if(!TAO::Ledger::CreateTransaction(user, strPIN, txout))
                    throw APIException(-17, "Failed to create transaction");

                /* Add the contracts into tx. NOTE we add one less than maximum allowed to leave room for a fee contract. */
                for(uint32_t i = 0; i < TAO::Ledger::MAX_TRANSACTION_CONTRACTS -1; ++i)
                {
                    /* Stop if run out of items to process. */
                    if(vProcessQueue.empty())
                        break;

                    /* Add the contract to tx. */
                    txout[i] = vProcessQueue.front();
                    vProcessQueue.pop();
                }

                /* Check for end. */
                if(txout.Size() == 0)
                    break;

                /* Increment processed contracts counter by number of contracts in transaction */
                nProcessed += txout.Size();

                /* Add the fee */
                AddFee(txout);

                /* If we are in client mode then we need to get a peer to validate the transaction for us, as we will not
                    have the ability to sanitize contracts with conditions or where other consensus rules require foreign
                    registers.  
                */
                if(config::fClient.load())
                {
                    uint32_t nContract = 0;

                    if(!validate_transaction(txout, nContract))
                    {
                        /* Retrieve the contract from our transaction */
                        const TAO::Operation::Contract& contract = txout[nContract];

                        /* Unpack the txid and contract ID of the notification that this contract credits/claims */
                        TAO::Register::Unpack(contract, hashTx, nContract);

                        debug::log(1, FUNCTION, "CLIENT MODE: validation failed for notification: ", hashTx.SubString());

                        /* Suppress this notification for 1 hour or until manually attempted  */
                        LLD::Local->WriteSuppressNotification(hashTx, nContract, runtime::unifiedtimestamp() + 3600);

                        /* Throw exception to signify this transaction failed to be accepted due to the contract failing peer 
                           validation and break out of this iteration of the process */
                        throw APIException(-257, "Contract failed peer validation");
                    }
                    
                }

                /* Execute the operations layer. */
                if(!txout.Build())
                    throw APIException(-30, "Failed to build register pre-states");

                /* Sign the transaction. */
                if(!txout.Sign(session.GetAccount()->Generate(txout.nSequence, strPIN)))
                    throw APIException(-31, "Ledger failed to sign transaction");

                /* Execute the operations layer. */
                if(!TAO::Ledger::mempool.Accept(txout))
                    throw APIException(-32, "Failed to accept");

                /* Capture the transaction ID */
                vTxIDs.push_back(txout.GetHash());
            }

            /* Populate the response JSON */
            ret["processed"] = nProcessed;
            ret["transactions"] = json::json::array();

            for(const auto& tx : vTxIDs)
            {
                json::json jsonTX;
                jsonTX["txid"] = tx.ToString();
                ret["transactions"].push_back(jsonTX);
            }

            return ret;

        }

        /* Checks that the contract passes both Build() and Execute() */
        bool Users::sanitize_contract(TAO::Operation::Contract& contract, std::map<uint256_t, TAO::Register::State> &mapStates)
        {
            /* Return flag */
            bool fSanitized = false;

            /* Lock the mempool at this point so that we can build and execute inside a mempool transaction */
            RLOCK(TAO::Ledger::mempool.MUTEX);

            try
            {
                /* Start a ACID transaction (to be disposed). */
                LLD::TxnBegin(TAO::Ledger::FLAGS::MEMPOOL);

                /* Temporarily disable error logging so that we don't log errors for contracts that fail to execute. */
                debug::fLogError = false;

                fSanitized = TAO::Register::Build(contract, mapStates, TAO::Ledger::FLAGS::MEMPOOL)
                             && TAO::Operation::Execute(contract, TAO::Ledger::FLAGS::MEMPOOL);

                /* Reenable error logging. */
                debug::fLogError = true;

                /* Abort the mempool ACID transaction once the contract is sanitized */
                LLD::TxnAbort(TAO::Ledger::FLAGS::MEMPOOL);

            }
            catch(const std::exception& e)
            {
                /* Just in case we encountered an exception whilst error logging was off, reenable error logging. */
                debug::fLogError = true;

                /* Abort the mempool ACID transaction */
                LLD::TxnAbort(TAO::Ledger::FLAGS::MEMPOOL);

                /* Log the error and attempt to continue processing */
                debug::error(FUNCTION, e.what());
            }

            return fSanitized;
        }


        /* Used when in client mode, this method will send the transaction to a peer to validate it.  This will in turn check 
        *  each contract in the transaction to verify that the conditions are met, the contract can be built, and executed.
        *  If any of the contracts in the transaction fail then the method will return the index of the failed contract.
        */
        bool Users::validate_transaction(const TAO::Ledger::Transaction& tx, uint32_t& nContract)
        {
            bool fValid = false;

            /* Check tritium server enabled. */
            if(LLP::TRITIUM_SERVER)
            {
                memory::atomic_ptr<LLP::TritiumNode>& pNode = LLP::TRITIUM_SERVER->GetConnection();
                if(pNode != nullptr)
                {
                    debug::log(1, FUNCTION, "CLIENT MODE: Validating transaction");

                    /* Create our trigger nonce. */
                    uint64_t nNonce = Util::GetRand();
                    pNode->PushMessage(LLP::Tritium::TYPES::TRIGGER, nNonce);

                    /* Request the transaction validation */
                    pNode->PushMessage(LLP::Tritium::ACTION::VALIDATE, uint8_t(LLP::Tritium::TYPES::TRANSACTION), tx);

                    /* Create the condition variable trigger. */
                    LLP::Trigger REQUEST_TRIGGER;
                    pNode->AddTrigger(LLP::Tritium::RESPONSE::VALIDATED, &REQUEST_TRIGGER);

                    /* Process the event. */
                    REQUEST_TRIGGER.wait_for_nonce(nNonce, 10000);

                    /* Cleanup our event trigger. */
                    pNode->Release(LLP::Tritium::RESPONSE::VALIDATED);

                    debug::log(1, FUNCTION, "CLIENT MODE: RESPONSE::VALIDATED received");

                    /* Check the response args to see if it was valid */
                    if(REQUEST_TRIGGER.HasArgs())
                    {
                        REQUEST_TRIGGER >> fValid;

                        /* If it was not valid then deserialize the failing contract ID from the response */
                        if(!fValid)
                        {
                            /* Deserialize the failing hash (which should be the one we sent) */
                            uint512_t hashTx;
                            REQUEST_TRIGGER >> hashTx;

                            /* Deserialize the failing contract ID */
                            REQUEST_TRIGGER >> nContract;

                            /* Check the hash is valid */
                            if(hashTx != tx.GetHash())
                                throw APIException(0, "Invalid transaction ID received from RESPONSE::VALIDATED");

                            /* Check the contract ID is valid */
                            if(nContract > tx.Size() -1)
                                throw APIException(0, "Invalid contract ID received from RESPONSE::VALIDATED");
                        }
                    }
                    else
                    {
                        throw APIException(0, "CLIENT MODE: timeout waiting for RESPONSE::VALIDATED");
                    }
                    
                }
                else
                    debug::error(FUNCTION, "no connections available...");
            }
        
            /* return the valid flag */
            return fValid;
        }
    }
}
