#pragma once
#include <string>
#include <vector>
#include <memory>

namespace jnivm {
    class Class;
    class Namespace {
    public:
        std::string name;
        std::vector<std::shared_ptr<Namespace>> namespaces;
        std::vector<std::shared_ptr<Class>> classes;

        std::string GenerateHeader(std::string scope);
        std::string GeneratePreDeclaration();
        std::string GenerateStubs(std::string scope);
        std::string GenerateJNIPreDeclaration(std::string scope);
        std::string GenerateJNIBinding(std::string scope);
    };
}