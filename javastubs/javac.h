#ifndef __JAVAC_H__
#define __JAVAC_H__

#include "baron/baron.h"
#include <fstream>
namespace jnivm
{
    namespace java
    {
        namespace lang
        {
            class ClassLoader : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("java/lang/ClassLoader")
                static std::shared_ptr<FakeJni::JString> findLibrary(std::shared_ptr<FakeJni::JString> name);
                ClassLoader();
            };
            class StringBuilder : public FakeJni::JObject
            {
            private:
                FakeJni::JString str;
            public:
                DEFINE_CLASS_NAME("java/lang/StringBuilder")
                StringBuilder();
                std::shared_ptr<StringBuilder> append(std::shared_ptr<FakeJni::JString> str);
                std::shared_ptr<FakeJni::JString> toString();
            };

        }

        namespace io
        {
            class InputStream : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("java/io/InputStream")
                std::ifstream* file;
                InputStream(std::shared_ptr<FakeJni::JString> filename);
                
            };

            class File : public FakeJni::JObject
            {
                public:
                DEFINE_CLASS_NAME("java/io/File")
                std::shared_ptr<FakeJni::JString> path;
                File(std::shared_ptr<FakeJni::JString> path);
                std::shared_ptr<FakeJni::JString> getPath();
            };
        }

        namespace util
        {
            class Scanner : public FakeJni::JObject
            {
            private:
                std::shared_ptr<jnivm::java::io::InputStream> istream;
                FakeJni::JString delimiter = (FakeJni::JString) " ";

            public:
                DEFINE_CLASS_NAME("java/util/Scanner")
                Scanner(std::shared_ptr<jnivm::java::io::InputStream> stream, std::shared_ptr<FakeJni::JString> str);
                std::shared_ptr<Scanner> useDelimiter(std::shared_ptr<FakeJni::JString> str);
                std::shared_ptr<FakeJni::JString> next();
            };

        }
    }
}
#endif