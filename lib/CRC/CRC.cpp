#include "CRC.h"

unsigned int CRC32(const unsigned char *buf, unsigned int size)
{
    unsigned int i, crc;
    crc = 0xFFFFFFFF;
    for (i = 0; i < size; i++)
        crc = crc32tab[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFF;
}