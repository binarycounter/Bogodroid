#include <jnivm/vm.h>
#include <fstream>
using namespace jnivm;

std::string VM::GeneratePreDeclaration() {
	return "#include "
#if JNIVM_FAKE_JNI_SYNTAX
	"<fake-jni/fake-jni.h>"
#else
	"<jnivm.h>"
#endif
	"\n" + np.GeneratePreDeclaration();
}

std::string VM::GenerateHeader() {
	return np.GenerateHeader("");
}

std::string VM::GenerateStubs() {
	return np.GenerateStubs("");
}

std::string VM::GenerateJNIPreDeclaration() {
	return np.GenerateJNIPreDeclaration("");
}

std::string VM::GenerateJNIBinding() {
	return np.GenerateJNIBinding("");
}

void VM::GenerateClassDump(const char *path) {
	std::ofstream of(path);
	of << GeneratePreDeclaration()
	   << "\n"
	   << GenerateHeader()
	   << GenerateStubs();
	if(!JNIVM_FAKE_JNI_SYNTAX) {
		of << "void InitJNIBinding(jnivm::ENV* env) {\n"
		   << GenerateJNIPreDeclaration()
		   << GenerateJNIBinding()
		   << "\n}";
	} else {
		of << GenerateJNIBinding() << "\n"
		   << "void InitJNIBinding(FakeJni::Jvm* vm) {\n"
		   << GenerateJNIPreDeclaration()
		   << "\n}";
	}
}