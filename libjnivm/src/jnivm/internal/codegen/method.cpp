#include <jnivm/method.h>
#include <jnivm/internal/codegen/parseJNIType.h>
#include <sstream>
#include <regex>
using namespace jnivm;

std::string Method::GenerateHeader(const std::string &cname) {
	if(native) { return ""; }
	std::ostringstream ss;
	std::vector<std::string> parameters;
	std::string rettype;
	bool inarg = false;
	for (const char *cur = signature.data(), *end = cur + signature.length();
				cur != end; cur++) {
		std::string type;
		switch (*cur) {
		case '(':
			inarg = true;
			break;
		case ')':
			inarg = false;
			break;
		default:
			cur = ParseJNIType(cur, end, type);
		}
		if (!type.empty()) {
			if (inarg) {
				parameters.emplace_back(std::move(type));
			} else {
				rettype = std::move(type);
			}
		}
	}
	if (name == "<init>") {
		ss << cname;
	} else {
		if (_static) {
			ss << "static ";
		}
		ss << rettype << " " << name;
	}
	ss << "(";
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "jnivm::ENV* env";
		if (_static) {
			ss << ", jnivm::Class* cl";
		}
	}
	for (size_t i = 0; i < parameters.size(); i++) {
		if(i || !JNIVM_FAKE_JNI_SYNTAX) {
			ss << ", ";
		}
		ss << parameters[i];
	}
	ss << ")"
			<< ";";
	return ss.str();
}

std::string Method::GenerateStubs(std::string scope, const std::string &cname) {
	if(native) { return ""; }
	std::ostringstream ss;
	std::vector<std::string> parameters;
	std::string rettype;
	bool inarg = false;
	for (const char *cur = signature.data(), *end = cur + signature.length();
				cur != end; cur++) {
		std::string type;
		switch (*cur) {
		case '(':
			inarg = true;
			break;
		case ')':
			inarg = false;
			break;
		default:
			cur = ParseJNIType(cur, end, type);
		}
		if (!type.empty()) {
			if (inarg) {
				parameters.emplace_back(std::move(type));
			} else {
				rettype = std::move(type);
			}
		}
	}
	if (name == "<init>") {
		ss << scope << "::" << cname;
	} else {
		ss << rettype << " " << scope << "::" << name;
	}
	ss << "(";
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "jnivm::ENV *";
		if (_static) {
			ss << ", jnivm::Class* cl";
		}
	}
	for (size_t i = 0; i < parameters.size(); i++) {
		if(i || !JNIVM_FAKE_JNI_SYNTAX) {
			ss << ", ";
		}
		ss << parameters[i] << " arg" << i;
	}
	ss << ") {\n    ";
	if(rettype != "void" && name != "<init>") {
		ss << "return {};";
	}
	ss << "\n}\n\n";
	return ss.str();
}

std::string Method::GenerateJNIBinding(std::string scope, const std::string &cname) {
	if(native) { return ""; }
	std::ostringstream ss;
	std::vector<std::string> parameters;
	std::string rettype;
	bool inarg = false;
	for (const char *cur = signature.data(), *end = cur + signature.length();
				cur != end; cur++) {
		std::string type;
		switch (*cur) {
		case '(':
			inarg = true;
			break;
		case ')':
			inarg = false;
			break;
		default:
			cur = ParseJNIType(cur, end, type);
		}
		if (!type.empty()) {
			if (inarg) {
				parameters.emplace_back(std::move(type));
			} else {
				rettype = std::move(type);
			}
		}
	}
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "c->Hook(env, \"" << name << "\", ";
		auto cl = scope;
		if (name == "<init>") {
			scope += "::" + cname;
		} else {
			scope += "::" + name;
		}
		if (name == "<init>") {
			ss << "[](jnivm::ENV *env, jnivm::Class* cl";
			for (size_t i = 0; i < parameters.size(); i++) {
				ss << ", " << parameters[i] << " arg" << i;
			}
			ss << ") {";
			ss << "   return std::make_shared<" << cl << ">(env, cl";
			for (size_t i = 0; i < parameters.size(); i++) {
				ss << ", arg" << i;
			}
			ss << ");}";
			
		} else {
			ss << "&" << scope;
		}
		ss << ");\n";
	} else {
		if (name == "<init>") {
			ss << "{FakeJni::Constructor<" << cname;
			for (size_t i = 0; i < parameters.size(); i++) {
				ss << ", " << parameters[i];
			}
			ss << ">{}},\n";
		} else {
			ss << "{FakeJni::Function<&" << cname << "::" << name << ">{}, \"" << name << "\", FakeJni::JMethodID::PUBLIC";
			if(_static) {
				ss << " | FakeJni::JMethodID::STATIC";
			}
			ss << " },\n";
		}
	}
	return ss.str();
}