#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace SocketLoader {

    namespace Il2CppLayout {

        struct ObjectHeader {
            void* klass;
            void* monitor;
        };

        struct ArrayHeader {
            ObjectHeader object;
            void* bounds;
            uintptr_t maxLength;
        };

        struct StringHeader {
            ObjectHeader object;
            int32_t length;
            uint16_t firstChar;
        };

        struct MethodInfoHeader {
            void* methodPointer;
        };

        static_assert(sizeof(void*) == 8, "PCBS2.SocketLoader requires x64");
        static_assert(sizeof(ObjectHeader) == 0x10, "Unexpected IL2CPP object header");
        static_assert(sizeof(ArrayHeader) == 0x20, "Unexpected IL2CPP array header");
        static_assert(
            offsetof(StringHeader, length) == 0x10,
            "Unexpected IL2CPP string length offset");
        static_assert(
            offsetof(StringHeader, firstChar) == 0x14,
            "Unexpected IL2CPP string data offset");
        static_assert(
            offsetof(MethodInfoHeader, methodPointer) == 0,
            "Unexpected MethodInfo method-pointer offset");

        inline void* ObjectClass(const void* object) {
            if (!object)
                return nullptr;

            return reinterpret_cast<const ObjectHeader*>(object)->klass;
        }

        template<typename T>
        inline T* ArrayData(void* array) {
            if (!array)
                return nullptr;

            return reinterpret_cast<T*>(
                reinterpret_cast<uintptr_t>(array) +
                sizeof(ArrayHeader));
        }

        template<typename T>
        inline const T* ArrayData(const void* array) {
            if (!array)
                return nullptr;

            return reinterpret_cast<const T*>(
                reinterpret_cast<uintptr_t>(array) +
                sizeof(ArrayHeader));
        }

        inline uintptr_t ArrayLengthFromHeader(const void* array) {
            if (!array)
                return 0;

            return reinterpret_cast<const ArrayHeader*>(array)->maxLength;
        }

        inline bool ArrayLayoutMatches(
            const void* array,
            uintptr_t apiLength)
        {
            return array &&
                ObjectClass(array) != nullptr &&
                ArrayLengthFromHeader(array) == apiLength;
        }

        inline int32_t StringLength(const void* stringObject) {
            if (!stringObject)
                return -1;

            return reinterpret_cast<const StringHeader*>(
                stringObject)->length;
        }

        inline const uint16_t* StringChars(const void* stringObject) {
            if (!stringObject)
                return nullptr;

            return &reinterpret_cast<const StringHeader*>(
                stringObject)->firstChar;
        }

        inline void* MethodPointer(void* methodInfo) {
            if (!methodInfo)
                return nullptr;

            return reinterpret_cast<MethodInfoHeader*>(
                methodInfo)->methodPointer;
        }

    }

    class Logger;

    typedef void* (*il2cpp_domain_get_t)();
    typedef void** (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* size);
    typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
    typedef const char* (*il2cpp_image_get_name_t)(void* image);
    typedef void* (*il2cpp_class_from_name_t)(void* image, const char* ns, const char* name);
    typedef void* (*il2cpp_class_get_field_from_name_t)(void* klass, const char* name);
    typedef int         (*il2cpp_field_get_offset_t)(void* field);
    typedef void        (*il2cpp_field_static_get_value_t)(void* field, void* value);
    typedef void        (*il2cpp_field_static_set_value_t)(void* field, void* value);
    typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argc);
    typedef void* (*il2cpp_array_new_t)(void* klass, uintptr_t count);
    typedef uintptr_t(*il2cpp_array_length_t)(void* array);
    typedef void* (*il2cpp_string_new_t)(const char* str);
    typedef void* (*il2cpp_thread_attach_t)(void* domain);
    typedef void        (*il2cpp_thread_detach_t)(void* thread);
    typedef void* (*il2cpp_class_get_element_class_t)(void* klass);

    struct Il2CppApi {
        il2cpp_domain_get_t                 domain_get = nullptr;
        il2cpp_domain_get_assemblies_t      domain_get_assemblies = nullptr;
        il2cpp_assembly_get_image_t         assembly_get_image = nullptr;
        il2cpp_image_get_name_t             image_get_name = nullptr;
        il2cpp_class_from_name_t            class_from_name = nullptr;
        il2cpp_class_get_field_from_name_t  class_get_field_from_name = nullptr;
        il2cpp_field_get_offset_t           field_get_offset = nullptr;
        il2cpp_field_static_get_value_t     field_static_get_value = nullptr;
        il2cpp_field_static_set_value_t     field_static_set_value = nullptr;
        il2cpp_class_get_method_from_name_t class_get_method_from_name = nullptr;
        il2cpp_array_new_t                  array_new = nullptr;
        il2cpp_array_length_t               array_length = nullptr;
        il2cpp_string_new_t                 string_new = nullptr;
        il2cpp_thread_attach_t              thread_attach = nullptr;
        il2cpp_thread_detach_t              thread_detach = nullptr;
        il2cpp_class_get_element_class_t    class_get_element_class = nullptr;

        bool Resolve(HMODULE gameAssembly, Logger& log);
    };

}