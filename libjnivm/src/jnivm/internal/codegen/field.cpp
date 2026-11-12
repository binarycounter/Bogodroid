#include <jnivm/field.h>
#include <jnivm/internal/codegen/parseJNIType.h>
#include <sstream>
#include <regex>
using namespace jnivm;

std::string Field::GenerateHeader() {
	std::ostringstream ss;
	std::string ctype;
	ParseJNIType(type.data(), type.data() + type.length(), ctype);
	if (_static) {
		ss << "static ";
	}
	ss << ctype << " " << name << ";";
	return ss.str();
}

std::string Field::GenerateStubs(std::string scope, const std::string &cname) {
	if(!_static) return std::string();
	std::ostringstream ss;
	std::string rettype;
	ParseJNIType(type.data(), type.data() + type.length(), rettype);
	ss << rettype << " " << scope << "::" << name << " = {};\n\n";
	return ss.str();
}

std::string Field::GenerateJNIBinding(std::string scope, const std::string &cname) {
	std::ostringstream ss;
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "c->Hook(env, \"" << name << "\", ";
		auto cl = scope;
		scope += "::" + name;
		ss << "&" << scope;
		ss << ");\n";
	} else {
		ss << "{FakeJni::Field<&" << cname << "::" << name << ">{}, \"" << name << "\", FakeJni::JFieldID::PUBLIC";
		if(_static) {
			ss << " | FakeJni::JFieldID::STATIC";
		}
		ss << " },\n";
	}
	return ss.str();
}