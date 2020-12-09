/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include "util.h"

#include <Util/include/random.h>

#include <LLD/include/global.h>

#include <TAO/Ledger/types/mempool.h>
#include <TAO/Operation/include/execute.h>

#include <unit/catch2/catch.hpp>

/* Dont use the global USERNAME / SESSION for these tests as we can't be sure that it hasn't already been created by 
    one of the other API tests, due to the unknown order that the tests are run in */
std::string strUsername = "USER" +std::to_string(Util::GetRand());
std::string strSession = "";
std::string strRecovery = "this is the recovery seed that i want to use";


TEST_CASE( "Test Users API", "[API/users]")
{
    /* Declare variables shared across test cases */
    json::json params;
    json::json ret;
    json::json result;
    json::json error;
    
    /* Enure that we use low argon2 requirements for unit test to speed up the use of the sig chain */
    config::SoftSetArg("-argon2", "0");
    config::SoftSetArg("-argon2_memory", "0");

    /* Ensure User1 is logged out before we start testing.  This is only an issue when not in multiuser mode */
    if(!config::fMultiuser.load())
        LogoutUser(GENESIS1, SESSION1);


    /* Test failure with username too short */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = "X";
        params["password"] = "X";
        params["pin"] = "X";

        /* Invoke the API */
        ret = APICall("users/create/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -191);
    }

    /* Test failure with password too short */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "X";
        params["pin"] = "X";

        /* Invoke the API */
        ret = APICall("users/create/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -192);
    }

    /* Test failure with pin too short */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = PASSWORD;
        params["pin"] = "X";

        /* Invoke the API */
        ret = APICall("users/create/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -193);
    }

    /* Test creating a new user */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = PASSWORD;
        params["pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/create/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("genesis") != result.end());
        REQUIRE(result.find("hash") != result.end());
        REQUIRE(result.find("nexthash") != result.end());
        REQUIRE(result.find("prevhash") != result.end());
        REQUIRE(result.find("pubkey") != result.end());
        REQUIRE(result.find("signature") != result.end());
        REQUIRE(result.find("sequence") != result.end());
        REQUIRE(result.find("timestamp") != result.end());
        REQUIRE(result.find("version") != result.end());

        /* Write sig chain genesis transaction to disk */
        uint256_t hashGenesis( result["genesis"].get<std::string>());
        uint512_t txid( result["hash"].get<std::string>());

        REQUIRE(LLD::Ledger->WriteGenesis(hashGenesis, txid));

        TAO::Ledger::Transaction tx;
        REQUIRE(TAO::Ledger::mempool.Get(txid, tx));

        REQUIRE(LLD::Ledger->WriteTx(txid, tx));
        REQUIRE(LLD::Ledger->WriteLast(hashGenesis, txid));

        REQUIRE(tx.Verify());
        
        for(uint32_t nContract = 0; nContract < tx.Size(); nContract++)
        {
            REQUIRE(TAO::Operation::Execute(tx[nContract], TAO::Ledger::FLAGS::BLOCK));
        }

        REQUIRE(TAO::Ledger::mempool.Remove(tx.GetHash()));

    }

    /* Test failing if user already exists */
    {
        /* Params are already populated from the previous call so just invoke the API again */
        ret = APICall("users/create/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -130);
    }

    /* Test login failure */
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "wrongpass";
        params["pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/login/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("error") != ret.end());

    }

    /* Test login success */
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = PASSWORD;
        params["pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/login/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("genesis") != result.end());

        if(config::fMultiuser.load())
        {
            REQUIRE(result.find("session") != result.end());

            /* Grab the session ID for future calls */
            strSession = result["session"].get<std::string>();
        }
    }

    /* Test unlock fail wrong pin */
    if(!config::fMultiuser.load())
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["pin"] = "5678";

        /* Invoke the API */
        ret = APICall("users/unlock/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -149);
    }

    /* Test unlock */
    if(!config::fMultiuser.load())
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/unlock/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("success") != result.end());
        REQUIRE(result["success"].get<bool>() == true);
    }


    /* Test lock */
    if(!config::fMultiuser.load())
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/lock/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("success") != result.end());
        REQUIRE(result["success"].get<bool>() == true);
    }


    /* Test password change fail, invalid password*/
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = "XXXXXXXX";//PASSWORD;
        params["pin"] = PIN;
        params["new_password"] = PASSWORD + "1";
        params["new_pin"] = PIN + "1";

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -139);
    }


    /* Test password change fail, invalid pin */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = 999999;//PIN;
        params["new_password"] = PASSWORD + "1";
        params["new_pin"] = PIN + "1";

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -139);
    }

    /* Test password change fail, missing new credentials */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -218);
    }


    /* Test password change success */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = PIN;
        params["new_password"] = PASSWORD + "1";
        params["new_pin"] = PIN + "1";

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];
        REQUIRE(result.find("txid") != result.end());

    }


    /* Test logout success */
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;

        /* Invoke the API */
        ret = APICall("users/logout/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("success") != result.end());
        REQUIRE(result["success"].get<bool>() == true);

    }

    /* Test login success with changed credentials*/
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = PASSWORD + "1";
        params["pin"] = PIN + "1";

        /* Invoke the API */
        ret = APICall("users/login/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("genesis") != result.end());

        if(config::fMultiuser.load())
        {
            REQUIRE(result.find("session") != result.end());

            /* Grab the session ID for future calls */
            strSession = result["session"].get<std::string>();
        }
    }

    /* Test password change back to original credentials success */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD + "1";
        params["pin"] = PIN + "1";
        params["new_password"] = PASSWORD;
        params["new_pin"] = PIN;

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];
        REQUIRE(result.find("txid") != result.end());

    }


    /* Test recovery set fail, recovery too short*/
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = PIN;
        params["new_recovery"] = "this recovery is too short";

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -221);
    }

    /* Test recovery set fail, invalid password*/
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = "XXXXXXXX";//PASSWORD;
        params["pin"] = PIN;
        params["new_recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -139);
    }

    /* Test recovery set success */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = PIN;
        params["new_recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];
        REQUIRE(result.find("txid") != result.end());
    }

    /* Test recovery set fail, missing previous recovery seed */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = PIN;
        params["new_recovery"] = "this is the NEW recovery seed that i want to use";

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -220);
    }

    /* Test recovery set success, NOTE the previous recovery must be provided in order to sign the transaction */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["session"] = strSession;
        params["password"] = PASSWORD;
        params["pin"] = PIN;
        params["recovery"] = strRecovery;
        params["new_recovery"] = "this is the NEW recovery seed that i want to use";

        /* Invoke the API */
        ret = APICall("users/update/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];
        REQUIRE(result.find("txid") != result.end());

        strRecovery = "this is the NEW recovery seed that i want to use";
    }

}

TEST_CASE( "Test Users API Recovery User", "[API/users/recover/user]")
{
    /* Declare variables shared across test cases */
    json::json params;
    json::json ret;
    json::json result;
    json::json error;
    
    /* Enure that we use low argon2 requirements for unit test to speed up the use of the sig chain */
    config::SoftSetArg("-argon2", "0");
    config::SoftSetArg("-argon2_memory", "0");

    /* Ensure User1 is logged out before we start testing.  This is only an issue when not in multiuser mode */
    if(!config::fMultiuser.load())
        LogoutUser(GENESIS1, SESSION1);


    /* Test recovery fail with missing recovery seed */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "newpassword";
        params["pin"] = "newpin";
        //params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -220);
    }

    /* Test recovery fail with missing username */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        //params["username"] = strUsername;
        params["password"] = "newpassword";
        params["pin"] = "newpin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -127);
    }

    /* Test recovery fail with missing password */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        //params["password"] = "newpassword";
        params["pin"] = "newpin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -128);
    }

    /* Test recovery fail with missing pin */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "newpassword";
        //params["pin"] = "newpin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -129);
    }

    /* Test recovery fail with invalid username */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = "invalidusername";
        params["password"] = "newpassword";
        params["pin"] = "newpin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -139);
    }

    /* Test recovery fail with password too short */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "short";
        params["pin"] = "newpin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -192);
    }

    /* Test recovery fail with pin too short */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "newpassword";
        params["pin"] = "pin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -193);
    }

    /* Test recovery fail with invalid recovery seed */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "newpassword";
        params["pin"] = "newpin";
        params["recovery"] = "not a recovery seed";

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check response is an error and validate error code */
        REQUIRE(ret.find("error") != ret.end());
        REQUIRE(ret["error"]["code"].get<int32_t>() == -139);
    }

    /* Test recovery success setting new password and pin */
    {

        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "newpassword";
        params["pin"] = "newpin";
        params["recovery"] = strRecovery;

        /* Invoke the API */
        ret = APICall("users/recover/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];
        REQUIRE(result.find("genesis") != result.end());
        REQUIRE(result.find("nexthash") != result.end());
        REQUIRE(result.find("prevhash") != result.end());
        REQUIRE(result.find("pubkey") != result.end());
        REQUIRE(result.find("signature") != result.end());
        REQUIRE(result.find("txid") != result.end());
    }

    /* Test login with new credentials */
    {
        /* Build the parameters to pass to the API */
        params.clear();
        params["username"] = strUsername;
        params["password"] = "newpassword";
        params["pin"] = "newpin";

        /* Invoke the API */
        ret = APICall("users/login/user", params);

        /* Check that the result is as we expect it to be */
        REQUIRE(ret.find("result") != ret.end());
        result = ret["result"];

        REQUIRE(result.find("genesis") != result.end());

        if(config::fMultiuser.load())
        {
            REQUIRE(result.find("session") != result.end());

            /* Grab the session ID for future calls */
            strSession = result["session"].get<std::string>();
        }
    }

}