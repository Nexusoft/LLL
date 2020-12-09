/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Common/include/random.h>

#include <LLD/include/global.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>
#include <TAO/Operation/include/enum.h>

#include <TAO/Register/include/create.h>
#include <TAO/Register/types/address.h>

#include <TAO/Ledger/types/transaction.h>
#include <TAO/Ledger/include/chainstate.h>

#include <unit/catch2/catch.hpp>



TEST_CASE("Claim Primitive Tests", "[operation]")
{
    //create object
    uint256_t hashAsset  = TAO::Register::Address(TAO::Register::Address::OBJECT);
    uint256_t hashGenesis  = Common::GetRand256();
    hashGenesis.SetType(TAO::Ledger::GENESIS::TESTNET);

    uint256_t hashGenesis2  = Common::GetRand256();
    hashGenesis2.SetType(TAO::Ledger::GENESIS::TESTNET);

    // create an asset
    {
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = hashGenesis;
        tx.nSequence   = 1;
        tx.nTimestamp  = runtime::timestamp();

        //create asset
        TAO::Register::Object asset = TAO::Register::CreateAsset();

        // add some data
        asset << std::string("data") << uint8_t(TAO::Register::TYPES::STRING) << std::string("somedata");

        //payload
        tx[0] << uint8_t(TAO::Operation::OP::CREATE) << hashAsset << uint8_t(TAO::Register::REGISTER::OBJECT) << asset.GetState();

        /* verify prestates and poststates, write tx, commit to disk */
        REQUIRE(tx.Build());
        REQUIRE(tx.Verify());
        REQUIRE(TAO::Operation::Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

        /* check register values */
        {
            TAO::Register::Object asset;
            REQUIRE(LLD::Register->ReadState(hashAsset, asset));

            /* check that the asset belongs to the original owner. */
            REQUIRE(asset.hashOwner == hashGenesis);
        }
    }

    /* Transfer the asset to another sigchain owner */
    {
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = hashGenesis;
        tx.nSequence   = 2;
        tx.nTimestamp  = runtime::timestamp();

        TAO::Ledger::Transaction tx2;
        tx2.hashGenesis = hashGenesis2;
        tx2.nSequence   = 1;
        tx2.nTimestamp  = runtime::timestamp();

        /* transfer payload */
        tx[0] << uint8_t(TAO::Operation::OP::TRANSFER) << hashAsset << hashGenesis2 << uint8_t(TAO::Operation::TRANSFER::CLAIM);

        /* verify prestates and poststates, write tx, commit to disk */
        REQUIRE(tx.Build());
        REQUIRE(tx.Verify());
        REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));
        REQUIRE(TAO::Operation::Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

        //check the new claim owner
        {
            TAO::Register::Object asset;
            REQUIRE(LLD::Register->ReadState(hashAsset, asset));

            //check asset owner
            REQUIRE(asset.hashOwner.GetType() == 0);

            //check the transfer owner
            uint256_t hashCheck = hashGenesis;
            hashCheck.SetType(TAO::Ledger::GENESIS::SYSTEM);

            //check owner
            REQUIRE(asset.hashOwner == hashCheck);

            //ensure asset isn't previous owner
            REQUIRE_FALSE(asset.hashOwner == hashGenesis);
        }

        /* claim payload */
        tx2[0] << uint8_t(TAO::Operation::OP::CLAIM) << tx.GetHash() << uint32_t(0) << hashAsset;

        /* verify prestates and poststates, write tx, commit to disk */
        REQUIRE(tx2.Build());
        REQUIRE(tx2.Verify());
        REQUIRE(LLD::Ledger->WriteTx(tx2.GetHash(), tx2));
        REQUIRE(LLD::Ledger->IndexBlock(tx.GetHash(), TAO::Ledger::ChainState::Genesis()));
        REQUIRE(TAO::Operation::Execute(tx2[0], TAO::Ledger::FLAGS::BLOCK));

        /* check register values */
        {
            TAO::Register::Object asset;
            REQUIRE(LLD::Register->ReadState(hashAsset, asset));

            /* check that the asset belongs to the new owner. */
            REQUIRE(asset.hashOwner == hashGenesis2);
        }
    }

    /* Attempt to claim asset with transfer force flag (instead of claim). */
    {
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = hashGenesis2;
        tx.nSequence   = 2;
        tx.nTimestamp  = runtime::timestamp();

        TAO::Ledger::Transaction tx2;
        tx2.hashGenesis = hashGenesis;
        tx2.nSequence   = 3;
        tx2.nTimestamp  = runtime::timestamp();

        /* transfer payload */
        tx[0] << uint8_t(TAO::Operation::OP::TRANSFER) << hashAsset << hashGenesis << uint8_t(TAO::Operation::TRANSFER::FORCE);

        /* verify prestates and poststates, write tx, commit to disk */
        REQUIRE(tx.Build());
        REQUIRE(tx.Verify());
        REQUIRE(LLD::Ledger->WriteTx(tx.GetHash(), tx));
        REQUIRE(TAO::Operation::Execute(tx[0], TAO::Ledger::FLAGS::BLOCK));

        /* claim payload */
        tx2[0] << uint8_t(TAO::Operation::OP::CLAIM) << tx.GetHash() << uint32_t(0) << hashAsset;

        /* verify prestates and poststates*/
        REQUIRE_FALSE(tx2.Build());
        REQUIRE_FALSE(tx2.Verify());

    }
}
