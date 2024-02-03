#include <jnivm/internal/jValuesfromValist.h>
#include <jnivm/internal/skipJNIType.h>
#include <cstring>
#include <string>
#include <stdexcept>
#include <algorithm>

std::vector<jvalue> jnivm::JValuesfromValist(va_list list, const char* signature) {
	auto org = signature;
	const char* end = signature + strlen(signature);
	std::vector<jvalue> values;
	if(signature[0] != '(') {
		throw std::invalid_argument("Signature doesn't begin with '(' " + std::string(signature));
	}
	signature++;
	for(size_t i = 0; *signature != ')' && signature != end; ++i) {
		values.emplace_back();
		switch (*signature) {
		case 'V':
				// Void has size 0 ignore it
				break;
		case 'Z':
				// These are promoted to int (gcc warning)
				values.back().z = (jboolean)va_arg(list, int);
				break;
		case 'B':
				// These are promoted to int (gcc warning)
				values.back().b = (jbyte)va_arg(list, int);
				break;
		case 'S':
				// These are promoted to int (gcc warning)
				values.back().s = (jshort)va_arg(list, int);
				break;
		case 'C':
				// These are promoted to int (gcc warning)
				values.back().c = (jchar)va_arg(list, int);
				break;
		case 'I':
				values.back().i = va_arg(list, jint);
				break;
		case 'J':
				values.back().j = va_arg(list, jlong);
				break;
		case 'F':
				values.back().f = (jfloat)va_arg(list, jdouble);
				break;
		case 'D':
				values.back().d = va_arg(list, jdouble);
				break;
		case '[':
				signature = jnivm::SkipJNIType(signature + 1, end) - 1;
				values.back().l = va_arg(list, jobject);
				break;
		case 'L':
				signature = std::find(signature, end, ';');
				if(signature == end) {
					throw std::invalid_argument("Signature missing ';' after 'L'");
				}
				values.back().l = va_arg(list, jobject);
				break;
		}
		signature++;
	}
	return values;
}