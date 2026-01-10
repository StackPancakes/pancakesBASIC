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

constexpr auto DUMP_TOKENS{ "--DUMP-TOKENS" };
constexpr auto COMPILE{ "--COMPILE" };
constexpr auto BASIC_FILE_EXTENSION_LENGTH{ 4 };

namespace fs = std::filesystem;

static std::string make_tokens_filename(const std::string& bas_file)
{
    if (bas_file.size() >= 4 && bas_file.substr(bas_file.size() - BASIC_FILE_EXTENSION_LENGTH) == ".bas")
        return bas_file.substr(0, bas_file.size() - BASIC_FILE_EXTENSION_LENGTH) + "tokens.txt";
    return bas_file + ".tokens.txt";
}

static bool linkObjectToExe(std::wstring const& objFile, std::wstring const& exeFile)
{
    std::wstring commandLine
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
        CreateProcessW(
                nullptr,
                commandLine.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                nullptr,
                &si,
                &pi
                )
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

int main(int argc, char* argv[])
{
    bool to_dump_tokens{ false };
    bool to_compile{ false };

    std::span args(argv + 1, argc - 1);

    if (args.empty())
    {
        std::cerr << "Usage: " << "[--DUMP-TOKENS] <.bas files>\n";
        return 1;
    }

    std::vector<std::string_view> file_names{};

    for (char* arguments : args)
    {
        std::string_view temp{ arguments };

        if (temp.ends_with(".bas"))
        {
            file_names.emplace_back(arguments);
            continue;
        }

        if (temp.starts_with("--"))
        {
            std::string flag{ arguments };
            for (char& c : flag)
                if (c != '-')
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            if (flag == DUMP_TOKENS)
                to_dump_tokens = true;
            if (flag == COMPILE)
                to_compile = true;
        }
    }


    std::string_view fileName{ file_names[0] };
    fs::path path{ std::string{ fileName } };
    if (!path.is_absolute())
    {
        try
        {
            path = fs::canonical(path);
        }
        catch (std::exception const&)
        {
            std::cerr << "Error: Failed to find " << path << '\n';
            return 1;
        }
    }

    std::ifstream reader{ path };
    if (!reader)
    {
        std::cerr << "Error: Failed to open " << path << " for reading.\n";
        return 1;
    }

    std::ostringstream ss;
    ss << reader.rdbuf();

    std::string buffer{ ss.str() };

    Lexer lexer{ buffer };

    if (to_dump_tokens)
    {
        std::string out_file{ make_tokens_filename(std::string{ file_names[0] }) };
        std::fstream ofs{ out_file, std::ios::out | std::ios::trunc };

        if (!ofs)
        {
            std::cerr << "Failed to open " << out_file << "\n";
            return 1;
        }

        ofs << lexer;
    }

    auto tokens{ lexer.tokenize() };

    try
    {
        Parser parser{ tokens };
        auto stmt{ parser.parse() };

        if (!to_compile)
        {
            Interpreter interpreter;
            if (stmt) stmt->accept(interpreter);
        }
        else
        {
            Compiler compiler;
            if (stmt) stmt->accept(compiler);
            compiler.finalizeModule();

            if (llvm::verifyModule(*compiler.module, &llvm::errs()))
            {
                llvm::errs() << "LLVM verification failed\n";
                return 1;
            }

            std::error_code ec;
            std::string llPath{ "output.ll" };
            std::string objPathStr{ "output.obj" };
            {
                llvm::raw_fd_ostream llOut{ llPath, ec, llvm::sys::fs::OF_Text };
                if (ec)
                {
                    llvm::errs() << "Failed to open output.ll: " << ec.message() << "\n";
                    return 1;
                }
                compiler.module->print(llOut, nullptr);
            }

            llvm::InitializeNativeTarget();
            llvm::InitializeNativeTargetAsmPrinter();
            llvm::InitializeNativeTargetAsmParser();

            std::string targetTriple{ llvm::sys::getDefaultTargetTriple() };
            compiler.module->setTargetTriple(targetTriple);

            std::string error;
            const llvm::Target* target{ llvm::TargetRegistry::lookupTarget(targetTriple, error) };
            if (!target)
            {
                llvm::errs() << error << "\n";
                return 1;
            }

            llvm::TargetOptions opt;
            std::optional<llvm::Reloc::Model> relocModel;
            std::unique_ptr<llvm::TargetMachine> targetMachine{ target->createTargetMachine(targetTriple, "generic", "", opt, relocModel) };

            compiler.module->setDataLayout(targetMachine->createDataLayout());

            {
                llvm::raw_fd_ostream objOut{ objPathStr, ec, llvm::sys::fs::OF_None };
                if (ec)
                    return 1;

                llvm::legacy::PassManager pass;
                if (targetMachine->addPassesToEmitFile(pass, objOut, nullptr, llvm::CodeGenFileType::ObjectFile))
                    return 1;
                pass.run(*compiler.module);
                objOut.flush();
            }

            auto exePath{ fs::path(file_names[0]) };
            exePath.replace_extension(".exe");

            if (!linkObjectToExe(std::wstring{ objPathStr.begin(), objPathStr.end() }, exePath.wstring()))
                return 1;
            fs::remove(llPath);
            fs::remove(objPathStr);
        }
    }
    catch (std::runtime_error const& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
