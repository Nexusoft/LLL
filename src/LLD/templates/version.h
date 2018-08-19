/*__________________________________________________________________________________________

			(c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2017] ++

			(c) Copyright The Nexus Developers 2014 - 2017

			Distributed under the MIT software license, see the accompanying
			file COPYING or http://www.opensource.org/licenses/mit-license.php.

			"ad vocem populi" - To The Voice of The People

____________________________________________________________________________________________*/


#ifndef NEXUS_LLD_INCLUDE_VERSION_H
#define NEXUS_LLD_INCLUDE_VERSION_H


#define DATABASE_MAJOR       0
#define DATABASE_MINOR       1
#define DATABASE_PATCH       1
#define DATABASE_BUILD       0


/* Used for features in the database. */
const int DATABASE_VERSION =
                    1000000 * DATABASE_MAJOR
                  +   10000 * DATABASE_MINOR
                  +     100 * DATABASE_PATCH
                  +       1 * DATABASE_BUILD;


/* The database type used (Berklee DB or Lower Level Database) */
#ifdef USE_LLD
const std::string DATABASE_NAME("LLD");
#else
const std::string DATABASE_NAME("BDB");
#endif


#endif
