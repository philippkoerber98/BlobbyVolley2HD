#pragma once
#include <SDL_stdinc.h>
#include <string>
#include <vector>
// Case-insensitive full-name order, with common Latin diacritics folded. Stable
// IDs disambiguate equal names; no locale-dependent server/client disagreement.
inline std::vector<Uint32> spectatorNameKey(const std::string& s)
{
    std::vector<Uint32> key;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    size_t i = 0;
    while (i < s.size())
    {
        Uint32 c = p[i++];
        int n = 0;
        if (c >= 0xc2 && c < 0xe0)
        {
            c &= 31;
            n = 1;
        }
        else if (c >= 0xe0 && c < 0xf0)
        {
            c &= 15;
            n = 2;
        }
        else if (c >= 0xf0)
        {
            c &= 7;
            n = 3;
        }
        while (n-- && i < s.size())
        {
            c = (c << 6) | (p[i++] & 63);
        }
        if (c >= 'A' && c <= 'Z')
        {
            c += 32;
        }
        if (c >= 0xc0 && c <= 0xde && c != 0xd7)
        {
            c += 32;
        }
        if (c >= 0xe0 && c <= 0xe5)
        {
            c = 'a';
        }
        else if (c >= 0xe8 && c <= 0xeb)
        {
            c = 'e';
        }
        else if (c >= 0xec && c <= 0xef)
        {
            c = 'i';
        }
        else if ((c >= 0xf2 && c <= 0xf6) || c == 0xf8)
        {
            c = 'o';
        }
        else if (c >= 0xf9 && c <= 0xfc)
        {
            c = 'u';
        }
        else if (c == 0xf1)
        {
            c = 'n';
        }
        else if (c == 0xe7)
        {
            c = 'c';
        }
        key.push_back(c);
    }
    return key;
}
inline bool spectatorLeft(const std::string& a, int aid, const std::string& b, int bid)
{
    const auto ka = spectatorNameKey(a), kb = spectatorNameKey(b);
    return ka == kb ? aid < bid : ka < kb;
}
