#include "baron/baron.h"
#include "javac.h"
#include "platform.h"


///// Classloader

jnivm::java::lang::ClassLoader::ClassLoader(){};

std::shared_ptr<FakeJni::JString> jnivm::java::lang::ClassLoader::findLibrary(std::shared_ptr<FakeJni::JString> name) {
    verbose("JBRIDGE","findLibrary stub %s",name.get()->c_str());
    return name;
}

///// StringBuilder

jnivm::java::lang::StringBuilder::StringBuilder() {
    str = (FakeJni::JString)"";
}

std::shared_ptr<jnivm::java::lang::StringBuilder> jnivm::java::lang::StringBuilder::append(std::shared_ptr<FakeJni::JString> str_to_append) {
    str=str.append(str_to_append.get()->c_str());
    return std::shared_ptr<jnivm::java::lang::StringBuilder>(this);
}

std::shared_ptr<FakeJni::JString> jnivm::java::lang::StringBuilder::toString() {
    verbose("JBRIDGE","toString: %s",str.c_str());
    return std::shared_ptr<FakeJni::JString>(&str);
}

///// InputStream

jnivm::java::io::InputStream::InputStream(std::shared_ptr<FakeJni::JString> filename)
{
    verbose("JBRIDGE","IOStream opening file %s", filename.get()->c_str());
    std::ifstream* file=new std::ifstream(filename.get()->c_str(), std::ios::binary);
    if (!file->is_open())
    {
        verbose("JBRIDGE","IOStream ERROR on file %s", filename.get()->c_str());
    }
    this->file=file;
}

///// File

jnivm::java::io::File::File(std::shared_ptr<FakeJni::JString> path)
{
    this->path=path;
}

std::shared_ptr<FakeJni::JString> jnivm::java::io::File::getPath() {
    return path;
}

///// Scanner

jnivm::java::util::Scanner::Scanner(std::shared_ptr<jnivm::java::io::InputStream>  stream, std::shared_ptr<FakeJni::JString> str){
    verbose("JBRIDGE","initialized Scanner with stream %p and str %s",stream.get(),str.get()->c_str());
    istream=stream;
}

std::shared_ptr<jnivm::java::util::Scanner> jnivm::java::util::Scanner::useDelimiter(std::shared_ptr<FakeJni::JString> str){
    verbose("JBRIDGE","Scanner using delimiter %s",str.get()->c_str());
    delimiter=*str;
    return std::shared_ptr<Scanner>(this);
}

std::shared_ptr<FakeJni::JString> jnivm::java::util::Scanner::next(){
    std::string str;
    (istream->file)->seekg(0, std::ios::end);   
    str.reserve((istream->file)->tellg());
    (istream->file)->seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(*(istream->file))),
            std::istreambuf_iterator<char>());
    verbose("JBRIDGE","Scanner returning: %s",str.c_str());
    return std::make_shared<FakeJni::JString>(str);
}


// Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::ClassLoader)
{FakeJni::Constructor<ClassLoader>{}},
{FakeJni::Function<&ClassLoader::findLibrary>{}, "findLibrary", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR
BEGIN_NATIVE_DESCRIPTOR(jnivm::java::lang::StringBuilder)
{FakeJni::Constructor<StringBuilder>{}},
{FakeJni::Function<&StringBuilder::append>{}, "append", FakeJni::JMethodID::PUBLIC },
{FakeJni::Function<&StringBuilder::toString>{}, "toString", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR
BEGIN_NATIVE_DESCRIPTOR(jnivm::java::io::InputStream)
{FakeJni::Constructor<InputStream, std::shared_ptr<FakeJni::JString>>{}},
END_NATIVE_DESCRIPTOR
BEGIN_NATIVE_DESCRIPTOR(jnivm::java::io::File)
{FakeJni::Constructor<File, std::shared_ptr<FakeJni::JString>>{}},
{FakeJni::Function<&File::getPath>{}, "getPath", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR
BEGIN_NATIVE_DESCRIPTOR(jnivm::java::util::Scanner)
{FakeJni::Constructor<Scanner, std::shared_ptr<jnivm::java::io::InputStream>, std::shared_ptr<FakeJni::JString>>{}},
{FakeJni::Function<&Scanner::useDelimiter>{}, "useDelimiter", FakeJni::JMethodID::PUBLIC },
{FakeJni::Function<&Scanner::next>{}, "next", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR


