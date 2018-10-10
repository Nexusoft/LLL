/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2018] ++

            (c) Copyright The Nexus Developers 2014 - 2018

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Util/include/args.h>
#include <Util/include/config.h>
#include <Util/include/signals.h>
#include <Util/include/convert.h>
#include <Util/include/runtime.h>

#include <LLC/include/random.h>

#include <LLD/templates/sector.h>
#include <LLD/templates/hashmap.h>
#include <LLD/templates/filemap.h>

#include <LLP/include/tritium.h>
#include <LLP/templates/server.h>

#include <TAO/Ledger/types/transaction.h>

 
class TestDB : public LLD::SectorDatabase<LLD::BinaryHashMap>
{
public:
    TestDB(const char* pszMode="r+") : SectorDatabase("testdb", pszMode) {}

    bool WriteTx(uint512_t hashTransaction, TAO::Ledger::Transaction tx)
    {
        return Write(std::make_pair(std::string("tx"), hashTransaction), tx);
    }

    bool ReadTx(uint512_t hashTransaction, TAO::Ledger::Transaction& tx)
    {
        return Read(std::make_pair(std::string("tx"), hashTransaction), tx);
    }
};

int main(int argc, char** argv)
{
    /* Handle all the signals with signal handler method. */
    SetupSignals();



    /* Parse out the parameters */
    ParseParameters(argc, argv);


    /* Create directories if they don't exist yet. */
    if(boost::filesystem::create_directories(GetDataDir(false)))
        printf(FUNCTION "Generated Path %s\n", __PRETTY_FUNCTION__, GetDataDir(false).c_str());


    /* Read the configuration file. */
    ReadConfigFile(mapArgs, mapMultiArgs);



    /* Create the database instances. */
    //LLD::regDB = new LLD::RegisterDB("r+");
    //LLD::legDB = new LLD::LedgerDB("r+");


    /* Handle Commandline switch */
    for (int i = 1; i < argc; i++)
    {
        if (!IsSwitchChar(argv[i][0]) && !(strlen(argv[i]) >= 7 && strncasecmp(argv[i], "Nexus:", 7) == 0))
        {
            //int ret = Net::CommandLineRPC(argc, argv);
            //exit(ret);
        }
    }


    //LLD::MemCachePool* cachePool = new LLD::MemCachePool(1024 * 1024 * 2048);

    TestDB* test = new TestDB();

    uint512_t  hashTest("c861dffe8d1f5f59c05b726546b05a1e57742004317519a4dee454dcefb3f838c4005625d4799646aac8694aad41a9c447686d26da05a95fe5d20ce7ce979962");

    //TAO::Ledger::Transaction tx;
    //if(!test->ReadTx(hashTest, tx))
    //    return error("FAILED");

    //tx.print();


    int nCounter = 0;
    Timer timer;
    timer.Start();
    while(!fShutdown)
    {
        TAO::Ledger::Transaction tx;
        tx.hashGenesis = LLC::GetRand256();
        uint512_t hash = tx.GetHash();

        test->WriteTx(hash, tx);

        TAO::Ledger::Transaction tx1;
        test->ReadTx(hash, tx1);

        //tx1.print();
        //Sleep(10);

        if(nCounter % 100000 == 0)
        {
            printf("100k records written in %u ms\n", timer.ElapsedMilliseconds());
            timer.Reset();
        }

        nCounter++;
    }


    /* Create an LLP Server.
    LLP::Server<LLP::TritiumNode>* SERVER = new LLP::Server<LLP::TritiumNode>(1111, 10, 30, false, 0, 0, 60, GetBoolArg("-listen", true), true);

    if(mapMultiArgs["-addnode"].size() > 0)
        for(auto node : mapMultiArgs["-addnode"])
            SERVER->AddConnection(node, 1111);

    while(!fShutdown)
    {
        Sleep(1000);
    }

    */


    return 0;
}
