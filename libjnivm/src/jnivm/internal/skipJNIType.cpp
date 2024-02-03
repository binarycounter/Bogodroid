#include <jnivm/internal/skipJNIType.h>
#include <algorithm>

const char * jnivm::SkipJNIType(const char *cur, const char *end) {
    switch (*cur) {
    case 'V':
            // Void has size 0 ignore it
            break;
    case 'Z':
    case 'B':
    case 'S':
    case 'C':
    case 'I':
    case 'J':
    case 'F':
    case 'D':
            break;
    case '[':
            cur = SkipJNIType(cur + 1, end) - 1;
            break;
    case 'L':
            cur = std::find(cur, end, ';');
            break;
    case '(':
            return SkipJNIType(cur + 1, end);
    }
    return cur + 1;
}