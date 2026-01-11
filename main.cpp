#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <system_error>
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>
#include <optional>
#include <windows.h>
#include "config.h"

import pancakes.basic.compiler;
import pancakes.basic.lexer;
import pancakes.basic.parser;
import pancakes.basic.interpreter;
import pancakes.basic.token;

constexpr auto DUMP_TOKENS{ "--DUMP-TOKENS" };
constexpr auto COMPILE{ "--COMPILE" };
constexpr auto BASIC_FILE_EXTENSION_LENGTH{ 4 };

namespace fs = std::filesystem;

extern "C" void pancakes_init();

static std::string makeTokensFilename(std::string const& basFile)
{
    if (basFile.size() >= BASIC_FILE_EXTENSION_LENGTH &&
        basFile.substr(basFile.size() - BASIC_FILE_EXTENSION_LENGTH) == ".bas")
        return basFile.substr(0, basFile.size() - BASIC_FILE_EXTENSION_LENGTH) + "tokens.txt";
    return basFile + ".tokens.txt";
}

static bool linkObjectToExe(std::wstring const& objFile, std::wstring const& exeFile)
{
    std::wstring const commandLine
    {
        L"link.exe /nologo \"" + objFile + L"\" \"" +
        PANCAKES_RUNTIME_LIB_DIR + L"/pancakes_runtime.lib\" /OUT:\"" +
        exeFile + L"\" /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:pancakesSTART kernel32.lib"
    };

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL const success
    {
        CreateProcessW(nullptr, const_cast<wchar_t*>(commandLine.data()),
                       nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)
    };

    if (!success)
    {
        std::cerr << "Failed to start linker process. Error: " << GetLastError() << "\n";
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode{};
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

static std::string readFile(fs::path const& path)
{
    std::ifstream const reader{ path };
    if (!reader)
        throw std::runtime_error{ "Failed to open " + path.string() };

    std::ostringstream ss;
    ss << reader.rdbuf();
    return ss.str();
}

static fs::path resolvePath(std::string_view const fileName)
{
    fs::path path{ fileName };
    if (!path.is_absolute())
    {
        try
        {
            path = fs::canonical(path);
        }
        catch (...)
        {
            throw std::runtime_error{ "Failed to find file: " + path.string() };
        }
    }
    return path;
}

static void dumpTokens(Lexer const& lexer, std::string const& outFile)
{
    std::ofstream ofs{ outFile, std::ios::trunc };
    if (!ofs)
        throw std::runtime_error{ "Failed to open " + outFile };

    ofs << lexer;
}

static void interpret(std::vector<Token>& tokens)
{
    pancakes_init();
    Parser parser{ tokens };
    auto stmt{ parser.parse() };
    if (!stmt) return;

    Interpreter interpreter;
    stmt->accept(interpreter);
}

static void compile(std::vector<Token>& tokens, std::string_view const inputFile)
{
    Parser parser{ tokens };
    auto stmt{ parser.parse() };
    if (!stmt) return;

    Compiler compiler;
    stmt->accept(compiler);
    compiler.finalizeModule();

    if (llvm::verifyModule(*compiler.module, &llvm::errs()))
        throw std::runtime_error{ "LLVM verification failed" };

    std::error_code ec;
    std::string const llPath{ "output.ll" };
    std::string const objPath{ "output.obj" };

    // Write LLVM IR
    {
        llvm::raw_fd_ostream llOut{ llPath, ec, llvm::sys::fs::OF_Text };
        if (ec)
            throw std::runtime_error{ "Failed to open output.ll: " + ec.message() };
        compiler.module->print(llOut, nullptr);
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string const targetTriple{ llvm::sys::getDefaultTargetTriple() };
    compiler.module->setTargetTriple(targetTriple);

    std::string error;
    const llvm::Target* target{ llvm::TargetRegistry::lookupTarget(targetTriple, error) };
    if (!target) throw std::runtime_error{ error };

    llvm::TargetOptions const opt{};
    constexpr std::optional<llvm::Reloc::Model> relocModel{};
    auto targetMachine{ target->createTargetMachine(targetTriple, "generic", "", opt, relocModel) };
    compiler.module->setDataLayout(targetMachine->createDataLayout());

    // Emit object file
    {
        llvm::raw_fd_ostream objOut{ objPath, ec, llvm::sys::fs::OF_None };
        if (ec)
            throw std::runtime_error{ "Failed to open object file: " + ec.message() };

        llvm::legacy::PassManager pass;
        if (targetMachine->addPassesToEmitFile(pass, objOut, nullptr, llvm::CodeGenFileType::ObjectFile))
            throw std::runtime_error{ "Failed to emit object file" };

        pass.run(*compiler.module);
        objOut.flush();
    }

    fs::path exePath{ inputFile };
    exePath.replace_extension(".exe");

    if (!linkObjectToExe(std::wstring{ objPath.begin(), objPath.end() }, exePath.wstring()))
        throw std::runtime_error{ "Linking failed" };

    fs::remove(llPath);
    fs::remove(objPath);
}

int main(int const argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: [--DUMP-TOKENS] [--COMPILE] <.bas file>\n";
            return 1;
        }

        std::span const args(argv + 1, argc - 1);

        bool toDumpTokens{ false };
        bool toCompile{ false };
        std::vector<std::string_view> fileNames{};

        for (char* argPtr : args)
        {
            std::string_view const arg{ argPtr };

            if (arg.ends_with(".bas"))
            {
                fileNames.push_back(arg);
                continue;
            }

            if (arg.starts_with("--"))
            {
                std::string flag{ arg };
                for (char& c : flag)
                    if (c != '-') c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

                if (flag == DUMP_TOKENS) toDumpTokens = true;
                if (flag == COMPILE) toCompile = true;
            }
        }

        if (fileNames.empty())
            throw std::runtime_error{ "No .bas file specified" };

        fs::path const path{ resolvePath(fileNames[0]) };
        std::string const buffer{ readFile(path) };

        Lexer lexer{ buffer };
        if (toDumpTokens)
            dumpTokens(lexer, makeTokensFilename(std::string{ fileNames[0] }));

        auto tokens{ lexer.tokenize() }; // mutable vector for Parser
        if (toCompile)
            compile(tokens, fileNames[0]);
        else
            interpret(tokens);

        return 0;
    }
    catch (std::runtime_error const& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}