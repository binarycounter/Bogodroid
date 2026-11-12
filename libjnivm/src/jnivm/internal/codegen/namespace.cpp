#include <jnivm/internal/codegen/namespace.h>
#include <jnivm/class.h>
#include <sstream>
#include <regex>
using namespace jnivm;

std::string Namespace::GenerateHeader(std::string scope) {
	std::ostringstream ss;
	if (name.length()) {
		scope += scope.empty() ? name : "::" + name;
	}
	for (auto &cl : classes) {
		auto sub = cl->GenerateHeader(scope);
		if(!sub.empty()) {
			ss << sub;
			if(sub[sub.length() - 1] != '\n')
				ss << "\n";
		}
	}
	for (auto &np : namespaces) {
		auto sub = np->GenerateHeader(scope);
		if(!sub.empty()) {
			ss << sub;
			if(sub[sub.length() - 1] != '\n')
				ss << "\n";
		}
	}
	return ss.str();
}

std::string Namespace::GeneratePreDeclaration() {
	std::ostringstream ss;
	bool indent = name.length();
	bool empty = true;
	for (auto &cl : classes) {
		auto sub = cl->GeneratePreDeclaration();
		if(!sub.empty()) {
			if(empty) {
				if (indent) {
					ss << "namespace " << name << " {\n";
				}
				empty = false;
			} else {
				ss << "\n";
			}
			ss << (indent ? std::regex_replace(sub, std::regex("(^|\n)([^\n]+)"), "$1    $2") : sub);
		}
	}
	for (auto &np : namespaces) {
		auto sub = np->GeneratePreDeclaration();
		if(!sub.empty()) {
			if(empty) {
				if (indent) {
					ss << "namespace " << name << " {\n";
				}
				empty = false;
			} else {
				ss << "\n";
			}
			ss << (indent ? std::regex_replace(sub, std::regex("(^|\n)([^\n]+)"), "$1    $2") : sub);
		}
	}
	if (!empty && indent) {
		ss << "\n}";
	}
	return ss.str();
}

std::string Namespace::GenerateStubs(std::string scope) {
	std::ostringstream ss;
	if (name.length()) {
		scope += scope.empty() ? name : "::" + name;
	}
	for (auto &cl : classes) {
		ss << cl->GenerateStubs(scope);
	}
	for (auto &np : namespaces) {
		ss << np->GenerateStubs(scope);
	}
	return ss.str();
}

std::string jnivm::Namespace::GenerateJNIPreDeclaration(std::string scope) {
	std::ostringstream ss;
	if (name.length()) {
		scope += scope.empty() ? name : "::" + name;
	}
	for (auto &cl : classes) {
		ss << cl->GenerateJNIPreDeclaration();
	}
	for (auto &np : namespaces) {
		ss << np->GenerateJNIPreDeclaration(scope);
	}
	return ss.str();
}

std::string Namespace::GenerateJNIBinding(std::string scope) {
	std::ostringstream ss;
	if (name.length()) {
		scope += scope.empty() ? name : "::" + name;
	}
	for (auto &cl : classes) {
		ss << cl->GenerateJNIBinding(scope);
	}
	for (auto &np : namespaces) {
		ss << np->GenerateJNIBinding(scope);
	}
	return ss.str();
}