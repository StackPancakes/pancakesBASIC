module;
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <windows.h>
#include <memory>

export module pancakes.basic.compiler;

import pancakes.basic.AST;

export struct Compiler final : ASTVisitor
{
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    llvm::FunctionCallee getStdHandleFn;
    llvm::FunctionCallee writeConsoleFn;

    llvm::GlobalVariable* hConsoleGV{};
    llvm::Function* initConsoleFn{};
    llvm::Function* mainFn{};
    llvm::AllocaInst* writtenAlloca{};

    Compiler()
        : module{ std::make_unique<llvm::Module>("pancakes", context) }
    , builder{ std::make_unique<llvm::IRBuilder<>>(context) }
    {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        auto* i1Ty{ llvm::Type::getInt1Ty(context) };
        auto* i8Ty{ llvm::Type::getInt8Ty(context) };
        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* voidTy{ llvm::Type::getVoidTy(context) };

        auto* voidPtrTy{ llvm::PointerType::getUnqual(i8Ty) };
        auto* dwordPtrTy{ llvm::PointerType::getUnqual(i32Ty) };

        getStdHandleFn = module->getOrInsertFunction(
                "GetStdHandle",
                llvm::FunctionType::get(voidPtrTy, { i32Ty }, false)
                );

        writeConsoleFn = module->getOrInsertFunction(
                "WriteConsoleA",
                llvm::FunctionType::get(
                    i1Ty,
                    { voidPtrTy, voidPtrTy, i32Ty, dwordPtrTy, voidPtrTy },
                    false
                    )
                );

        llvm::cast<llvm::Function>(getStdHandleFn.getCallee())->addFnAttr(llvm::Attribute::NoUnwind);
        llvm::cast<llvm::Function>(writeConsoleFn.getCallee())->addFnAttr(llvm::Attribute::NoUnwind);


        hConsoleGV = new llvm::GlobalVariable(
                *module,
                voidPtrTy,
                false,
                llvm::GlobalVariable::InternalLinkage,
                llvm::ConstantPointerNull::get(voidPtrTy),
                "hConsole"
                );

        auto* initTy{ llvm::FunctionType::get(voidTy, false) };

        initConsoleFn = llvm::Function::Create(
                initTy,
                llvm::GlobalValue::InternalLinkage,
                "pancakes_init_console",
                module.get()
                );

        initConsoleFn->addFnAttr(llvm::Attribute::AlwaysInline);

        {
            auto* entry{ llvm::BasicBlock::Create(context, "entry", initConsoleFn) };
            llvm::IRBuilder<> tmpBuilder{ entry };

            auto* stdoutConst{
                llvm::ConstantInt::get(i32Ty, static_cast<int32_t>(STD_OUTPUT_HANDLE))
            };

            auto* handle{ tmpBuilder.CreateCall(getStdHandleFn, { stdoutConst }) };
            tmpBuilder.CreateStore(handle, hConsoleGV);
            tmpBuilder.CreateRetVoid();
        }
    }

    void ensureMain()
    {
        if (mainFn)
            return;

        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* i8PtrTy{ llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)) };
        auto* i8PtrPtrTy{ llvm::PointerType::getUnqual(i8PtrTy) };

        std::vector<llvm::Type*> mainArgTypes{ i32Ty, i8PtrPtrTy };

        auto* mainTy{ llvm::FunctionType::get(i32Ty, mainArgTypes, false) };

        mainFn = llvm::Function::Create(
                mainTy,
                llvm::Function::ExternalLinkage,
                "main",
                module.get()
                );

        auto* entry{ llvm::BasicBlock::Create(context, "entry", mainFn) };
        builder->SetInsertPoint(entry);

        writtenAlloca = builder->CreateAlloca(i32Ty, nullptr, "written");
        builder->CreateCall(initConsoleFn);
    }

    void finalizeMain()
    {
        if (!mainFn)
            return;

        if (!builder->GetInsertBlock()->getTerminator())
            builder->CreateRet(llvm::ConstantInt::get(
                        llvm::Type::getInt32Ty(context), 0
                        ));
    }

    void visit(PrintNode* node) override
    {
        ensureMain();

        auto* i32Ty{ llvm::Type::getInt32Ty(context) };
        auto* i8Ty{ llvm::Type::getInt8Ty(context) };
        auto* voidPtrTy{ llvm::PointerType::getUnqual(i8Ty) };

        std::string text{ node->text + '\n' };
        auto* str{ builder->CreateGlobalString(text) };

        auto* handle{ builder->CreateLoad(voidPtrTy, hConsoleGV) };

        auto* len{
            llvm::ConstantInt::get(i32Ty, static_cast<int32_t>(text.size()))
        };

        builder->CreateCall(
                writeConsoleFn,
                {
                handle,
                str,
                len,
                writtenAlloca,
                llvm::ConstantPointerNull::get(voidPtrTy)
                }
                );
    }
};
