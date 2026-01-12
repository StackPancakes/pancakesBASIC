module;
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <variant>

export module pancakes.basic.compiler;

import pancakes.basic.AST;

export struct Compiler final : ASTVisitor<Compiler>
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
        setupExternalFunctions();
        createEntryPoint();
    }

    void visit(PrintNode& node)
    {
        for (auto& item : node.items)
            dispatch(item);

        if (node.items.empty())
        {
            emitString("\n");
            return;
        }

        bool const hasTrailingSep{ std::visit([]<typename T0>(T0 const& x) -> bool
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, PrintItem::Sep>)
                return x.symbol == ";" || x.symbol == ",";
            return false;
        }, node.items.back().value) };

        if (!hasTrailingSep)
            emitString("\n");
    }

    void visit(InputNode const& node)
    {
        llvm::Type* i8Ty{ llvm::Type::getInt8Ty(context) };
        llvm::ArrayType* arrayTy{ llvm::ArrayType::get(i8Ty, 256) };
        llvm::AllocaInst* buffer{ builder->CreateAlloca(arrayTy, nullptr, "inputBuffer") };
        llvm::Value* zero{ llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0) };
        llvm::Value* bufPtr{ builder->CreateInBoundsGEP(arrayTy, buffer, { zero, zero }) };
        llvm::Value* bufSize{ llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 256) };
        llvm::Value* readCount{ builder->CreateCall(inputFn, { bufPtr, bufSize }) };
        llvm::Value* val{ builder->CreateCall(parseFloatFn, { bufPtr, readCount }) };
        builder->CreateStore(val, getOrCreateVariable(node.variable));
    }

    void visit(PrintItem::Expression const& x)
    {
        if (x.isStringLiteral)
            emitString(x.text);
        else
        {
            llvm::Value* val{ builder->CreateLoad(llvm::Type::getFloatTy(context), getOrCreateVariable(x.text)) };
            builder->CreateCall(printFloatFn, { val });
        }
    }

    void visit(PrintItem::Sep const& x)
    {
        if (x.symbol == ",")
            emitString("    ");
        else if (x.symbol == "'")
            emitString("\n");
    }

    void visit(PrintItem::Tab& x)
    {
        emitString("    ");
    }

    void visit(PrintItem::Spc& x)
    {
        emitString(" ");
    }

    void finalizeModule() const
    {
        builder->CreateRetVoid();
    }

private:
    void emitString(std::string const& str)
    {
        llvm::Value* strPtr{ builder->CreateGlobalStringPtr(str) };
        llvm::Value* len{ llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), static_cast<int>(str.size())) };
        builder->CreateCall(printStringFn, { strPtr, len });
    }

    llvm::AllocaInst* getOrCreateVariable(std::string const& name)
    {
        if (variables.contains(name))
            return variables[name];

        llvm::BasicBlock& entryBlock{ builder->GetInsertBlock()->getParent()->getEntryBlock() };
        llvm::IRBuilder<> tmpBuilder{ &entryBlock, entryBlock.begin() };
        llvm::AllocaInst* inst{ tmpBuilder.CreateAlloca(llvm::Type::getFloatTy(context), nullptr, name) };
        variables[name] = inst;
        return inst;
    }

    void createEntryPoint()
    {
        llvm::FunctionType* ft{ llvm::FunctionType::get(llvm::Type::getVoidTy(context), false) };
        llvm::Function* f{ llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "pancakesSTART", module.get()) };
        llvm::BasicBlock* bb{ llvm::BasicBlock::Create(context, "entry", f) };
        builder->SetInsertPoint(bb);
        builder->CreateCall(initFn);
    }

    void setupExternalFunctions()
    {
        llvm::Type* voidTy{ llvm::Type::getVoidTy(context) };
        llvm::Type* floatTy{ llvm::Type::getFloatTy(context) };
        llvm::Type* i32Ty{ llvm::Type::getInt32Ty(context) };
        llvm::Type* i8PtrTy{ llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)) };
        initFn = module->getOrInsertFunction("pancakes_init", llvm::FunctionType::get(voidTy, false));
        parseFloatFn = module->getOrInsertFunction("pancakes_parse_float", llvm::FunctionType::get(floatTy, { i8PtrTy, i32Ty }, false));
        printFloatFn = module->getOrInsertFunction("pancakes_print_float", llvm::FunctionType::get(voidTy, { floatTy }, false));
        printStringFn = module->getOrInsertFunction("pancakes_print_string", llvm::FunctionType::get(voidTy, { i8PtrTy, i32Ty }, false));
        inputFn = module->getOrInsertFunction("pancakes_input", llvm::FunctionType::get(i32Ty, { i8PtrTy, i32Ty }, false));
    }
};