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

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        setupExternalFunctions();

        auto* voidTy{ llvm::Type::getVoidTy(context) };
        auto* startFn{ llvm::Function::Create(
            llvm::FunctionType::get(voidTy, false),
            llvm::Function::ExternalLinkage,
            "pancakesSTART",
            module.get()
        ) };

        auto* entry{ llvm::BasicBlock::Create(context, "entry", startFn) };
        builder->SetInsertPoint(entry);

        // Call pancakes_init at the very start
        builder->CreateCall(initFn);
    }

    void finalizeModule()
    {
        auto* voidTy{ llvm::Type::getVoidTy(context) };
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* exitProcTy{ llvm::FunctionType::get(voidTy, { i32Ty }, false) };
        llvm::FunctionCallee const exitProcFn{ module->getOrInsertFunction("ExitProcess", exitProcTy) };
        builder->CreateCall(exitProcFn, { llvm::ConstantInt::get(i32Ty, 0) });
        builder->CreateRetVoid();
    }

    void visit(PrintNode* node) override
    {
        if (node->isStringLiteral)
        {
            auto* strConst{ builder->CreateGlobalStringPtr(node->text) };
            builder->CreateCall(printStringFn, {
                        strConst,
                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), static_cast<int>(node->text.size()))
                    });
        }
        else
        {
            auto* alloca{ getVariableAlloca(node->text) };
            auto* floatVal{ builder->CreateLoad(llvm::Type::getFloatTy(context), alloca) };
            builder->CreateCall(printFloatFn, { floatVal });
        }
    }


    void visit(InputNode* node) override
    {
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* i8Ty{ llvm::Type::getInt8Ty(context) };
        auto* buffer{ builder->CreateAlloca(llvm::ArrayType::get(i8Ty, 256), nullptr, "inputBuffer") };

        auto* charsRead{ builder->CreateCall(inputFn, { buffer, llvm::ConstantInt::get(i32Ty, 256) }) };
        auto* varAlloca{ getVariableAlloca(node->variable) };
        auto* value{ builder->CreateCall(parseFloatFn, { buffer, charsRead }) };
        builder->CreateStore(value, varAlloca);
    }

private:
    llvm::AllocaInst* getVariableAlloca(const std::string& name)
    {
        if (auto const it{ variables.find(name) }; it != variables.end())
            return it->second;

        auto* alloca{ builder->CreateAlloca(llvm::Type::getFloatTy(context), nullptr, name) };
        variables[name] = alloca;
        return alloca;
    }

    void setupExternalFunctions()
    {
        auto* voidTy{ llvm::Type::getVoidTy(context) };
        auto* floatTy{ llvm::Type::getFloatTy(context) };
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* i8PtrTy{ llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)) };

        auto setCommonAttrs{ [](llvm::FunctionCallee fn)
        {
            if (auto* func{ llvm::dyn_cast<llvm::Function>(fn.getCallee()) })
                func->addFnAttr(llvm::Attribute::NoUnwind);
        } };

        initFn = module->getOrInsertFunction("pancakes_init", llvm::FunctionType::get(voidTy, false));
        setCommonAttrs(initFn);

        parseFloatFn = module->getOrInsertFunction("pancakes_parse_float", llvm::FunctionType::get(floatTy, { i8PtrTy, i32Ty }, false));
        setCommonAttrs(parseFloatFn);
        if (auto* f{ llvm::dyn_cast<llvm::Function>(parseFloatFn.getCallee()) })
        {
            f->addParamAttr(0, llvm::Attribute::ReadOnly);
            f->addParamAttr(0, llvm::Attribute::NoCapture);
        }

        printFloatFn = module->getOrInsertFunction("pancakes_print_float", llvm::FunctionType::get(voidTy, { floatTy }, false));
        setCommonAttrs(printFloatFn);

        printStringFn = module->getOrInsertFunction("pancakes_print_string", llvm::FunctionType::get(voidTy, { i8PtrTy, i32Ty }, false));
        setCommonAttrs(printStringFn);
        if (auto* f{ llvm::dyn_cast<llvm::Function>(printStringFn.getCallee()) })
        {
            f->addParamAttr(0, llvm::Attribute::ReadOnly);
            f->addParamAttr(0, llvm::Attribute::NoCapture);
        }

        inputFn = module->getOrInsertFunction("pancakes_input", llvm::FunctionType::get(i32Ty, { i8PtrTy, i32Ty }, false));
        setCommonAttrs(inputFn);
        if (auto* f{ llvm::dyn_cast<llvm::Function>(inputFn.getCallee()) })
            f->addParamAttr(0, llvm::Attribute::NoCapture);
    }
};
