/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2018] ++

            (c) Copyright The Nexus Developers 2014 - 2018

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#ifndef NEXUS_LLP_PACKETS_TRITIUM_H
#define NEXUS_LLP_PACKETS_TRITIUM_H

#include <vector>
#include <limits>
#include <stdint>

#include <LLC/hash/SK.h>

namespace LLP
{

    /** Class to handle sending and receiving of More Complese Message LLP Packets. **/
    class TritiumPacket
    {
    public:

        /*
        * Components of a Message LLP Packet.
        * BYTE 0 - 4    : Start
        * BYTE 5 - 17   : Message
        * BYTE 18 - 22  : Size
        * BYTE 23 - 26  : Checksum
        * BYTE 26 - X   : Data
        *
        */
        uint16_t        MESSAGE;
        uint32_t	    LENGTH;
        uint32_t	    CHECKSUM;

        std::vector<uint8_t> DATA;

        TritiumPacket()
        {
            SetNull();
            SetHeader();
        }

        TritiumPacket(uint16_t nMessage)
        {
            SetNull();

            MESSAGE = nMessage;
        }

        IMPLEMENT_SERIALIZE
        (
            READWRITE(MESSAGE);
            READWRITE(LENGTH);
            READWRITE(CHECKSUM);
        )


        /* Set the Packet Null Flags. */
        void SetNull()
        {
            MESSAGE   = std::numeric_limits<uint8_t>();
            LENGTH    = 0;
            CHECKSUM  = 0;

            DATA.clear();
        }


        /* Packet Null Flag. Length and Checksum both 0. */
        bool IsNull() { return (MESSAGE == std::numeric_limits<uint8_t>() && LENGTH == 0 && CHECKSUM == 0 && DATA.size() == 0); }


        /* Determine if a packet is fully read. */
        bool Complete() { return (Header() && DATA.size() == LENGTH); }


        /* Determine if header is fully read */
        bool Header()   { return IsNull() ? false : (CHECKSUM > 0 && MESSAGE != std::numeric_limits<uint8_t>()); }


        /* Sets the size of the packet from Byte Vector. */
        void SetLength(std::vector<uint8_t> vBytes)
        {
            CDataStream ssLength(vBytes, SER_NETWORK, MIN_PROTO_VERSION);
            ssLength >> LENGTH;
        }


        /* Set the Packet Checksum Data. */
        void SetChecksum()
        {
            CHECKSUM = LLC::SK32(DATA.begin(), DATA.end());
        }


        /* Set the Packet Data. */
        void SetData(CDataStream ssData)
        {
            std::vector<uint8_t> vData(ssData.begin(), ssData.end());

            LENGTH = vData.size();
            DATA   = vData;

            SetChecksum();
        }


        /* Check the Validity of the Packet. */
        bool IsValid()
        {
            /* Check that the packet isn't NULL. */
            if(IsNull())
                return false;

            /* Make sure Packet length is within bounds. (Max 2 MB Packet Size) */
            if (LENGTH > (1024 * 1024 * 2))
                return error("Tritium Packet (%s, %u bytes) : Message too Large", MESSAGE, LENGTH);

            /* Double check the Message Checksum. */
            if (LLC::SK32(DATA.begin(), DATA.end()) != CHECKSUM)
                return error("Tritium Packet (%s, %u bytes) : CHECKSUM MISMATCH nChecksum=%u hdr.nChecksum=%u",
                MESSAGE, LENGTH, nChecksum, CHECKSUM);

            return true;
        }


        /* Serializes class into a Byte Vector. Used to write Packet to Sockets. */
        std::vector<uint8_t> GetBytes()
        {
            CDataStream ssHeader(SER_NETWORK, MIN_PROTO_VERSION);
            ssHeader << *this;

            std::vector<uint8_t> vBytes(ssHeader.begin(), ssHeader.end());
            vBytes.insert(vBytes.end(), vData.begin(), vData.end());

            return vBytes;
        }
    };
}

#endif
