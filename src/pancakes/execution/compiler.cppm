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
import pancakes.basic.settings;
import pancakes.basic.visitor;
import pancakes.basic.util;

export struct Compiler final
{
    llvm::LLVMContext context{};
    std::unique_ptr<llvm::Module> module{};
    std::unique_ptr<llvm::IRBuilder<>> builder{};

    llvm::FunctionCallee initFn{};
    llvm::FunctionCallee parseFloatFn{};
    llvm::FunctionCallee printNumberFn{};
    llvm::FunctionCallee printStringFn{};
    llvm::FunctionCallee printIntFn{};
    llvm::FunctionCallee inputFn{};
    llvm::FunctionCallee getWindowsSizeFn{};
    llvm::FunctionCallee moveCursorFn{};
    llvm::FunctionCallee getCursorPosFn{};

    llvm::Constant* spacesGlobal{ nullptr };

    std::unordered_map<std::string, llvm::AllocaInst*, CaseInsensitiveHash, CaseInsensitiveEqual> variables{};

    int fieldWidth{ 10 };
    int screenWidth{ 80 };
    int screenHeight{ 25 };

    Compiler()
    {
        module = std::make_unique<llvm::Module>("pancakes", context);
        builder = std::make_unique<llvm::IRBuilder<>>(context);
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        createReusableGlobals();
        setupExternalFunctions();
        createEntryPoint();
    }

    void run(ProgramNode& program)
    {
        visitAll(program.statements, ConstexprVisitor{ *this });
    }

    void visit(PrintNode& node)
    {
        for (auto const&[value] : node.items)
            std::visit(ConstexprVisitor{ *this }, value);

        if (node.items.empty())
        {
            emitString("\n");
            return;
        }

        bool const hasTrailingSep{ std::visit([]<typename T0>(T0 const& x) -> bool
        {
            using T = std::decay_t<T0>;
            if constexpr (std::is_same_v<T, PrintItem::Sep>)
                return x.symbol == ";" || x.symbol == "," || x.symbol == "'";
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
            return emitString(x.text);

        llvm::Value* val{ nullptr };

        if (isNumericLiteral(x.text))
        {
            llvm::Value* strPtr{ builder->CreateGlobalStringPtr(x.text) };
            llvm::Value* len{ builder->getInt32(static_cast<int>(x.text.size())) };
            val = builder->CreateCall(parseFloatFn, { strPtr, len });
        }
        else
        {
            llvm::Value* allocaInst{ getOrCreateVariable(x.text) };
            val = builder->CreateLoad(builder->getFloatTy(), allocaInst);
        }

        builder->CreateCall(printNumberFn, { val });
    }

    void visit(PrintItem::Sep const& x)
    {
        if (x.symbol == ",")
        {
            llvm::Type* i32Ty{ llvm::Type::getInt32Ty(context) };

            llvm::AllocaInst* colAlloca{ builder->CreateAlloca(i32Ty, nullptr, "col") };
            llvm::AllocaInst* rowAlloca{ builder->CreateAlloca(i32Ty, nullptr, "row") };
            builder->CreateCall(getCursorPosFn, { colAlloca, rowAlloca });

            llvm::Value* colLoad{ builder->CreateLoad(i32Ty, colAlloca) };
            llvm::Value* fld{ llvm::ConstantInt::get(i32Ty, fieldWidth) };
            llvm::Value* rem{ builder->CreateSRem(colLoad, fld) };
            llvm::Value* spacesToPrint{ builder->CreateSub(fld, rem) };

            llvm::Value* zero{ llvm::ConstantInt::get(i32Ty, 0) };
            llvm::Value* ptr{ builder->CreateInBoundsGEP(
                llvm::ArrayType::get(llvm::Type::getInt8Ty(context), fieldWidth + 1),
                spacesGlobal,
                { zero, zero }
            ) };
            builder->CreateCall(printStringFn, { ptr, spacesToPrint });
        }
        else if (x.symbol == "'")
            emitString("\n");
    }

    void visit(PrintItem::Tab const& x)
    {
        if (x.first.empty())
            return;

        llvm::Type* i32Ty{ llvm::Type::getInt32Ty(context) };

        llvm::Value* winSize = builder->CreateCall(getWindowsSizeFn);
        llvm::Value* screenWidthVal = builder->CreateExtractValue(winSize, {0});
        llvm::Value* screenHeightVal = builder->CreateExtractValue(winSize, {1});

        int const rawX{ std::stoi(x.first) };
        llvm::Value* xVal{ llvm::ConstantInt::get(i32Ty, rawX) };
        llvm::Value* zero{ llvm::ConstantInt::get(i32Ty, 0) };
        llvm::Value* clampedX{ emitClamp(xVal, zero, builder->CreateSub(screenWidthVal, llvm::ConstantInt::get(i32Ty, 1))) };

        llvm::AllocaInst* colAlloca{ builder->CreateAlloca(i32Ty, nullptr, "tab_col") };
        llvm::AllocaInst* rowAlloca{ builder->CreateAlloca(i32Ty, nullptr, "tab_row") };
        builder->CreateCall(getCursorPosFn, { colAlloca, rowAlloca });
        llvm::Value* destY{ builder->CreateLoad(i32Ty, rowAlloca) };

        if (x.second.has_value())
        {
            int const rawY{ std::stoi(*x.second) };
            llvm::Value* yVal{ llvm::ConstantInt::get(i32Ty, rawY) };
            destY = emitClamp(yVal, zero, builder->CreateSub(screenHeightVal, llvm::ConstantInt::get(i32Ty, 1)));
        }

        builder->CreateCall(moveCursorFn, { clampedX, destY });
    }

    void visit(PrintItem::Spc const& x)
    {
        if (!x.count.empty())
            if (int const n{ std::stoi(x.count) }; n > 0)
            {
                std::string const s(n, ' ');
                emitString(s);
            }
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

    llvm::Value* emitClamp(
    llvm::Value* value,
    llvm::Value* min,
    llvm::Value* max) const
    {
        llvm::Value* ltMin{
            builder->CreateICmpSLT(value, min)
        };

        llvm::Value* gtMax{
            builder->CreateICmpSGT(value, max)
        };

        llvm::Value* minSel{
            builder->CreateSelect(ltMin, min, value)
        };

        llvm::Value* maxSel{
            builder->CreateSelect(gtMax, max, minSel)
        };

        return maxSel;
    }

    llvm::AllocaInst* getOrCreateVariable(std::string const& name)
    {
        if (variables.contains(name))
            return variables[name];

        llvm::BasicBlock& entryBlock{ builder->GetInsertBlock()->getParent()->getEntryBlock() };
        llvm::IRBuilder tmpBuilder{ &entryBlock, entryBlock.begin() };
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
        printNumberFn = module->getOrInsertFunction("pancakes_print_number", llvm::FunctionType::get(voidTy, { floatTy }, false));
        printStringFn = module->getOrInsertFunction("pancakes_print_string", llvm::FunctionType::get(voidTy, { i8PtrTy, i32Ty }, false));
        inputFn = module->getOrInsertFunction("pancakes_input", llvm::FunctionType::get(i32Ty, { i8PtrTy, i32Ty }, false));
        printIntFn = module->getOrInsertFunction("pancakes_print_int", llvm::FunctionType::get(voidTy, { i32Ty }, false));

        llvm::StructType* winStruct{ llvm::StructType::create(context, "WindowsSize") };
        winStruct->setBody({ i32Ty, i32Ty }, false);
        getWindowsSizeFn = module->getOrInsertFunction("pancakes_get_windows_size", llvm::FunctionType::get(winStruct, false));

        moveCursorFn = module->getOrInsertFunction("pancakes_move_cursor_to", llvm::FunctionType::get(voidTy, { i32Ty, i32Ty }, false));

        llvm::PointerType* i32PtrTy{ llvm::PointerType::getUnqual(i32Ty) };
        getCursorPosFn = module->getOrInsertFunction("pancakes_get_cursor_pos", llvm::FunctionType::get(voidTy, { i32PtrTy, i32PtrTy }, false));
    }


    void createReusableGlobals()
    {
        std::string const s(fieldWidth, ' ');
        auto* globalConst{  llvm::ConstantDataArray::getString(context, s, true) };
        spacesGlobal = new llvm::GlobalVariable(
                *module,
                globalConst->getType(),
                true,
                llvm::GlobalValue::PrivateLinkage,
                globalConst,
                "field_spaces"
            );
    }
};