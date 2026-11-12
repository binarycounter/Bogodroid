#include "stringUtil.h"
#include <stdexcept>

using namespace jnivm;

int jnivm::UTFToJCharLength(const char * cur) {
    if(!cur) {
        throw std::runtime_error("UtfDataFormatError");
    }
    if((*cur & 0b10000000) == 0) {
        // Single ascii char
        return 1;
    } else if((*cur & 0b11100000) == 0b11000000) {
        // unicode char pair
        if((cur[1] & 0b11000000) != 0b10000000) {
            throw std::runtime_error("UtfDataFormatError");
        }
        return 2;
    } else if((*cur & 0b11110000) == 0b11100000) {
        // unicode char 3 tuple
        if((cur[1] & 0b11000000) != 0b10000000) {
            throw std::runtime_error("UtfDataFormatError");
        }
        if((cur[2] & 0b11000000) != 0b10000000) {
            throw std::runtime_error("UtfDataFormatError");
        }
        return 3;
    } else {
        throw std::runtime_error("UtfDataFormatError");
    }
}

jchar jnivm::UTFToJChar(const char * cur, int& size) {
    size = UTFToJCharLength(cur);
    switch (size)
    {
    case 1:
        return (jchar) *cur;
    case 2:
        return (jchar) (((*cur & 0x1F) << 6) | (cur[1] & 0x3F));
    case 3:
        return (jchar) (((*cur & 0x0F) << 12) | ((cur[1] & 0x3F) << 6) | (cur[2] & 0x3F));
    default:
        throw std::runtime_error("UtfDataFormatError");
    }
}

int jnivm::JCharToUTFLength(jchar c) {
    if(c == 0) {
        return 2;
    } else if((c & (0b10000000 - 1)) == c) {
        return 1;
    } else if((c & ((1 << (5 + 6)) - 1)) == c) {
        return 2;
    } else {
        return 3;
    }
}

int jnivm::JCharToUTF(jchar c, char* cur, int len) {
    int size = JCharToUTFLength(c);
    if(size > len) {
        throw std::runtime_error("End of String");
    }
    switch (size)
    {
    case 1:
        cur[0] = (char) c;
        break;
    case 2:
        cur[0] = (char) 0b11000000 | ((c >> 6) & 0x1F);
        cur[1] = (char) 0b10000000 | (c & 0x3F);
        break;
    case 3:
        cur[0] = (char) 0b11100000 | ((c >> 12) & 0x0F);
        cur[1] = (char) 0b10000000 | ((c >> 6) & 0x3F);
        cur[2] = (char) 0b10000000 | (c & 0x3F);
        break;
    }
    return size;
}