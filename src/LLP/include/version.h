/*__________________________________________________________________________________________

			(c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2017] ++

			(c) Copyright The Nexus Developers 2014 - 2017

			Distributed under the MIT software license, see the accompanying
			file COPYING or http://www.opensource.org/licenses/mit-license.php.

			"ad vocem populi" - To The Voice of The People

____________________________________________________________________________________________*/

#ifndef NEXUS_LLP_INCLUDE_VERSION_H
#define NEXUS_LLP_INCLUDE_VERSION_H

#include <string>

#define PROTOCOL_MAJOR       0
#define PROTOCOL_MINOR       2
#define PROTOCOL_REVISION    0
#define PROTOCOL_BUILD       0


/* Used to determine the features available in the Nexus Network */
const int PROTOCOL_VERSION =
                   1000000 * PROTOCOL_MAJOR
                 +   10000 * PROTOCOL_MINOR
                 +     100 * PROTOCOL_REVISION
                 +       1 * PROTOCOL_BUILD;


/* Used to Lock-Out Nodes that are running a protocol version that is too old,
 * Or to allow certain new protocol changes without confusing Old Nodes. */
const int MIN_PROTO_VERSION = 10000;


/* Used to define the baseline of Tritium Versioning. */
const int MIN_TRITIUM_VERSION = 20000;


/* Version Specifiers. */
std::string FormatFullVersion();
std::string FormatSubVersion();


#endif
