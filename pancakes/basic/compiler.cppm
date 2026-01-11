module;
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <ranges>

export module pancakes.basic.compiler;

import pancakes.basic.AST;

export struct Compiler final : ASTVisitor
{
    llvm::LLVMContext context{};
    std::unique_ptr<llvm::Module> module{};
    std::unique_ptr<llvm::IRBuilder<>> builder{};

    llvm::FunctionCallee initFn{};
    llvm::FunctionCallee parseFloatFn{};
    llvm::FunctionCallee printFloatFn{};
    llvm::FunctionCallee printStringFn{};
    llvm::FunctionCallee inputFn{};

    std::unordered_map<std::string, llvm::AllocaInst*> variables{};

    Compiler()
    {
        module = std::make_unique<llvm::Module>("pancakes", context);
        builder = std::make_unique<llvm::IRBuilder<>>(context);

        initializeTarget();
        setupExternalFunctions();
        createEntryPoint();
    }

    void finalizeModule()
    {
        createExitCall();
        builder->CreateRetVoid();
    }

    void visit(PrintNode* node) override
    {
        for (auto const& item : node->items)
            emitPrintItem(item);

        appendNewlineIfNeeded(node);
    }

    void visit(InputNode* node) override
    {
        constexpr int bufferSize{ 256 };
        auto* buffer{ allocateInputBuffer(bufferSize) };
        auto* bufferSizeConst{ llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), bufferSize) };
        auto* charsRead{ builder->CreateCall(inputFn, { buffer, bufferSizeConst }) };
        auto* value{ builder->CreateCall(parseFloatFn, { buffer, charsRead }) };
        builder->CreateStore(value, getOrCreateVariable(node->variable));
    }

private:
    static void initializeTarget()
    {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
    }

    void createEntryPoint()
    {
        auto* voidTy{ llvm::Type::getVoidTy(context) };
        auto* startFn{ llvm::Function::Create(
            llvm::FunctionType::get(voidTy, false),
            llvm::Function::ExternalLinkage,
            "pancakesSTART",
            module.get()
        ) };

        auto* entry{ llvm::BasicBlock::Create(context, "entry", startFn) };
        builder->SetInsertPoint(entry);
        builder->CreateCall(initFn);
    }

    void createExitCall()
    {
        auto* voidTy{ llvm::Type::getVoidTy(context) };
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* exitProcTy{ llvm::FunctionType::get(voidTy, { i32Ty }, false) };
        llvm::FunctionCallee const exitFn{ module->getOrInsertFunction("ExitProcess", exitProcTy) };
        auto* zero{ llvm::ConstantInt::get(i32Ty, 0) };
        builder->CreateCall(exitFn, { zero });
    }

    void emitPrintItem(const PrintItem& item)
    {
        switch (item.kind)
        {
            case PrintItem::Kind::Expression:
                emitExpression(item);
                break;
            case PrintItem::Kind::Tab:
                emitString("    ");
                break;
            case PrintItem::Kind::Spc:
                emitString(" ");
                break;
            case PrintItem::Kind::Sep:
                emitSeparator(item.text);
                break;
        }
    }

    void emitExpression(const PrintItem& item)
    {
        if (item.isStringLiteral)
        {
            emitString(item.text);
        }
        else
        {
            auto* value{ builder->CreateLoad(llvm::Type::getFloatTy(context), getOrCreateVariable(item.text)) };
            builder->CreateCall(printFloatFn, { value });
        }
    }

    void emitSeparator(const std::string& sep)
    {
        if (sep == ",") emitString("    ");
        else if (sep == "'") emitString("\r\n");
    }

    void appendNewlineIfNeeded(PrintNode const* node)
    {
        if (node->items.empty()) return;
        if (node->items.back().kind != PrintItem::Kind::Sep)
            emitString("\r\n");
    }

    llvm::AllocaInst* getOrCreateVariable(const std::string& name)
    {
        if (auto it{ variables.find(name) }; it != variables.end())
            return it->second;

        auto* alloca{ builder->CreateAlloca(llvm::Type::getFloatTy(context), nullptr, name) };
        variables[name] = alloca;
        return alloca;
    }

    // ReSharper disable once CppDFAConstantParameter
    llvm::Value* allocateInputBuffer(int const size)
    {
        auto* i8Ty{ llvm::Type::getInt8Ty(context) };
        auto* arrayTy{ llvm::ArrayType::get(i8Ty, size) };
        auto* buffer{ builder->CreateAlloca(arrayTy, nullptr, "inputBuffer") };
        auto* zero{ llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0) };
        return builder->CreateInBoundsGEP(arrayTy, buffer, { zero, zero }, "bufptr");
    }

    void emitString(const std::string& str)
    {
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* strPtr{ builder->CreateGlobalStringPtr(str) };
        auto* length{ llvm::ConstantInt::get(i32Ty, static_cast<int>(str.size())) };
        builder->CreateCall(printStringFn, { strPtr, length });
    }

    void setupExternalFunctions()
    {
        auto* voidTy{ llvm::Type::getVoidTy(context) };
        auto* floatTy{ llvm::Type::getFloatTy(context) };
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* i8PtrTy{ llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)) };

        auto addNoUnwind{ [](llvm::FunctionCallee fn)
        {
            if (auto* func{ llvm::dyn_cast<llvm::Function>(fn.getCallee()) })
                func->addFnAttr(llvm::Attribute::NoUnwind);
        } };

        initFn = module->getOrInsertFunction("pancakes_init", llvm::FunctionType::get(voidTy, false));
        parseFloatFn = module->getOrInsertFunction("pancakes_parse_float", llvm::FunctionType::get(floatTy, { i8PtrTy, i32Ty }, false));
        printFloatFn = module->getOrInsertFunction("pancakes_print_float", llvm::FunctionType::get(voidTy, { floatTy }, false));
        printStringFn = module->getOrInsertFunction("pancakes_print_string", llvm::FunctionType::get(voidTy, { i8PtrTy, i32Ty }, false));
        inputFn = module->getOrInsertFunction("pancakes_input", llvm::FunctionType::get(i32Ty, { i8PtrTy, i32Ty }, false));

        addNoUnwind(initFn);
        addNoUnwind(parseFloatFn);
        addNoUnwind(printFloatFn);
        addNoUnwind(printStringFn);
        addNoUnwind(inputFn);
    }
};
