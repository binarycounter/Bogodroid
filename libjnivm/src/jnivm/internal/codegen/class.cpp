#include <jnivm/class.h>
#include <jnivm/method.h>
#include <jnivm/internal/codegen/parseJNIType.h>
#include <sstream>
#include <regex>
using namespace jnivm;

static const char* blacklisted[] = { "java/lang/Object", "java/lang/String", "java/lang/Class", "java/nio/ByteBuffer", "java/lang/Throwable", "java/lang/reflect/Method", "java/lang/reflect/Field", "java/lang/ref/WeakReference", "internal/lang/Global" };

std::string Class::GenerateHeader(std::string scope) {
	if (std::find(std::begin(blacklisted), std::end(blacklisted), nativeprefix) != std::end(blacklisted)) return {};
	std::ostringstream ss;
	scope += scope.empty() ? name : "::" + name;
	ss << "class " << scope << " : ";
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "public jnivm::Object";
	} else {
		ss << "public FakeJni::JObject";
	}
	ss << " {\npublic:\n";
	if(JNIVM_FAKE_JNI_SYNTAX) {
	    ss << "    DEFINE_CLASS_NAME(\"" << nativeprefix << "\")\n";
	}
	for (auto &cl : classes) {
		auto sub = cl->GeneratePreDeclaration();
		if(!sub.empty()) {
			ss << std::regex_replace(sub, std::regex("(^|\n)([^\n]+)"), "$1    $2");
			ss << "\n";
		}
	}
	for (auto &field : fields) {
		auto sub = field->GenerateHeader();
		if(!sub.empty()) {
			ss << std::regex_replace(sub, std::regex("(^|\n)([^\n]+)"), "$1    $2");
			ss << "\n";
		}
	}
	for (auto &method : methods) {
		auto sub = method->GenerateHeader(name);
		if(!sub.empty()) {
			ss << std::regex_replace(sub, std::regex("(^|\n)([^\n]+)"), "$1    $2");
			ss << "\n";
		}
	}
	ss << "};\n\n";
	for (auto &cl : classes) {
		// ss << "\n";
		ss << cl->GenerateHeader(scope);
	}
	return ss.str();
}

std::string Class::GeneratePreDeclaration() {
	if (std::find(std::begin(blacklisted), std::end(blacklisted), nativeprefix) != std::end(blacklisted)) return {};
	std::ostringstream ss;
	ss << "class " << name << ";";
	return ss.str();
}

std::string Class::GenerateStubs(std::string scope) {
	if (std::find(std::begin(blacklisted), std::end(blacklisted), nativeprefix) != std::end(blacklisted)) return {};
	std::ostringstream ss;
	scope += scope.empty() ? name : "::" + name;
	for (auto &cl : classes) {
		ss << cl->GenerateStubs(scope);
	}
	for (auto &field : fields) {
		ss << field->GenerateStubs(scope, name);
	}
	for (auto &method : methods) {
		ss << method->GenerateStubs(scope, name);
	}
	return ss.str();
}

std::string jnivm::Class::GenerateJNIPreDeclaration() {
	if (std::find(std::begin(blacklisted), std::end(blacklisted), nativeprefix) != std::end(blacklisted)) return {};
	std::ostringstream ss;
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "env->GetClass<jnivm::" << std::regex_replace(nativeprefix, std::regex("[/\\$]"), "::") << ">(\"" << nativeprefix << "\");\n";
	} else {
		ss << "vm->registerClass<jnivm::" << std::regex_replace(nativeprefix, std::regex("[/\\$]"), "::") << ">();\n";
	}
	for (auto &cl : classes) {
		ss << cl->GenerateJNIPreDeclaration();
	}
	return ss.str();
}

std::string Class::GenerateJNIBinding(std::string scope) {
	if (std::find(std::begin(blacklisted), std::end(blacklisted), nativeprefix) != std::end(blacklisted)) return {};
	std::ostringstream ss;
	scope += scope.empty() ? name : "::" + name;
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		ss << "{\nauto c = env->GetClass(\"" << nativeprefix << "\");\n";
		for (auto &cl : classes) {
			ss << cl->GenerateJNIBinding(scope);
		}
		for (auto &field : fields) {
			ss << field->GenerateJNIBinding(scope, name);
		}
		for (auto &method : methods) {
			ss << method->GenerateJNIBinding(scope, name);
		}
		ss << "}\n";
	} else {
		ss << "BEGIN_NATIVE_DESCRIPTOR(" << scope << ")\n";
		for (auto &field : fields) {
			ss << field->GenerateJNIBinding(scope, name);
		}
		for (auto &method : methods) {
			ss << method->GenerateJNIBinding(scope, name);
		}
		ss << "END_NATIVE_DESCRIPTOR\n";
		for (auto &cl : classes) {
			ss << cl->GenerateJNIBinding(scope);
		}
	}
	return ss.str();
}
