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
#include <algorithm>
#include <string>
#include <vector>


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
#include <main.h>

#include "dobby.h"

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

void* il2cpp_api_init(void *handle) {
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %p", dlInfo.dli_fbase);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return nullptr;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
    return (void*)il2cpp_base;
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
struct ClosureUserData {
    const MethodInfo *method;
    void *orig_function_pointer;
};

std::list<std::shared_ptr<ClosureUserData>> &getMethodInfoList() {
    static std::list<std::shared_ptr<ClosureUserData>> methodInfoList;
    return methodInfoList;
}
void instrument_callback(void *address, DobbyRegisterContext *reg_ctx) {
    for (const auto &info: getMethodInfoList()) {
        if (info->orig_function_pointer == address) {
            LOGI(">>>>>>>> %s %p", getMethodInfo(info->method).c_str(),(char*)address-il2cpp_base)
            return;
        }
    }
    LOGI("not find callback method info %p", address)
}
bool hook_il2cpp_method(const MethodInfo *method) {
    void *method_address = (void *) method->methodPointer;
    if (method_address == nullptr) {
        LOGE("Method address is NULL");
        return false;
    }

    auto pData = std::make_shared<ClosureUserData>();
    pData->method = method;
    pData->orig_function_pointer = method_address;

    if (HookUtils::addTrampoline(method_address, instrument_callback)) {
        getMethodInfoList().push_back(pData);
        return true;
    } else {
        return false;
    }
}
bool ishook(const string& msg){
    if(isTraceALL){
        return true;
    }
    return std::any_of(filter_list.begin(),
                       filter_list.end(),
                       [&msg](const string& str) {
        return StringUtils::containsInsensitive(msg, str);
    });
}
void il2cpp_tracer(const char *outDir) {
    HookUtils::startBranchTrampoline();
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
                        auto methodInfoStr = string(clazzName) + " " + methodInfo;
                        if(ishook(methodInfoStr)){
                            if (hook_il2cpp_method(method)) {
                                hookSize++;
                                LOGI("hook method info success  %s  size-> %zu  %p",
                                     methodInfoStr.c_str(), hookSize, ((char *) pointer - il2cpp_base))
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

