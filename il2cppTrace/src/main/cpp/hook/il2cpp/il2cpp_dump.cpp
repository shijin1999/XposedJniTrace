//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <list>
#include <memory>

#include "xdl.h"
#include "ZhenxiLog.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"
#include "HookUtils.h"
#include "stringUtils.h"
#include "mylibc.h"
#include "ffi.h"
#include "ffi_cxx.h"
#include <sys/mman.h>


#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %p", dlInfo.dli_fbase);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

using namespace std;

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            //LOGD("image name : %s", image->name);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = std::string(outDir).append("FunIl2cpp_dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done! [%s]", outPath.c_str());
}

static string getMethodInfo(const MethodInfo *method) {
    if (method == nullptr) {
        return {};
    }
    std::stringstream outPut;
    uint32_t iflags = 0;
    auto flags = il2cpp_method_get_flags(method, &iflags);
    outPut << get_method_modifier(flags);
    //TODO genericContainerIndex
    auto return_type = il2cpp_method_get_return_type(method);
    if (_il2cpp_type_is_byref(return_type)) {
        outPut << "ref ";
    }
    auto return_class = il2cpp_class_from_type(return_type);
    outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
           << "(";
    auto param_count = il2cpp_method_get_param_count(method);
    for (int i = 0; i < param_count; ++i) {
        auto param = il2cpp_method_get_param(method, i);
        auto attrs = param->attrs;
        if (_il2cpp_type_is_byref(param)) {
            if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                outPut << "out ";
            } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                outPut << "in ";
            } else {
                outPut << "ref ";
            }
        } else {
            if (attrs & PARAM_ATTRIBUTE_IN) {
                outPut << "[In] ";
            }
            if (attrs & PARAM_ATTRIBUTE_OUT) {
                outPut << "[Out] ";
            }
        }
        auto parameter_class = il2cpp_class_from_type(param);
        outPut << il2cpp_class_get_name(parameter_class) << " "
               << il2cpp_method_get_param_name(method, i);
        outPut << ", ";
    }
    if (param_count > 0) {
        outPut.seekp(-2, outPut.cur);
    }
    outPut << ") { }\n";
    return outPut.str();
}

static std::ofstream *mOs = nullptr;
static string saveDir = {};
static bool isFinish = false;

HOOK_DEF(Il2CppObject*, il2cpp_runtime_invoke,
         const MethodInfo *method, void *obj,
         [[maybe_unused]] void **params, Il2CppException **exc) {
    std::stringstream imageOutput;
    auto *clazz = il2cpp_method_get_class(method);
    if (clazz != nullptr) {
        auto *image = il2cpp_class_get_image(clazz);
        if (image != nullptr) {
            imageOutput << "[" << il2cpp_image_get_name(image) << "] ";
        }
        imageOutput << "[" << il2cpp_class_get_name(clazz) << "] ";
    }
    const string &methodInfo = getMethodInfo(method);
    imageOutput << methodInfo;
    auto pointer = method->methodPointer;
    if (pointer) {
        imageOutput << " 0x" << ((uint64_t) pointer - il2cpp_base);
    }
    imageOutput << "\n";
    LOGE("%s", imageOutput.str().c_str())
    //save file
    *mOs << imageOutput.str();
    return orig_il2cpp_runtime_invoke(method, obj, params, exc);
}

void hook_invoke(void *handle, const char *outDir) {
    saveDir = std::string(outDir).append("FunIl2cpp_tracer.txt");
    LOGI("il2cpp_tracer tracer... %s", saveDir.c_str())
    if (mOs == nullptr) {
        mOs = new std::ofstream(saveDir);
    }
    bool isSuccess = HookUtils::Hooker(
            (void *) il2cpp_runtime_invoke,
            (void *) new_il2cpp_runtime_invoke,
            (void **) &orig_il2cpp_runtime_invoke);
    LOGE("il2cpp_runtime_invoke hook finish  %d", isSuccess)
}


static size_t hookSize = 0;

// 将 IL2CPP 类型转换为 libffi 类型
ffi_type *il2cpp_type_to_ffi_type(const Il2CppType *type) {
    switch (type->type) {
        case IL2CPP_TYPE_VOID:
            return &ffi_type_void;
        case IL2CPP_TYPE_BOOLEAN:
            return &ffi_type_uint8;
        case IL2CPP_TYPE_CHAR:
            return &ffi_type_sint16;
        case IL2CPP_TYPE_I1:
            return &ffi_type_sint8;
        case IL2CPP_TYPE_U1:
            return &ffi_type_uint8;
        case IL2CPP_TYPE_I2:
            return &ffi_type_sint16;
        case IL2CPP_TYPE_U2:
            return &ffi_type_uint16;
        case IL2CPP_TYPE_I4:
            return &ffi_type_sint32;
        case IL2CPP_TYPE_U4:
            return &ffi_type_uint32;
        case IL2CPP_TYPE_I8:
            return &ffi_type_sint64;
        case IL2CPP_TYPE_U8:
            return &ffi_type_uint64;
        case IL2CPP_TYPE_R4:
            return &ffi_type_float;
        case IL2CPP_TYPE_R8:
            return &ffi_type_double;
        case IL2CPP_TYPE_STRING:
        case IL2CPP_TYPE_CLASS:
        case IL2CPP_TYPE_OBJECT:
        case IL2CPP_TYPE_ARRAY:
        case IL2CPP_TYPE_SZARRAY:
            return &ffi_type_pointer;
        default: {
            return &ffi_type_void;
        }
    }
}

struct ClosureUserData {
    const MethodInfo *method;
    void *orig_function_pointer;
};

static void ffi_closure_func(ffi_cif *cif, void *ret, void **args, void *user_data) {
    if (cif == nullptr) {
        LOGI("ffi_closure_func cif ==null")
        return;
    }
    auto closure_data = static_cast<ClosureUserData *>(user_data);
    auto method = closure_data->method;
    if (method == nullptr) {
        LOGI("ffi_closure_func method ==null")
        return;
    }
    void *orig_function_pointer = closure_data->orig_function_pointer;
    if (orig_function_pointer == nullptr) {
        LOGI("ffi_closure_func orig_function_pointer ==null")
        return;
    }
    if (ret == nullptr) {
        LOGE("Invalid ret pointer in ffi_closure_func");
        return;
    }
    if (args == nullptr) {
        LOGE("Invalid args pointer in ffi_closure_func");
        return;
    }
    LOGE("Method called: %s", getMethodInfo(method).c_str())
    ffi_call(cif, FFI_FN(orig_function_pointer), ret, args);
    LOGE("Method called success ! : %s", getMethodInfo(method).c_str())
}

ffi_type **get_ffi_arg_types(const MethodInfo *method, uint32_t param_count) {
    uint32_t iflags = 0;
    auto flags = il2cpp_method_get_flags(method, &iflags);
    bool is_instance_method = (flags & METHOD_ATTRIBUTE_STATIC) == 0;
    //il2cpp可能会存在隐藏的this指针
    uint32_t total_param_count = param_count + (is_instance_method ? 1 : 0);
    auto **arg_types = static_cast<ffi_type **>(malloc(sizeof(ffi_type *) * total_param_count));
    if (arg_types == nullptr) {
        return nullptr;
    }
    int offset = 0;
    if (is_instance_method) {
        arg_types[0] = &ffi_type_pointer;
        offset = 1;
    }
    for (int i = 0; i < param_count; ++i) {
        const Il2CppType *param_type = il2cpp_method_get_param(method, i);
        arg_types[i + offset] = il2cpp_type_to_ffi_type(param_type);
        LOGD("get_ffi_arg_types: arg_type[%d] = %d", i + offset, (int) arg_types[i + offset]->type);
    }
    return arg_types;
}

ffi_type *get_ffi_ret_type(const MethodInfo *method) {
    const Il2CppType *ret_type = il2cpp_method_get_return_type(method);
    return il2cpp_type_to_ffi_type(ret_type);
}

std::unique_ptr<ffi_closure, void (*)(void *)> create_ffi_closure(ffi_cif *cif, void *user_data) {
    ffi_closure *closure;
    void *closure_func;

    closure = static_cast<ffi_closure *>(ffi_closure_alloc(sizeof(ffi_closure), &closure_func));
    if (!closure) {
        LOGE("Failed to allocate closure");
        return {nullptr, [](void *) {}};
    }
    return {closure, ffi_closure_free};
}

bool hook_il2cpp_method(const MethodInfo *method, void *pointer) {
    uint32_t param_count = il2cpp_method_get_param_count(method);
    std::unique_ptr<ffi_type *[], void (*)(void *)> arg_types(
            get_ffi_arg_types(method, param_count), free);
    if (!arg_types) {
        LOGE("Failed to allocate arg_types array");
        return false;
    }

    uint32_t iflags = 0;
    auto flags = il2cpp_method_get_flags(method, &iflags);
    bool is_instance_method = (flags & METHOD_ATTRIBUTE_STATIC) == 0;
    uint32_t total_param_count = param_count + (is_instance_method ? 1 : 0);

    ffi_type *ret_type = get_ffi_ret_type(method);
    ffi_cif cif;
    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_param_count, ret_type, arg_types.get());
    LOGI("cif: abi = %d, nargs = %u, rtype = %d", cif.abi, cif.nargs, cif.rtype->type);
    auto closure = create_ffi_closure(&cif, (void *) method);
    if (!closure) {
        LOGE("Failed to create closure");
        return false;
    }
    void *orig_function_pointer = nullptr;
    bool isSuccess = HookUtils::Hooker(pointer,
                                       closure.get(),
                                       (void **) &orig_function_pointer);
    if (isSuccess && orig_function_pointer != nullptr) {
        auto *user_data = new ClosureUserData{method, orig_function_pointer};
        ffi_closure *raw_closure = closure.release(); // 获取原始指针并防止unique_ptr删除closure
        ffi_status status = ffi_prep_closure_loc
                (raw_closure, &cif, ffi_closure_func, user_data, raw_closure);
        if (status != FFI_OK) {
            LOGE("Failed to prepare closure");
            ffi_closure_free(closure.get());
            delete user_data;
            return false;
        }
        hookSize++;
    } else {
        LOGE("hook method fail ");
    }
    return isSuccess;
}

void il2cpp_tracer(const char *outDir) {
    //HookUtils::startBranchTrampoline();
    saveDir = std::string(outDir).append("FunIl2cpp_tracer.txt");
    LOGI("il2cpp_tracer tracer... %s", saveDir.c_str())
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    if (il2cpp_image_get_class) {
        hookSize = 0;
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            auto imageName = il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto tempKlass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(tempKlass));
                auto *klass = il2cpp_class_from_type(type);
                void *iter = nullptr;
                while (auto method = il2cpp_class_get_methods(klass, &iter)) {
                    auto pointer = method->methodPointer;
                    if (pointer) {
                        const string &methodInfo = getMethodInfo(method);
                        const char *clazzName = il2cpp_class_get_name(klass);
                        if (StringUtils::containsInsensitive(methodInfo, "fog")) {
                            auto methodInfoStr = string(clazzName) + " " + methodInfo;
                            if (hook_il2cpp_method(method, (void *) pointer)) {
                                LOGI("hook method info success  %s  size-> %zu  %p",
                                     methodInfoStr.c_str(), hookSize,((char*)pointer-il2cpp_base))
                            } else {
                                LOGE("Failed to hook method: %s", methodInfoStr.c_str());
                            }
                        }
                    }
                    if (iter == nullptr) {
                        break;
                    }
                }
            }
        }
        LOGE("hook finish %zu ", hookSize)
    }
}

