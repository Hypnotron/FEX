#include "clang/AST/RecursiveASTVisitor.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string_view>

#include <openssl/sha.h>

#include "interface.h"

struct FunctionParams {
    std::vector<clang::QualType> param_types;
};

/**
 * Guest<->Host transition point.
 *
 * These are normally used to translate the public API of the guest to host
 * function calls (ThunkedAPIFunction), but a thunk library may also define
 * internal thunks that don't correspond to any function in the implemented
 * API.
 */
struct ThunkedFunction : FunctionParams {
    std::string function_name;
    clang::QualType return_type;

    clang::FunctionDecl* decl;
};

/**
 * Function that is part of the API of the thunked library.
 *
 * For each of these, there is:
 * - A publicly visible guest entrypoint (usually auto-generated but may be manually defined)
 * - A pointer to the native host library function loaded through dlsym (or a user-provided function specified via host_loader)
 * - A ThunkedFunction with the same function_name (possibly suffixed with _internal)
 */
struct ThunkedAPIFunction : FunctionParams {
    std::string function_name;

    clang::QualType return_type;
};

static std::vector<ThunkedFunction> thunks;
static std::vector<ThunkedAPIFunction> thunked_api;

class ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
    clang::ASTContext& context;

    using ClangDiagnosticAsException = std::pair<clang::SourceLocation, unsigned>;

    template<std::size_t N>
    [[nodiscard]] ClangDiagnosticAsException Error(clang::SourceLocation loc, const char (&message)[N]) {
        auto id = context.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Error, message);
        return std::pair(loc, id);
    }

public:
    ASTVisitor(clang::ASTContext& context_) : context(context_) {
    }

    /**
     * Matches "template<> struct fex_gen_config<LibraryFunc> { ... }"
     */
    bool VisitClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl* decl) try {
        if (decl->getName() != "fex_gen_config") {
            return true;
        }

        if (decl->getSpecializationKind() == clang::TSK_ExplicitInstantiationDefinition) {
            throw Error(decl->getBeginLoc(), "fex_gen_config may not be partially specialized\n");
        }

        const auto& template_args = decl->getTemplateArgs();
        assert(template_args.size() == 1);

        auto emitted_function = llvm::dyn_cast<clang::FunctionDecl>(template_args[0].getAsDecl());
        assert(emitted_function && "Argument is not a function");
        auto return_type = emitted_function->getReturnType();

        if (return_type->isFunctionPointerType()) {
            throw Error(decl->getBeginLoc(),
                        "Function pointer return types require explicit annotation\n");
        }

        // TODO: Use the types as written in the signature instead?
        ThunkedFunction data;
        data.function_name = emitted_function->getName().str();
        data.return_type = return_type;

        data.decl = emitted_function;

        for (auto* param : emitted_function->parameters()) {
            data.param_types.push_back(param->getType());

            if (param->getType()->isFunctionPointerType()) {
                throw Error(param->getBeginLoc(), "Function pointer parameters are not supported\n");
            }
        }

        thunked_api.push_back(ThunkedAPIFunction { (const FunctionParams&)data, data.function_name, data.return_type });

        thunks.push_back(std::move(data));

        return true;
    } catch (ClangDiagnosticAsException& exception) {
        context.getDiagnostics().Report(exception.first, exception.second);
        return false;
    }
};

class ASTConsumer : public clang::ASTConsumer {
public:
    void HandleTranslationUnit(clang::ASTContext& context) override {
        ASTVisitor{context}.TraverseDecl(context.getTranslationUnitDecl());
    }
};

FrontendAction::FrontendAction(const std::string& libname_, const OutputFilenames& output_filenames_)
    : libname(libname_), output_filenames(output_filenames_) {
    thunks.clear();
    thunked_api.clear();
}

void FrontendAction::EndSourceFileAction() {
    static auto format_decl = [](clang::QualType type, const std::string_view& name) {
        if (type->isFunctionPointerType()) {
            auto signature = type.getAsString();
            const char needle[] = { '(', '*', ')' };
            auto it = std::search(signature.begin(), signature.end(), std::begin(needle), std::end(needle));
            if (it == signature.end()) {
                // It's *probably* a typedef, so this should be safe after all
                return signature + " " + std::string(name);
            } else {
                signature.insert(it + 2, name.begin(), name.end());
                return signature;
            }
        } else {
            return type.getAsString() + " " + std::string(name);
        }
    };

    auto format_struct_members = [](const FunctionParams& params, const char* indent) {
        std::string ret;
        for (std::size_t idx = 0; idx < params.param_types.size(); ++idx) {
            ret += indent + format_decl(params.param_types[idx].getUnqualifiedType(), "a_" + std::to_string(idx)) + ";\n";
        }
        return ret;
    };

    auto format_function_args = [](const FunctionParams& params) {
        std::string ret;
        for (std::size_t idx = 0; idx < params.param_types.size(); ++idx) {
            ret += "args->a_" + std::to_string(idx) + ", ";
        }
        // drop trailing ", "
        ret.resize(ret.size() > 2 ? ret.size() - 2 : 0);
        return ret;
    };

    auto format_function_params = [](const FunctionParams& params) {
        std::string ret;
        for (std::size_t idx = 0; idx < params.param_types.size(); ++idx) {
            auto& type = params.param_types[idx];
            ret += format_decl(type, "a_" + std::to_string(idx)) + ", ";
        }
        // drop trailing ", "
        ret.resize(ret.size() > 2 ? ret.size() - 2 : 0);
        return ret;
    };

    auto get_sha256 = [this](const std::string& function_name) {
        std::string sha256_message = libname + ":" + function_name;
        std::vector<unsigned char> sha256(SHA256_DIGEST_LENGTH);
        SHA256(reinterpret_cast<const unsigned char*>(sha256_message.data()),
               sha256_message.size(),
               sha256.data());
        return sha256;
    };

    if (!output_filenames.thunks.empty()) {
        std::ofstream file(output_filenames.thunks);

        file << "extern \"C\" {\n";
        for (auto& thunk : thunks) {
            const auto& function_name = thunk.function_name;
            auto sha256 = get_sha256(function_name);
            file << "MAKE_THUNK(" << libname << ", " << function_name << ", \"";
            bool first = true;
            for (auto c : sha256) {
                file << (first ? "" : ", ") << "0x" << std::hex << std::setw(2) << std::setfill('0') << +c;
                first = false;
            }
            file << "\")\n";
        }

        file << "}\n";
    }

    if (!output_filenames.function_packs_public.empty()) {
        std::ofstream file(output_filenames.function_packs_public);

        file << "extern \"C\" {\n";
        for (auto& data : thunked_api) {
            const auto& function_name = data.function_name;

            file << "__attribute__((alias(\"fexfn_pack_" << function_name << "\"))) auto " << function_name << "(";
            for (std::size_t idx = 0; idx < data.param_types.size(); ++idx) {
                auto& type = data.param_types[idx];
                file << (idx == 0 ? "" : ", ") << format_decl(type, "a_" + std::to_string(idx));
            }
            file << ") -> " << data.return_type.getAsString() << ";\n";
        }
        file << "}\n";
    }

    if (!output_filenames.function_packs.empty()) {
        std::ofstream file(output_filenames.function_packs);

        file << "extern \"C\" {\n";
        for (auto& data : thunks) {
            const auto& function_name = data.function_name;
            bool is_void = data.return_type->isVoidType();
            file << "static auto fexfn_pack_" << function_name << "(";
            for (std::size_t idx = 0; idx < data.param_types.size(); ++idx) {
                auto& type = data.param_types[idx];
                file << (idx == 0 ? "" : ", ") << format_decl(type, "a_" + std::to_string(idx));
            }
            // Using trailing return type as it makes handling function pointer returns much easier
            file << ") -> " << data.return_type.getAsString() << " {\n";
            file << "  struct {\n";
            for (std::size_t idx = 0; idx < data.param_types.size(); ++idx) {
                auto& type = data.param_types[idx];
                file << "    " << format_decl(type.getUnqualifiedType(), "a_" + std::to_string(idx)) << ";\n";
            }
            if (!is_void) {
                file << "    " << format_decl(data.return_type, "rv") << ";\n";
            } else if (data.param_types.size() == 0) {
                // Avoid "empty struct has size 0 in C, size 1 in C++" warning
                file << "    char force_nonempty;\n";
            }
            file << "  } args;\n";
            for (std::size_t idx = 0; idx < data.param_types.size(); ++idx) {
                file << "  args.a_" << idx << " = a_" << idx << ";\n";
            }
            file << "  fexthunks_" << libname << "_" << function_name << "(&args);\n";
            if (!is_void) {
                file << "  return args.rv;\n";
            }
            file << "}\n";
        }
        file << "}\n";
    }

    if (!output_filenames.function_unpacks.empty()) {
        std::ofstream file(output_filenames.function_unpacks);

        file << "extern \"C\" {\n";
        for (auto& thunk : thunks) {
            const auto& function_name = thunk.function_name;
            bool is_void = thunk.return_type->isVoidType();

            file << "struct fexfn_packed_args_" << libname << "_" << function_name << " {\n";
            file << format_struct_members(thunk, "  ");
            if (!is_void) {
                file << "  " << format_decl(thunk.return_type, "rv") << ";\n";
            } else if (thunk.param_types.size() == 0) {
                // Avoid "empty struct has size 0 in C, size 1 in C++" warning
                file << "    char force_nonempty;\n";
            }
            file << "};\n";

            file << "static void fexfn_unpack_" << libname << "_" << function_name << "(fexfn_packed_args_" << libname << "_" << function_name << "* args) {\n";
            file << (is_void ? "  " : "  args->rv = ") << "fexldr_ptr_" << libname << "_" << function_name << "(" << format_function_args(thunk) << ");\n";
            file << "}\n";
        }

        file << "}\n";
    }

    if (!output_filenames.tab_function_unpacks.empty()) {
        std::ofstream file(output_filenames.tab_function_unpacks);

        for (auto& thunk : thunks) {
            const auto& function_name = thunk.function_name;
            auto sha256 = get_sha256(function_name);

            file << "{(uint8_t*)\"";
            for (auto c : sha256) {
                file << "\\x" << std::hex << std::setw(2) << std::setfill('0') << +c;
            }
            file << "\", &fexfn_type_erased_unpack<fexfn_unpack_" << libname << "_" << function_name << ">}, // " << libname << ":" << function_name << "\n";
        }
    }

    if (!output_filenames.ldr.empty()) {
        std::ofstream file(output_filenames.ldr);

        file << "static void* fexldr_ptr_" << libname << "_so;\n";
        file << "extern \"C\" bool fexldr_init_" << libname << "() {\n";
        file << "  fexldr_ptr_" << libname << "_so = dlopen(\"" << libname << ".so\", RTLD_LOCAL | RTLD_LAZY);\n";
        file << "  if (!fexldr_ptr_" << libname << "_so) { return false; }\n\n";
        for (auto& import : thunked_api) {
            file << "  (void*&)fexldr_ptr_" << libname << "_" << import.function_name << " = dlsym(fexldr_ptr_" << libname << "_so, \"" << import.function_name << "\");\n";
        }
        file << "  return true;\n";
        file << "}\n";
    }

    if (!output_filenames.ldr_ptrs.empty()) {
        std::ofstream file(output_filenames.ldr_ptrs);

        for (auto& import : thunked_api) {
            const auto& function_name = import.function_name;
            file << "using fexldr_type_" << libname << "_" << function_name << " = auto " << "(" << format_function_params(import) << ") -> " << import.return_type.getAsString() << ";\n";
            file << "static fexldr_type_" << libname << "_" << function_name << " *fexldr_ptr_" << libname << "_" << function_name << ";\n";
        }
    }
}

std::unique_ptr<clang::ASTConsumer> FrontendAction::CreateASTConsumer(clang::CompilerInstance&, clang::StringRef) {
    return std::make_unique<ASTConsumer>();
}
