/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Util/include/random.h>

#include <LLD/include/global.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>
#include <TAO/Operation/include/enum.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/include/rollback.h>
#include <TAO/Register/include/create.h>
#include <TAO/Register/include/reserved.h>
#include <TAO/Register/types/address.h>

#include <TAO/Ledger/types/transaction.h>
#include <TAO/Ledger/include/enum.h>
#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/types/genesis.h>

#include <unit/catch2/catch.hpp>

TEST_CASE( "Register Rollback Tests", "[register]")
{
    using namespace TAO::Register;
    using namespace TAO::Operation;


    //rollback a token object register
    {
        //create the transaction object
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = TAO::Ledger::Genesis(Util::GetRand256(), true);
        tx.nSequence   = 0;
        tx.nTimestamp  = runtime::timestamp();

        //create object
        uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::TOKEN);
        Object account = CreateToken(hashRegister, 1000, 100);

        //payload
        tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::OBJECT) << account.GetState();

        //generate the prestates and poststates
        REQUIRE(tx.Build());

        //verify the prestates and poststates
        REQUIRE(tx.Verify());

        //commit to disk
        REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

        //check that register exists
        REQUIRE(LLD::Register->HasState(hashRegister));

        //rollback the transaction
        REQUIRE(Rollback(tx[0]));

        //make sure object is deleted
        REQUIRE_FALSE(LLD::Register->HasState(hashRegister));
    }


    //rollback a state write
    {
        //create object
        uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::RAW);
        uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);

        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::RAW) << std::vector<uint8_t>(10, 0xff);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check data
            REQUIRE(state.GetState() == std::vector<uint8_t>(10, 0xff));

        }

        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 1;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::WRITE) << hashRegister << std::vector<uint8_t>(10, 0x1f);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check data
            REQUIRE(state.GetState() == std::vector<uint8_t>(10, 0x1f));

            //rollback the transaction
            REQUIRE(Rollback(tx[0]));

            //grab the new state
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check data
            REQUIRE(state.GetState() == std::vector<uint8_t>(10, 0xff));
        }


        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 2;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::APPEND) << hashRegister << std::vector<uint8_t>(10, 0xff);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check data
            REQUIRE(state.GetState() == std::vector<uint8_t>(20, 0xff));

            //rollback the transaction
            REQUIRE(Rollback(tx[0]));

            //grab the new state
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check data
            REQUIRE(state.GetState() == std::vector<uint8_t>(10, 0xff));
        }
    }



    //rollback a transfer to a random genesis
    {
        //create object
        uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::RAW);
        uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);

        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::RAW) << std::vector<uint8_t>(10, 0xff);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check owner
            REQUIRE(state.hashOwner == hashGenesis);

        }

        //create the transaction object
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = hashGenesis;
        tx.nSequence   = 1;
        tx.nTimestamp  = runtime::timestamp();


        //payload
        tx[0] << uint8_t(OP::TRANSFER) << hashRegister << uint256_t(0xffff) << false;

        //generate the prestates and poststates
        REQUIRE(tx.Build());

        //verify the prestates and poststates
        REQUIRE(tx.Verify());

        //write transaction
        REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));

        //commit to disk
        REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

        //check that register exists
        State state;
        REQUIRE(LLD::Register->ReadState(hashRegister, state));

        //check owner
        REQUIRE(state.hashOwner.GetType() == 0);

        //rollback the transaction
        REQUIRE(Rollback(tx[0]));

        //grab the new state
        REQUIRE(LLD::Register->ReadState(hashRegister, state));

        //check owner
        REQUIRE(state.hashOwner == hashGenesis);
    }

    //rollback a forced transfer to a random genesis
    {
        //create object
        uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::RAW);
        uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);

        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::RAW) << std::vector<uint8_t>(10, 0xff);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check owner
            REQUIRE(state.hashOwner == hashGenesis);

        }

        //create the transaction object
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = hashGenesis;
        tx.nSequence   = 1;
        tx.nTimestamp  = runtime::timestamp();


        //payload
        tx[0] << uint8_t(OP::TRANSFER) << hashRegister << uint256_t(0xffff) << uint8_t(TRANSFER::FORCE);

        //generate the prestates and poststates
        REQUIRE(tx.Build());

        //verify the prestates and poststates
        REQUIRE(tx.Verify());

        //write transaction
        REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));

        //commit to disk
        REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

        //check that register exists
        State state;
        REQUIRE(LLD::Register->ReadState(hashRegister, state));

        //check owner has changed to uint256_t(0xffff) since we forced it
        REQUIRE(state.hashOwner == uint256_t(0xffff));

        //rollback the transaction
        REQUIRE(Rollback(tx[0]));

        //grab the new state
        REQUIRE(LLD::Register->ReadState(hashRegister, state));

        //check owner has reverted back to hashGenesis
        REQUIRE(state.hashOwner == hashGenesis);
    }



    //rollback a claim
    {
        //create object
        uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::RAW);
        uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);
        uint256_t hashGenesis2 = TAO::Ledger::Genesis(Util::GetRand256(), true);

        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::RAW) << std::vector<uint8_t>(10, 0xff);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check owner
            REQUIRE(state.hashOwner == hashGenesis);

        }

        uint512_t hashTx;
        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 1;
            tx.nTimestamp  = runtime::timestamp();


            //payload
            tx[0] << uint8_t(OP::TRANSFER) << hashRegister << hashGenesis2 << false;

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //write transaction
            REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));
            REQUIRE(LLD::Ledger->IndexBlock(tx.GetHash(), TAO::Ledger::ChainState::Genesis()));

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //get claim hash
            hashTx = tx.GetHash();

            //check that register exists
            State state;
            REQUIRE(LLD::Register->ReadState(hashRegister, state));

            //check owner
            REQUIRE(state.hashOwner.GetType() == 0);

        }


        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis2;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CLAIM) << hashTx << uint32_t(0) << hashRegister;

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            {
                State state;
                REQUIRE(LLD::Register->ReadState(hashRegister, state));

                //check owner
                REQUIRE(state.hashOwner == hashGenesis2);
            }

            //check proof is active
            REQUIRE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));

            //rollback the transaction
            REQUIRE(Rollback(tx[0]));

            //grab the new state
            {
                State state;
                REQUIRE(LLD::Register->ReadState(hashRegister, state));

                //check owner
                REQUIRE(state.hashOwner.GetType() == 0);

                //check for the proof
                REQUIRE_FALSE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));
            }

            {
                //check for event
                uint32_t nSequence;
                REQUIRE(LLD::Ledger->ReadSequence(hashGenesis2, nSequence));

                //check for tx
                TAO::Ledger::Transaction txEvent;
                REQUIRE(LLD::Ledger->ReadEvent(hashGenesis2, nSequence - 1, txEvent));

                //check the hashes match
                REQUIRE(txEvent.GetHash() == hashTx);
            }
        }


        //claim back to self and rollback
        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 2;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CLAIM) << hashTx << uint32_t(0) << hashRegister;

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            {
                State state;
                REQUIRE(LLD::Register->ReadState(hashRegister, state));

                //check owner
                REQUIRE(state.hashOwner == hashGenesis);
            }

            //check proof is active
            REQUIRE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));

            /*
            {
                //check event is discarded
                uint32_t nSequence;
                REQUIRE(LLD::Ledger->ReadSequence(hashGenesis2, nSequence));

                //check for tx
                TAO::Ledger::Transaction txEvent;
                REQUIRE_FALSE(LLD::Ledger->ReadEvent(hashGenesis2, nSequence - 1, txEvent));
            }
            */

            //rollback the transaction
            REQUIRE(Rollback(tx[0]));

            //grab the new state
            {
                State state;
                REQUIRE(LLD::Register->ReadState(hashRegister, state));

                //check owner
                REQUIRE(state.hashOwner.GetType() == 0);

                //check for the proof
                REQUIRE_FALSE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));
            }

            {
                //check event is back
                uint32_t nSequence;
                REQUIRE(LLD::Ledger->ReadSequence(hashGenesis2, nSequence));

                //check for tx
                TAO::Ledger::Transaction txEvent;
                REQUIRE(LLD::Ledger->ReadEvent(hashGenesis2, nSequence - 1, txEvent));

                //check the hashes match
                REQUIRE(txEvent.GetHash() == hashTx);
            }
        }


        //simulate original user finally getting their claim
        {

            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis2;
            tx.nSequence   = 1;
            tx.nTimestamp  = runtime::timestamp();


            //payload
            tx[0] << uint8_t(OP::CLAIM) << hashTx << uint32_t(0) << hashRegister;

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //write transaction
            REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check that register exists
            {
                State state;
                REQUIRE(LLD::Register->ReadState(hashRegister, state));

                //check owner
                REQUIRE(state.hashOwner == hashGenesis2);
            }

            //check proof is active
            REQUIRE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));

            //grab the new state
            {
                State state;
                REQUIRE(LLD::Register->ReadState(hashRegister, state));

                //check owner
                REQUIRE(state.hashOwner == hashGenesis2);
            }

            {
                //check for event
                uint32_t nSequence;
                REQUIRE(LLD::Ledger->ReadSequence(hashGenesis2, nSequence));

                //check for tx
                TAO::Ledger::Transaction txEvent;
                REQUIRE(LLD::Ledger->ReadEvent(hashGenesis2, nSequence - 1, txEvent));

                //check the hashes match
                REQUIRE(txEvent.GetHash() == hashTx);
            }
        }
    }


    //rollback a debit from token
    {
        //create object
        uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::TOKEN);
        uint256_t hashAccount  = TAO::Register::Address(TAO::Register::Address::ACCOUNT);
        uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);

        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();

            //create object
            Object token = CreateToken(hashRegister, 1000, 100);

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::OBJECT) << token.GetState();

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));
        }


        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 1;
            tx.nTimestamp  = runtime::timestamp();

            //create object
            Object account = CreateAccount(hashRegister);

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashAccount << uint8_t(REGISTER::OBJECT) << account.GetState();

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));
        }


        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 2;
            tx.nTimestamp  = runtime::timestamp();


            //payload
            tx[0] << uint8_t(OP::DEBIT) << hashRegister << hashAccount << uint64_t(500) << uint64_t(0);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //write transaction
            REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));
            REQUIRE(LLD::Ledger->IndexBlock(tx.GetHash(), TAO::Ledger::ChainState::Genesis()));

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check register values
            {
                Object token;
                REQUIRE(LLD::Register->ReadState(hashRegister, token));

                //parse register
                REQUIRE(token.Parse());

                //check balance
                REQUIRE(token.get<uint64_t>("balance") == 500);
            }

            //rollback
            REQUIRE(Rollback(tx[0]));

            //check register values
            {
                Object token;
                REQUIRE(LLD::Register->ReadState(hashRegister, token));

                //parse register
                REQUIRE(token.Parse());

                //check balance
                REQUIRE(token.get<uint64_t>("balance") == 1000);
            }
        }


        //rollback a credit from token
        {
            //create object
            uint256_t hashRegister = TAO::Register::Address(TAO::Register::Address::TOKEN);
            uint256_t hashAccount  = TAO::Register::Address(TAO::Register::Address::ACCOUNT);
            uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);
            uint256_t hashGenesis2 = TAO::Ledger::Genesis(Util::GetRand256(), true);

            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis;
                tx.nSequence   = 0;
                tx.nTimestamp  = runtime::timestamp();

                //create object
                Object token = CreateToken(hashRegister, 1000, 100);

                //payload
                tx[0] << uint8_t(OP::CREATE) << hashRegister << uint8_t(REGISTER::OBJECT) << token.GetState();

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //commit to disk
                REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));
            }


            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis2;
                tx.nSequence   = 0;
                tx.nTimestamp  = runtime::timestamp();

                //create object
                Object account = CreateAccount(hashRegister);

                //payload
                tx[0] << uint8_t(OP::CREATE) << hashAccount << uint8_t(REGISTER::OBJECT) << account.GetState();

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //commit to disk
                REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));
            }


            uint512_t hashTx;
            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis;
                tx.nSequence   = 1;
                tx.nTimestamp  = runtime::timestamp();


                //payload
                tx[0] << uint8_t(OP::DEBIT) << hashRegister << hashAccount << uint64_t(500) << uint64_t(0);

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //write transaction
                REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));
                REQUIRE(LLD::Ledger->IndexBlock(tx.GetHash(), TAO::Ledger::ChainState::Genesis()));

                //commit to disk
                REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

                //set hash
                hashTx = tx.GetHash();

                //check register values
                {
                    Object account;
                    REQUIRE(LLD::Register->ReadState(hashRegister, account));

                    //parse register
                    REQUIRE(account.Parse());

                    //check balance
                    REQUIRE(account.get<uint64_t>("balance") == 500);
                }
            }


            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis2;
                tx.nSequence   = 2;
                tx.nTimestamp  = runtime::timestamp();

                //payload
                tx[0] << uint8_t(OP::CREDIT) << hashTx << uint32_t(0) << hashAccount << hashRegister << uint64_t(500);

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //commit to disk
                REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

                //check register values
                {
                    Object account;
                    REQUIRE(LLD::Register->ReadState(hashAccount, account));

                    //parse register
                    REQUIRE(account.Parse());

                    //check balance
                    REQUIRE(account.get<uint64_t>("balance") == 500);

                    //check that proofs are established
                    REQUIRE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));
                }

                //rollback
                REQUIRE(Rollback(tx[0]));

                //check register values
                {
                    Object account;
                    REQUIRE(LLD::Register->ReadState(hashAccount, account));

                    //parse register
                    REQUIRE(account.Parse());

                    //check balance
                    REQUIRE(account.get<uint64_t>("balance") == 0);

                    //check that proofs are removed
                    REQUIRE_FALSE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));
                }
            }


            //attempt to to send back to self
            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis;
                tx.nSequence   = 3;
                tx.nTimestamp  = runtime::timestamp();

                //payload
                tx[0] << uint8_t(OP::CREDIT) << hashTx << uint32_t(0) << hashRegister << hashRegister << uint64_t(500);

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //commit to disk
                REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

                //check register values
                {
                    Object account;
                    REQUIRE(LLD::Register->ReadState(hashRegister, account));

                    //parse register
                    REQUIRE(account.Parse());

                    //check balance
                    REQUIRE(account.get<uint64_t>("balance") == 1000);

                    //check that proofs are removed
                    REQUIRE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));
                }

                //rollback
                REQUIRE(Rollback(tx[0]));

                //check register values
                {
                    Object account;
                    REQUIRE(LLD::Register->ReadState(hashRegister, account));

                    //parse register
                    REQUIRE(account.Parse());

                    //check balance
                    REQUIRE(account.get<uint64_t>("balance") == 500);

                    //check that proofs are removed
                    REQUIRE_FALSE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));

                    {
                        //check event is discarded
                        uint32_t nSequence;
                        REQUIRE(LLD::Ledger->ReadSequence(hashGenesis2, nSequence));

                        //check for tx
                        TAO::Ledger::Transaction txEvent;
                        REQUIRE(LLD::Ledger->ReadEvent(hashGenesis2, nSequence - 1, txEvent));

                        //make sure hashes match
                        REQUIRE(txEvent.GetHash() == hashTx);
                    }
                }
            }


            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis2;
                tx.nSequence   = 3;
                tx.nTimestamp  = runtime::timestamp();

                //payload
                tx[0] << uint8_t(OP::CREDIT) << hashTx << uint32_t(0) << hashAccount << hashRegister << uint64_t(500);

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //commit to disk
                REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

                //check register values
                {
                    Object account;
                    REQUIRE(LLD::Register->ReadState(hashAccount, account));

                    //parse register
                    REQUIRE(account.Parse());

                    //check balance
                    REQUIRE(account.get<uint64_t>("balance") == 500);

                    //check that proofs are established
                    REQUIRE(LLD::Ledger->HasProof(hashRegister, hashTx, 0));
                }
            }


            //attempt to double spend
            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis2;
                tx.nSequence   = 3;
                tx.nTimestamp  = runtime::timestamp();

                //payload
                tx[0] << uint8_t(OP::CREDIT) << hashTx << uint32_t(0) << hashAccount << hashRegister << uint64_t(500);

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //make sure it fails
                REQUIRE_FALSE(Execute(tx[0], TAO::Ledger::FLAGS::MEMPOOL));
            }


            //attempt to double spend back to self
            {
                //create the transaction object
                TAO::Ledger::Transaction tx;
                tx.hashGenesis = hashGenesis;
                tx.nSequence   = 3;
                tx.nTimestamp  = runtime::timestamp();

                //payload
                tx[0] << uint8_t(OP::CREDIT) << hashTx << uint32_t(0) << hashRegister << hashRegister << uint64_t(500);

                //generate the prestates and poststates
                REQUIRE(tx.Build());

                //verify the prestates and poststates
                REQUIRE(tx.Verify());

                //make sure it fails
                REQUIRE_FALSE(Execute(tx[0], TAO::Ledger::FLAGS::MEMPOOL));
            }
        }
    }
}

TEST_CASE( "Register Rollback Tests - Stake operations", "[register]")
{
    using namespace TAO::Register;
    using namespace TAO::Operation;

    TAO::Ledger::BlockState state;
    state.nHeight = 150;

    REQUIRE(LLD::Ledger->WriteBlock(state.GetHash(), state));

    {
        //create new user and get trust account address
        uint256_t hashGenesis  = TAO::Ledger::Genesis(Util::GetRand256(), true);
        uint256_t hashTrust = TAO::Register::Address(std::string("trust"), hashGenesis, TAO::Register::Address::TRUST);

        uint512_t hashCoinbaseTx = 0;
        uint512_t hashLastTrust = Util::GetRand512();

        //Create coinbase tx to mint some NXS
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 0;
            tx.nTimestamp  = runtime::timestamp();


            //payload (hashGenesis, coinbase reward, extra nonce)
            tx[0] << uint8_t(OP::COINBASE) << hashGenesis << uint64_t(5000) << (uint64_t)0;

            //write transaction
            REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));
            REQUIRE(LLD::Ledger->IndexBlock(tx.GetHash(), TAO::Ledger::ChainState::Genesis()));

            //write index
            REQUIRE(LLD::Ledger->IndexBlock(tx.GetHash(), state.GetHash()));

            //set the hash
            hashCoinbaseTx = tx.GetHash();
        }


        //Create a trust register
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 1;
            tx.nTimestamp  = runtime::timestamp();

            //create object
            Object trustRegister = CreateTrust();

            //payload
            tx[0] << uint8_t(OP::CREATE) << hashTrust << uint8_t(REGISTER::OBJECT) << trustRegister.GetState();

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check trust account initial values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadState(hashTrust, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 0);
                REQUIRE(trust.get<uint64_t>("trust")   == 0);
                REQUIRE(trust.get<uint64_t>("stake")   == 0);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }


        //Add balance to trust account by crediting from Coinbase tx
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 2;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::CREDIT) << hashCoinbaseTx << uint32_t(0) << hashTrust << hashGenesis << uint64_t(5000);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadState(hashTrust, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check balance (claimed Coinbase amount added to balance)
                REQUIRE(trust.get<uint64_t>("balance") == 5000);
                REQUIRE(trust.get<uint64_t>("trust")   == 0);
                REQUIRE(trust.get<uint64_t>("stake")   == 0);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }


        //OP::GENESIS rollback
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 3;
            tx.nTimestamp  = runtime::timestamp();

            //payload with coinstake reward
            tx[0] << uint8_t(OP::GENESIS) << uint64_t(5);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check register values
            {
                //check added to trust index
                REQUIRE(LLD::Register->HasTrust(hashGenesis));

                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 5);
                REQUIRE(trust.get<uint64_t>("trust")   == 0);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }

            //rollback the genesis
            REQUIRE(Rollback(tx[0]));

            //check register values
            {
                //check removed from trust index
                REQUIRE_FALSE(LLD::Register->HasTrust(hashGenesis));

                Object trust;
                REQUIRE(LLD::Register->ReadState(hashTrust, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 5000);
                REQUIRE(trust.get<uint64_t>("trust")   == 0);
                REQUIRE(trust.get<uint64_t>("stake")   == 0);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }


        //handle an OP::GENESIS
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 4;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::GENESIS) << uint64_t(6);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //set trust hash
            hashLastTrust = tx.GetHash();

            //check register values
            {
                //check added to trust index
                REQUIRE(LLD::Register->HasTrust(hashGenesis));

                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 6);
                REQUIRE(trust.get<uint64_t>("trust")   == 0);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }


        //OP::TRUST rollback
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 5;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::TRUST) << hashLastTrust << uint64_t(555) << int64_t(0) << uint64_t(7);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 13);
                REQUIRE(trust.get<uint64_t>("trust")   == 555);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }

            //rollback
            Rollback(tx[0]);

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 6);
                REQUIRE(trust.get<uint64_t>("trust")   == 0);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }


        //handle an OP::TRUST
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 6;
            tx.nTimestamp  = runtime::timestamp();

            //payload
            tx[0] << uint8_t(OP::TRUST) << hashLastTrust << uint64_t(2000) << int64_t(0) << uint64_t(10);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            hashLastTrust = tx.GetHash();

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 16);
                REQUIRE(trust.get<uint64_t>("trust")   == 2000);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);

                Object trust2;
                REQUIRE(LLD::Register->ReadState(hashTrust, trust2));

                //parse register
                REQUIRE(trust2.Parse());

                //check register
                REQUIRE(trust2.get<uint64_t>("balance") == 16);
                REQUIRE(trust2.get<uint64_t>("trust")   == 2000);
                REQUIRE(trust2.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust2.get<uint256_t>("token")  == 0);
            }
        }


        //OP::TRUST with add stake rollback
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 7;
            tx.nTimestamp  = runtime::timestamp();

            //payload containing trust, reward, and add 15 from balance to stake
            tx[0] << uint8_t(OP::TRUST) << hashLastTrust << uint64_t(3000) << int64_t(15) << uint64_t(8);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 9);
                REQUIRE(trust.get<uint64_t>("trust")   == 3000);
                REQUIRE(trust.get<uint64_t>("stake")   == 5015);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }

            //rollback the stake
            REQUIRE(Rollback(tx[0]));

            //verify rollback
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 16);
                REQUIRE(trust.get<uint64_t>("trust")   == 2000);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }


        //OP::TRUST with unstake rollback
        {
            //create the transaction object
            TAO::Ledger::Transaction tx;
            tx.hashGenesis = hashGenesis;
            tx.nSequence   = 8;
            tx.nTimestamp  = runtime::timestamp();

            //payload containing trust (after penalty), reward, and remove 2000 from stake to balance
            tx[0] << uint8_t(OP::TRUST) << hashLastTrust << uint64_t(1200) << int64_t(-2000) << uint64_t(6);

            //generate the prestates and poststates
            REQUIRE(tx.Build());

            //verify the prestates and poststates
            REQUIRE(tx.Verify());

            //commit to disk
            REQUIRE(Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 2022);
                REQUIRE(trust.get<uint64_t>("trust")   == 1200);
                REQUIRE(trust.get<uint64_t>("stake")   == 3000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }

            //rollback
            Rollback(tx[0]);

            //check register values
            {
                Object trust;
                REQUIRE(LLD::Register->ReadTrust(hashGenesis, trust));

                //parse register
                REQUIRE(trust.Parse());

                //check register
                REQUIRE(trust.get<uint64_t>("balance") == 16);
                REQUIRE(trust.get<uint64_t>("trust")   == 2000);
                REQUIRE(trust.get<uint64_t>("stake")   == 5000);
                REQUIRE(trust.get<uint256_t>("token")  == 0);
            }
        }
    }
}
