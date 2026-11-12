
#include "logging.h"
#include "platform.h"
#include "so_util.h"
#include "thunk_gen.h"
#include <cstring>
#include <stdlib.h>

#define verbose(tag, format, ...)

typedef enum {
    IL2CPP_UNHANDLED_POLICY_LEGACY,
    IL2CPP_UNHANDLED_POLICY_CURRENT
} Il2CppRuntimeUnhandledExceptionPolicy;

static void* cached_domain = NULL;

// Initialization functions
static void* (*mono_jit_init)(const char*) = NULL;
static void (*mono_icall_init)() = NULL;
static void (*mono_add_internal_call)(const char*, const void*) = NULL;
static void (*mono_unity_install_unitytls_interface)(void*) = NULL;

// Global getters and setters
static void (*mono_set_dirs)(const char*, const char*) = NULL;
static void (*mono_set_find_plugin_callback)(void*) = NULL;
static void* (*mono_get_corlib)() = NULL;
static void (*mono_config_parse)(const char*) = NULL;
static void (*mono_dllmap_insert)(void*, const char*, const char*, const char*, const char*) = NULL;
// static void (*mono_set_config_dir)(const char*) = NULL;
// static void (*mono_unity_set_data_dir)(const char*) = NULL;

// Runtime functions
static void (*mono_runtime_unhandled_exception_policy_set)(Il2CppRuntimeUnhandledExceptionPolicy policy) = NULL;
static void* (*mono_runtime_invoke)(void*, void*, void*, void*) = NULL;

// Garbage collection functions
static uint32_t (*mono_gchandle_new)(void*, bool) = NULL;
static void (*mono_gchandle_free)(uint32_t) = NULL;
static void (*mono_gc_collect)(int) = NULL;
static uint8_t (*mono_gc_is_incremental)() = NULL;
static void (*mono_gc_wbarrier_set_field)(void*, void*, void*) = NULL;
// static void (*mono_stop_gc_world)

// Thread functions
static void* (*mono_thread_current)() = NULL;
static void* (*mono_thread_attach)(void*) = NULL;

// Domain functions
static void* (*mono_domain_get)() = NULL;
static void* (*mono_domain_assembly_open)(void*, const char*) = NULL;

// Assembly functions
static void* (*mono_assembly_get_image)(void*) = NULL;
static void* (*mono_assembly_get_name)(void*) = NULL;
static char* (*mono_stringify_assembly_name)(void*) = NULL;

// Image functions
static void* (*mono_image_get_assembly)(void*) = NULL;
static int (*mono_image_get_table_rows)(void*, int) = NULL;

// Class functions
static void* (*mono_class_from_name)(void*, const char*, const char*) = NULL;
static void* (*mono_class_get_methods)(void*, void*) = NULL;
static const char* (*mono_class_get_name)(void*) = NULL;
static void* (*mono_class_get_image)(void*) = NULL;
static void* (*mono_class_get_parent)(void*) = NULL;
static void* (*mono_class_get_nested_types)(void*, void*) = NULL;
static void* (*mono_class_get_field_from_name)(void*, const char*) = NULL;
static void* (*mono_class_from_mono_type)(void*) = NULL;
static int32_t (*mono_class_is_subclass_of)(void*, void*, int32_t) = NULL;
static int32_t (*mono_unity_class_is_abstract)(void*) = NULL;
static int32_t (*mono_class_is_generic)(void*) = NULL;
static int32_t (*mono_class_is_inflated)(void*) = NULL;
static void (*mono_class_set_userdata)(void*, void*) = NULL;
static void* (*mono_class_get_nesting_type)(void*) = NULL;
static const char* (*mono_class_get_namespace)(void*) = NULL;
static void* (*mono_class_get_fields)(void*, void*) = NULL;
static int (*mono_class_get_userdata_offset)() = NULL;
static int32_t (*mono_class_is_valuetype)(void*) = NULL;
static int32_t (*mono_class_is_enum)(void*) = NULL;
static int (*mono_class_get_rank)(void*) = NULL;
static void* (*mono_class_get_element_class)(void*) = NULL;
static void* (*mono_class_get_type)(void*) = NULL;
static uint32_t (*mono_class_get_flags)(void*) = NULL;
static int32_t (*mono_unity_class_is_interface)(void*) = NULL;
static void* (*mono_class_enum_basetype)(void*) = NULL;
static int32_t (*mono_class_instance_size)(void*) = NULL;
static int32_t (*mono_class_array_element_size)(void*) = NULL;

// Custom attributes functions
static void* (*mono_custom_attrs_from_class)(void*) = NULL;
static int32_t (*mono_custom_attrs_has_attr)(void*, void*) = NULL;
static void* (*mono_custom_attrs_get_attr)(void*, void*) = NULL;
static void* (*mono_custom_attrs_construct)(void*) = NULL;
static void (*mono_custom_attrs_free)(void*) = NULL;
static void* (*mono_custom_attrs_from_field)(void*, void*) = NULL;

// Method functions
static const char* (*mono_method_get_name)(void*) = NULL;
static int32_t (*unity_mono_method_is_inflated)(void*) = NULL;
static int32_t (*unity_mono_method_is_generic)(void*) = NULL;
static void* (*mono_method_signature)(void*) = NULL;
static void* (*mono_method_get_class)(void*) = NULL;

// Signature functions
static uint32_t (*mono_signature_get_param_count)(void*) = NULL;
static void* (*mono_signature_get_params)(void*, void*) = NULL;
static int32_t (*mono_signature_is_instance)(void*) = NULL;
static void* (*mono_signature_get_return_type)(void*) = NULL;

// Field functions
static uint32_t (*mono_field_get_offset)(void*) = NULL;
static void* (*mono_field_get_type)(void*) = NULL;
static uint32_t (*mono_field_get_flags)(void*) = NULL;
static const char* (*mono_field_get_name)(void*) = NULL;
static void* (*mono_field_get_parent)(void*) = NULL;

// Type functions
static int32_t (*mono_type_is_byref)(void*) = NULL;
static int (*mono_type_get_type)(void*) = NULL;

// Reflection functions
static void* (*mono_reflection_type_get_type)(void*) = NULL;

// Object functions
static void* (*mono_object_get_class)(void*) = NULL;
static void* (*mono_object_new)(void*, void*) = NULL;
static void* (*mono_object_get_virtual_method)(void*, void*) = NULL;

// Array functions
static void* (*mono_array_class_get)(void*, uint32_t) = NULL;
static void* (*mono_array_new)(void*, void*, uintptr_t) = NULL;
static uintptr_t (*mono_array_length)(void*) = NULL;
static int32_t (*mono_array_element_size)(void*) = NULL;

// String functions
static void* (*mono_string_new_len)(void*, const char*, unsigned int) = NULL;
static int (*mono_string_length)(void*) = NULL;
static uint16_t* (*mono_string_chars)(void*) = NULL;
static void* (*mono_string_new_wrapper)(const char*) = NULL;

// Liveness functions
static void* (*mono_unity_liveness_allocate_struct)(void*, unsigned int, void*, void*, void*, void*) = NULL;
static void (*mono_unity_liveness_calculation_from_root)(void*, void*) = NULL;
static void (*mono_unity_liveness_calculation_from_statics)(void*) = NULL;
static void (*mono_unity_liveness_stop_gc_world)(void*) = NULL;
static void (*mono_unity_liveness_finalize)(void*) = NULL;
static void (*mono_unity_liveness_start_gc_world)(void*) = NULL;
static void (*mono_unity_liveness_free_struct)(void*) = NULL;

// Exception functions
static void* (*mono_exception_from_name_msg)(void*, const char*, const char*, const char*) = NULL;
static void (*mono_raise_exception)(void*) = NULL;

void monobridge_init(so_module* mod)
{
    verbose("Monobridge", "Initializing monobridge");

    // Initialization functions
    mono_jit_init = (void* (*)(const char*))so_symbol(mod, "mono_jit_init");
    mono_icall_init = (void (*)())so_symbol(mod, "mono_icall_init");
    mono_add_internal_call = (void (*)(const char*, const void*))so_symbol(mod, "mono_add_internal_call");
    mono_unity_install_unitytls_interface = (void (*)(void*))so_symbol(mod, "mono_unity_install_unitytls_interface");

    // Global getters and setters
    mono_set_dirs = (void (*)(const char*, const char*))so_symbol(mod, "mono_set_dirs");
    mono_set_find_plugin_callback = (void (*)(void*))so_symbol(mod, "mono_set_find_plugin_callback");
    mono_get_corlib = (void* (*)())so_symbol(mod, "mono_get_corlib");
    mono_config_parse = (void (*)(const char*))so_symbol(mod, "mono_config_parse");
    mono_dllmap_insert = (void (*)(void*, const char*, const char*, const char*, const char*))so_symbol(mod, "mono_dllmap_insert");
    // mono_set_config_dir = (void (*)(const char*))so_symbol(mod, "mono_set_config_dir");
    // mono_unity_set_data_dir = (void (*)(const char*))so_symbol(mod, "mono_unity_set_data_dir");

    // Runtime functions
    mono_runtime_unhandled_exception_policy_set = (void (*)(Il2CppRuntimeUnhandledExceptionPolicy policy))so_symbol(mod, "mono_runtime_unhandled_exception_policy_set");
    mono_runtime_invoke = (void* (*)(void*, void*, void*, void*))so_symbol(mod, "mono_runtime_invoke");

    // Garbage collection functions
    mono_gchandle_new = (uint32_t (*)(void*, bool))so_symbol(mod, "mono_gchandle_new");
    mono_gchandle_free = (void (*)(uint32_t))so_symbol(mod, "mono_gchandle_free");
    mono_gc_collect = (void (*)(int))so_symbol(mod, "mono_gc_collect");
    mono_gc_is_incremental = (uint8_t (*)())so_symbol(mod, "mono_gc_is_incremental");
    mono_gc_wbarrier_set_field = (void (*)(void*, void*, void*))so_symbol(mod, "mono_gc_wbarrier_set_field");

    // Thread functions
    mono_thread_current = (void* (*)())so_symbol(mod, "mono_thread_current");
    mono_thread_attach = (void* (*)(void*))so_symbol(mod, "mono_thread_attach");

    // Domain functions
    mono_domain_get = (void* (*)())so_symbol(mod, "mono_domain_get");
    mono_domain_assembly_open = (void* (*)(void*, const char*))so_symbol(mod, "mono_domain_assembly_open");

    // Assembly functions
    mono_assembly_get_image = (void* (*)(void*))so_symbol(mod, "mono_assembly_get_image");
    mono_assembly_get_name = (void* (*)(void*))so_symbol(mod, "mono_assembly_get_name");
    mono_stringify_assembly_name = (char* (*)(void*))so_symbol(mod, "mono_stringify_assembly_name");

    // Image functions
    mono_image_get_assembly = (void* (*)(void*))so_symbol(mod, "mono_image_get_assembly");
    mono_image_get_table_rows = (int (*)(void*, int))so_symbol(mod, "mono_image_get_table_rows");

    // Class functions
    mono_class_from_name = (void* (*)(void*, const char*, const char*))so_symbol(mod, "mono_class_from_name");
    mono_class_get_methods = (void* (*)(void*, void*))so_symbol(mod, "mono_class_get_methods");
    mono_class_get_name = (const char* (*)(void*))so_symbol(mod, "mono_class_get_name");
    mono_class_get_image = (void* (*)(void*))so_symbol(mod, "mono_class_get_image");
    mono_class_get_parent = (void* (*)(void*))so_symbol(mod, "mono_class_get_parent");
    mono_class_get_nested_types = (void* (*)(void*, void*))so_symbol(mod, "mono_class_get_nested_types");
    mono_class_get_field_from_name = (void* (*)(void*, const char*))so_symbol(mod, "mono_class_get_field_from_name");
    mono_class_from_mono_type = (void* (*)(void*))so_symbol(mod, "mono_class_from_mono_type");
    mono_class_is_subclass_of = (int32_t (*)(void*, void*, int32_t))so_symbol(mod, "mono_class_is_subclass_of");
    mono_unity_class_is_abstract = (int32_t (*)(void*))so_symbol(mod, "mono_unity_class_is_abstract");
    mono_class_is_generic = (int32_t (*)(void*))so_symbol(mod, "mono_class_is_generic");
    mono_class_is_inflated = (int32_t (*)(void*))so_symbol(mod, "mono_class_is_inflated");
    mono_class_set_userdata = (void (*)(void*, void*))so_symbol(mod, "mono_class_set_userdata");
    mono_class_get_nesting_type = (void* (*)(void*))so_symbol(mod, "mono_class_get_nesting_type");
    mono_class_get_namespace = (const char* (*)(void*))so_symbol(mod, "mono_class_get_namespace");
    mono_class_get_fields = (void* (*)(void*, void*))so_symbol(mod, "mono_class_get_fields");
    mono_class_get_userdata_offset = (int (*)())so_symbol(mod, "mono_class_get_userdata_offset");
    mono_class_is_valuetype = (int32_t (*)(void*))so_symbol(mod, "mono_class_is_valuetype");
    mono_class_is_enum = (int32_t (*)(void*))so_symbol(mod, "mono_class_is_enum");
    mono_class_get_rank = (int (*)(void*))so_symbol(mod, "mono_class_get_rank");
    mono_class_get_element_class = (void* (*)(void*))so_symbol(mod, "mono_class_get_element_class");
    mono_class_get_type = (void* (*)(void*))so_symbol(mod, "mono_class_get_type");
    mono_class_get_flags = (uint32_t (*)(void*))so_symbol(mod, "mono_class_get_flags");
    mono_unity_class_is_interface = (int32_t (*)(void*))so_symbol(mod, "mono_unity_class_is_interface");
    mono_class_enum_basetype = (void* (*)(void*))so_symbol(mod, "mono_class_enum_basetype");
    mono_class_instance_size = (int32_t (*)(void*))so_symbol(mod, "mono_class_instance_size");
    mono_class_array_element_size = (int32_t (*)(void*))so_symbol(mod, "mono_class_array_element_size");


    // Custom attributes functions
    mono_custom_attrs_from_class = (void* (*)(void*))so_symbol(mod, "mono_custom_attrs_from_class");
    mono_custom_attrs_has_attr = (int32_t (*)(void*, void*))so_symbol(mod, "mono_custom_attrs_has_attr");
    mono_custom_attrs_get_attr = (void* (*)(void*, void*))so_symbol(mod, "mono_custom_attrs_get_attr");
    mono_custom_attrs_construct = (void* (*)(void*))so_symbol(mod, "mono_custom_attrs_construct");
    mono_custom_attrs_free = (void (*)(void*))so_symbol(mod, "mono_custom_attrs_free");
    mono_custom_attrs_from_field = (void* (*)(void*, void*))so_symbol(mod, "mono_custom_attrs_from_field");

    // Method functions
    mono_method_get_name = (const char* (*)(void*))so_symbol(mod, "mono_method_get_name");
    unity_mono_method_is_inflated = (int32_t (*)(void*))so_symbol(mod, "unity_mono_method_is_inflated");
    unity_mono_method_is_generic = (int32_t (*)(void*))so_symbol(mod, "unity_mono_method_is_generic");
    mono_method_signature = (void* (*)(void*))so_symbol(mod, "mono_method_signature");
    mono_method_get_class = (void* (*)(void*))so_symbol(mod, "mono_method_get_class");

    // Signature functions
    mono_signature_get_param_count = (uint32_t (*)(void*))so_symbol(mod, "mono_signature_get_param_count");
    mono_signature_get_params = (void* (*)(void*, void*))so_symbol(mod, "mono_signature_get_params");
    mono_signature_is_instance = (int32_t (*)(void*))so_symbol(mod, "mono_signature_is_instance");
    mono_signature_get_return_type = (void* (*)(void*))so_symbol(mod, "mono_signature_get_return_type");

    // Field functions
    mono_field_get_offset = (uint32_t (*)(void*))so_symbol(mod, "mono_field_get_offset");
    mono_field_get_type = (void* (*)(void*))so_symbol(mod, "mono_field_get_type");
    mono_field_get_flags = (uint32_t (*)(void*))so_symbol(mod, "mono_field_get_flags");
    mono_field_get_name = (const char* (*)(void*))so_symbol(mod, "mono_field_get_name");
    mono_field_get_parent = (void* (*)(void*))so_symbol(mod, "mono_field_get_parent");

    // Type functions
    mono_type_is_byref = (int32_t (*)(void*))so_symbol(mod, "mono_type_is_byref");
    mono_type_get_type = (int (*)(void*))so_symbol(mod, "mono_type_get_type");

    // Reflection functions
    mono_reflection_type_get_type = (void* (*)(void*))so_symbol(mod, "mono_reflection_type_get_type");

    // Object functions
    mono_object_get_class = (void* (*)(void*))so_symbol(mod, "mono_object_get_class");
    mono_object_new = (void* (*)(void*, void*))so_symbol(mod, "mono_object_new");
    mono_object_get_virtual_method = (void* (*)(void*, void*))so_symbol(mod, "mono_object_get_virtual_method");

    // Array functions
    mono_array_class_get = (void* (*)(void*, uint32_t))so_symbol(mod, "mono_array_class_get");
    mono_array_new = (void* (*)(void*, void*, uintptr_t))so_symbol(mod, "mono_array_new");
    mono_array_length = (uintptr_t (*)(void*))so_symbol(mod, "mono_array_length");
    mono_array_element_size = (int32_t (*)(void*))so_symbol(mod, "mono_array_element_size");

    // String functions
    mono_string_new_len = (void* (*)(void*, const char*, unsigned int))so_symbol(mod, "mono_string_new_len");
    mono_string_length = (int (*)(void*))so_symbol(mod, "mono_string_length");
    mono_string_chars = (uint16_t* (*)(void*))so_symbol(mod, "mono_string_chars");
    mono_string_new_wrapper = (void* (*)(const char*))so_symbol(mod, "mono_string_new_wrapper");

    // Liveness functions
    mono_unity_liveness_allocate_struct = (void* (*)(void*, unsigned int, void*, void*, void*, void*))so_symbol(mod, "mono_unity_liveness_allocate_struct");
    mono_unity_liveness_calculation_from_root = (void (*)(void*, void*))so_symbol(mod, "mono_unity_liveness_calculation_from_root");
    mono_unity_liveness_calculation_from_statics = (void (*)(void*))so_symbol(mod, "mono_unity_liveness_calculation_from_statics");
    mono_unity_liveness_stop_gc_world = (void (*)(void*))so_symbol(mod, "mono_unity_liveness_stop_gc_world");
    mono_unity_liveness_finalize = (void (*)(void*))so_symbol(mod, "mono_unity_liveness_finalize");
    mono_unity_liveness_start_gc_world = (void (*)(void*))so_symbol(mod, "mono_unity_liveness_start_gc_world");
    mono_unity_liveness_free_struct = (void (*)(void*))so_symbol(mod, "mono_unity_liveness_free_struct");

    // Exception functions
    mono_exception_from_name_msg = (void* (*)(void*, const char*, const char*, const char*))so_symbol(mod, "mono_exception_from_name_msg");
    mono_raise_exception = (void (*)(void*))so_symbol(mod, "mono_raise_exception");


    mono_icall_init();
    mono_set_dirs("assets/bin/Data/Managed/", "assets/bin/Data/Managed");
    mono_config_parse(NULL);
    mono_dllmap_insert(NULL, "System.Native", NULL, "libmono-native.so", NULL);
}

void* il2cpp_init_impl(const char* domain)
{
    verbose("Monobridge", "Bridged call: il2cpp_init %s", domain);
    if (cached_domain == NULL)
        cached_domain = mono_jit_init(domain);
    verbose("Monobridge", "  -> il2cpp_init = %p", cached_domain);
    return cached_domain;
}

void il2cpp_init_utf16_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_init_utf16");
    exit(-2);
}

void il2cpp_shutdown_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_shutdown"); // Does not exist in mono and we don't really care. Memory will be cleaned up by the OS anyway when exiting.
}

void il2cpp_set_config_dir_impl(const char* config_path)
{
    verbose("Monobridge", "Stubbed call: il2cpp_set_config_dir %s", config_path); // We set directories during our own initialization
    // return mono_set_config_dir(config_path);
}

void il2cpp_set_data_dir_impl(const char* data_path)
{
    verbose("Monobridge", "Stubbed call: il2cpp_set_data_dir %s", data_path); // We set directories during our own initialization
    // return mono_unity_set_data_dir(data_path);
}

void il2cpp_set_temp_dir_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_set_temp_dir");
    exit(-2);
}

void il2cpp_set_commandline_arguments_impl(int argc, const char* const argv[], const char* basedir)
{
    verbose("Monobridge", "Stubbed call: il2cpp_set_commandline_arguments"); // Should probably be implemented at some point, some games use these
}

void il2cpp_set_commandline_arguments_utf16_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_set_commandline_arguments_utf16");
    exit(-2);
}

void il2cpp_set_config_utf16_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_set_config_utf16");
    exit(-2);
}

void il2cpp_set_config_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_set_config"); // No config needed
}

void il2cpp_set_memory_callbacks_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_set_memory_callbacks");
    exit(-1);
}

void* il2cpp_get_corlib_impl()
{
    verbose("Monobridge", "Bridged call: il2cpp_get_corlib");
    return mono_get_corlib();
}

void il2cpp_add_internal_call_impl(const char* name, void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_add_internal_call %s %p", name, method);
    if (cached_domain == NULL)
        il2cpp_init_impl("IL2CPP Root Domain");
    return mono_add_internal_call(name, method);
}

void il2cpp_resolve_icall_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_resolve_icall");
    exit(-2);
}

void il2cpp_alloc_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_alloc");
    exit(-1);
}

void il2cpp_free_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_free");
    exit(-1);
}

void* il2cpp_array_class_get_impl(void* klass, uint32_t rank)
{
    verbose("Monobridge", "Bridged call: il2cpp_array_class_get");
    return mono_array_class_get(klass, rank);
}

uint32_t il2cpp_array_length_impl(void* array)
{
    verbose("Monobridge", "Bridged call: il2cpp_array_length");
    return (uint32_t)mono_array_length(array);
}

void il2cpp_array_get_byte_length_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_array_get_byte_length");
    exit(-1);
}

void* il2cpp_array_new_impl(void* klass, uintptr_t length)
{
    verbose("Monobridge", "Bridged call: il2cpp_array_new");
    return mono_array_new(cached_domain, klass, length);
}

void il2cpp_array_new_specific_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_array_new_specific");
    exit(-1);
}

void il2cpp_array_new_full_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_array_new_full");
    exit(-1);
}

void il2cpp_bounded_array_class_get_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_bounded_array_class_get");
    exit(-1);
}

int il2cpp_array_element_size_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_array_element_size");
    return (int)mono_array_element_size(klass);
}

void* il2cpp_assembly_get_image_impl(void* assembly)
{
    verbose("Monobridge", "Bridged call: il2cpp_assembly_get_image");
    return mono_assembly_get_image(assembly);
}

void il2cpp_class_for_each_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_class_for_each");
    exit(-1);
}

void* il2cpp_class_enum_basetype_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_enum_basetype");
    return mono_class_enum_basetype(klass);
}

bool il2cpp_class_is_generic_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_generic");
    return (bool)mono_class_is_generic(klass);
}

bool il2cpp_class_is_inflated_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_inflated");
    return (bool)mono_class_is_inflated(klass);
}

void il2cpp_class_is_assignable_from_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_is_assignable_from");
    exit(-2);
}

bool il2cpp_class_is_subclass_of_impl(void* klass, void* klass2, bool check_interfaces)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_subclass_of");
    return (bool)mono_class_is_subclass_of(klass, klass2, (int32_t)check_interfaces);
}

bool il2cpp_class_has_parent_impl(void* klass, void* klass2)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_has_parent");
    return (bool)mono_class_is_subclass_of(klass, klass2, (int32_t)false);
}

void il2cpp_class_from_il2cpp_type_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_class_from_il2cpp_type");
    exit(-1);
}

void* il2cpp_class_from_name_impl(void* image, const char* namespaze, const char* name)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_from_name %s %s", namespaze, name);
    return mono_class_from_name(image, namespaze, name);
}

void* il2cpp_class_from_system_type_impl(void* reflectiontype)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_from_system_type");
    auto type = mono_reflection_type_get_type(reflectiontype);
    return mono_class_from_mono_type(type);
}

void* il2cpp_class_get_element_class_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_element_class");
    return mono_class_get_element_class(klass);
}

void il2cpp_class_get_events_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_events");
    exit(-2);
}

void* il2cpp_class_get_fields_impl(void* klass, void* iter)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_fields");
    return mono_class_get_fields(klass, iter);
}

void* il2cpp_class_get_nested_types_impl(void* klass, void* iter)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_nested_types");
    return mono_class_get_nested_types(klass, iter);
}

void il2cpp_class_get_interfaces_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_interfaces");
    exit(-2);
}

void il2cpp_class_get_properties_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_properties");
    exit(-2);
}

void il2cpp_class_get_property_from_name_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_property_from_name");
    exit(-2);
}

void* il2cpp_class_get_field_from_name_impl(void* klass, const char* name)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_field_from_name");
    return mono_class_get_field_from_name(klass, name);
}

void* il2cpp_class_get_methods_impl(void* klass, void* iter)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_methods");
    return mono_class_get_methods(klass, iter);
}

void il2cpp_class_get_method_from_name_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_method_from_name");
    exit(-2);
}

const char* il2cpp_class_get_name_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_name");
    return mono_class_get_name(klass);
}

void il2cpp_type_get_name_chunked_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_get_name_chunked");
    exit(-1);
}

const char* il2cpp_class_get_namespace_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_namespace");
    return mono_class_get_namespace(klass);
}

void* il2cpp_class_get_parent_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_parent");
    return mono_class_get_parent(klass);
}

void* il2cpp_class_get_declaring_type_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_declaring_type");
    return mono_class_get_nesting_type(klass);
}

int32_t il2cpp_class_instance_size_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_instance_size");
    return mono_class_instance_size(klass);
}

void il2cpp_class_num_fields_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_num_fields");
    exit(-2);
}

bool il2cpp_class_is_valuetype_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_valuetype");
    return (bool)mono_class_is_valuetype(klass);
}

void il2cpp_class_value_size_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_value_size");
    exit(-2);
}

void il2cpp_class_is_blittable_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_class_is_blittable");
    exit(-1);
}

int il2cpp_class_get_flags_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_flags");
    return (int) mono_class_get_flags(klass);
}

bool il2cpp_class_is_abstract_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_abstract");
    return (bool)mono_unity_class_is_abstract(klass);
}

bool il2cpp_class_is_interface_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_interface");
    return (bool) mono_unity_class_is_interface(klass);
}

int il2cpp_class_array_element_size_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_array_element_size");
    return (int)mono_class_array_element_size(klass);
}

void* il2cpp_class_from_type_impl(void* type)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_from_type");
    return mono_class_from_mono_type(type);
}

void* il2cpp_class_get_type_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_type");
    return mono_class_get_type(klass);
}

void il2cpp_class_get_type_token_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_type_token");
    exit(-2);
}

bool il2cpp_class_has_attribute_impl(void* klass, void* klass2)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_has_attribute");
    auto attrs = mono_custom_attrs_from_class(klass);
    if (attrs == NULL)
        return false;

    bool has_attr = (bool)mono_custom_attrs_has_attr(attrs, klass2);
    mono_custom_attrs_free(attrs);
    return has_attr;
}

void il2cpp_class_has_references_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_has_references");
    exit(-2);
}

bool il2cpp_class_is_enum_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_is_enum");
    return (bool)mono_class_is_enum(klass);
}

void* il2cpp_class_get_image_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_image");
    return mono_class_get_image(klass);
}

const char* il2cpp_class_get_assemblyname_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_assemblyname");
    auto image = mono_class_get_image(klass);
    auto assembly = mono_image_get_assembly(image);
    auto aname = mono_assembly_get_name(assembly);
    return mono_stringify_assembly_name(aname);
}

int il2cpp_class_get_rank_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_rank");
    return mono_class_get_rank(klass);
}

void il2cpp_class_get_data_size_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_class_get_data_size");
    exit(-1);
}

void il2cpp_class_get_static_field_data_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_class_get_static_field_data");
    exit(-1);
}

void il2cpp_class_get_bitmap_size_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_bitmap_size");
    exit(-2);
}

void il2cpp_class_get_bitmap_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_class_get_bitmap");
    exit(-2);
}

void il2cpp_stats_dump_to_file_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_stats_dump_to_file");
    exit(-2);
}

void il2cpp_stats_get_value_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_stats_get_value");
    exit(-2);
}

void* il2cpp_domain_get_impl()
{
    verbose("Monobridge", "Bridged call: il2cpp_domain_get");
    cached_domain = mono_domain_get();
    return cached_domain;
}

void* il2cpp_domain_assembly_open_impl(void* domain, const char* name)
{
    verbose("Monobridge", "Bridged call: il2cpp_domain_assembly_open %s", name);
    const char* prefix = "assets/bin/Data/Managed/";
    size_t prefix_len = strlen(prefix);
    size_t filename_len = strlen(name);
    char* path = (char*)malloc(prefix_len + filename_len + 1);
    strcpy(path, prefix);
    strcat(path, name);
    auto result = mono_domain_assembly_open(domain, path);
    free(path);
    return result;
}

void il2cpp_domain_get_assemblies_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_domain_get_assemblies");
    exit(-1);
}

void il2cpp_raise_exception_impl(void* exception)
{
    verbose("Monobridge", "Bridged call: il2cpp_raise_exception");
    return mono_raise_exception(exception);
}

void* il2cpp_exception_from_name_msg_impl(void* image, const char* name_space, const char* name, const char* message)
{
    verbose("Monobridge", "Bridged call: il2cpp_exception_from_name_msg image=%p name_space=%s name=%s message=%s", image, name_space, name, message);
    return mono_exception_from_name_msg(image, name_space, name, message);
}

void il2cpp_get_exception_argument_null_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_get_exception_argument_null");
    exit(-1);
}

void il2cpp_format_exception_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_format_exception");
    exit(-2);
}

void il2cpp_format_stack_trace_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_format_stack_trace");
    exit(-2);
}

void il2cpp_unhandled_exception_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_unhandled_exception");
    exit(-2);
}

void il2cpp_native_stack_trace_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_native_stack_trace");
    exit(-2);
}

int il2cpp_field_get_flags_impl(void* field)
{
    verbose("Monobridge", "Bridged call: il2cpp_field_get_flags");
    return (int)mono_field_get_flags(field);
}

const char* il2cpp_field_get_name_impl(void* field)
{
    verbose("Monobridge", "Bridged call: il2cpp_field_get_name");
    return mono_field_get_name(field);
}

void* il2cpp_field_get_parent_impl(void* field)
{
    verbose("Monobridge", "Bridged call: il2cpp_field_get_parent");
    return mono_field_get_parent(field);
}

size_t il2cpp_field_get_offset_impl(void* field)
{
    verbose("Monobridge", "Bridged call: il2cpp_field_get_offset");
    return (size_t)mono_field_get_offset(field);
}

void* il2cpp_field_get_type_impl(void* field)
{
    verbose("Monobridge", "Bridged call: il2cpp_field_get_type");
    return mono_field_get_type(field);
}

void il2cpp_field_get_value_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_field_get_value");
    exit(-2);
}

void il2cpp_field_get_value_object_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_field_get_value_object");
    exit(-2);
}

bool il2cpp_field_has_attribute_impl(void* field, void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_field_has_attribute");
    auto parent = mono_field_get_parent(field);
    auto attrs = mono_custom_attrs_from_field(parent, field);

    if (attrs == NULL)
        return false;

    auto found = mono_custom_attrs_has_attr(attrs, klass);
    mono_custom_attrs_free(attrs);
    return found != 0;
}

void il2cpp_field_set_value_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_field_set_value");
    exit(-2);
}

void il2cpp_field_static_get_value_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_field_static_get_value");
    exit(-2);
}

void il2cpp_field_static_set_value_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_field_static_set_value");
    exit(-2);
}

void il2cpp_field_set_value_object_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_field_set_value_object");
    exit(-2);
}

void il2cpp_field_is_literal_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_field_is_literal");
    exit(-1);
}

void il2cpp_gc_collect_impl(int generations)
{
    verbose("Monobridge", "Bridged call: il2cpp_gc_collect");
    return mono_gc_collect(generations);
}

void il2cpp_gc_collect_a_little_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_collect_a_little");
    exit(-1);
}

void il2cpp_gc_start_incremental_collection_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_start_incremental_collection");
    exit(-1);
}

void il2cpp_gc_disable_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_disable");
    exit(-1);
}

void il2cpp_gc_enable_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_gc_enable");
    exit(-2);
}

void il2cpp_gc_is_disabled_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_gc_is_disabled");
    exit(-2);
}

void il2cpp_gc_set_mode_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_set_mode");
    exit(-1);
}

void il2cpp_gc_get_max_time_slice_ns_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_get_max_time_slice_ns");
    exit(-1);
}

void il2cpp_gc_set_max_time_slice_ns_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_set_max_time_slice_ns");
    exit(-1);
}

bool il2cpp_gc_is_incremental_impl()
{
    verbose("Monobridge", "Bridged call: il2cpp_gc_is_incremental");
    return (bool)mono_gc_is_incremental();
}

void il2cpp_gc_get_used_size_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_get_used_size");
    exit(-1);
}

void il2cpp_gc_get_heap_size_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_get_heap_size");
    exit(-1);
}

void il2cpp_gc_wbarrier_set_field_impl(void* param1, void* param2, void* param3)
{
    verbose("Monobridge", "Bridged call: il2cpp_gc_wbarrier_set_field");
    return mono_gc_wbarrier_set_field(param1, param2, param3);
}

bool il2cpp_gc_has_strict_wbarriers_impl()
{
    verbose("Monobridge", "Call: il2cpp_gc_has_strict_wbarriers");
    return 0;
}

void il2cpp_gc_set_external_allocation_tracker_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_set_external_allocation_tracker");
    exit(-1);
}

void il2cpp_gc_set_external_wbarrier_tracker_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_set_external_wbarrier_tracker");
    exit(-1);
}

void il2cpp_gc_foreach_heap_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gc_foreach_heap");
    exit(-1);
}

void il2cpp_stop_gc_world_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_stop_gc_world");
}

void il2cpp_start_gc_world_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_start_gc_world");
}

void il2cpp_gc_alloc_fixed_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_gc_alloc_fixed");
    exit(-2);
}

void il2cpp_gc_free_fixed_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_gc_free_fixed");
    exit(-2);
}

uint32_t il2cpp_gchandle_new_impl(void* obj, bool pinned)
{
    verbose("Monobridge", "Bridged call: il2cpp_gchandle_new");
    return mono_gchandle_new(obj, pinned);
}

void il2cpp_gchandle_new_weakref_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gchandle_new_weakref");
    exit(-1);
}

void il2cpp_gchandle_get_target_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gchandle_get_target");
    exit(-1);
}

void il2cpp_gchandle_free_impl(uint32_t handle)
{
    verbose("Monobridge", "Bridged call: il2cpp_gchandle_free");
    return mono_gchandle_free(handle);
}

void il2cpp_gchandle_foreach_get_target_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_gchandle_foreach_get_target");
    exit(-1);
}

void il2cpp_object_header_size_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_object_header_size");
    exit(-1);
}

void il2cpp_array_object_header_size_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_array_object_header_size");
    exit(-1);
}

void il2cpp_offset_of_array_length_in_array_object_header_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_offset_of_array_length_in_array_object_header");
    exit(-1);
}

void il2cpp_offset_of_array_bounds_in_array_object_header_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_offset_of_array_bounds_in_array_object_header");
    exit(-1);
}

void il2cpp_allocation_granularity_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_allocation_granularity");
    exit(-1);
}

void* il2cpp_unity_liveness_allocate_struct_impl(void* filter, int max_object_count, void* register_callback, void* userdata, void* reallocate_callback)
{
    verbose("Monobridge", "Bridged call: il2cpp_unity_liveness_allocate_struct");
    // IL2CPP API has 5 params (filter, max_count, callback, userdata, reallocate).
    // Mono API has 6 params (filter, max_count, callback, userdata, onWorldStopCB, onWorldStartCB).
    // The 5th param (reallocate) is only used by mono if arrays need to grow during traversal.
    // With adequate max_object_count, growth won't happen. On ARM64, the 6th register (x5)
    // holds a safe residual value since mono only stores the pointer and never calls it
    // unless array growth triggers during traversal.
    return mono_unity_liveness_allocate_struct(filter, max_object_count, register_callback, userdata, reallocate_callback, NULL);
}

void il2cpp_unity_liveness_calculation_from_root_impl(void* root, void* state)
{
    verbose("Monobridge", "Bridged call: il2cpp_unity_liveness_calculation_from_root");
    mono_unity_liveness_calculation_from_root(root, state);
}

void il2cpp_unity_liveness_calculation_from_statics_impl(void* state)
{
    verbose("Monobridge", "Bridged call: il2cpp_unity_liveness_calculation_from_statics");
    mono_unity_liveness_calculation_from_statics(state);
}

void il2cpp_unity_liveness_finalize_impl(void* state)
{
    verbose("Monobridge", "Bridged call: il2cpp_unity_liveness_finalize");
    mono_unity_liveness_finalize(state);
}

void il2cpp_unity_liveness_free_struct_impl(void* state)
{
    verbose("Monobridge", "Bridged call: il2cpp_unity_liveness_free_struct");
    mono_unity_liveness_free_struct(state);
}

void* il2cpp_method_get_return_type_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_get_return_type");
    auto sig = mono_method_signature(method);
    return mono_signature_get_return_type(sig);
}

void il2cpp_method_get_declaring_type_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_method_get_declaring_type");
    exit(-2);
}

const char* il2cpp_method_get_name_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_get_name");
    return mono_method_get_name(method);
}

void il2cpp_method_get_from_reflection_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_method_get_from_reflection");
    exit(-1);
}

void il2cpp_method_get_object_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_method_get_object");
    exit(-1);
}

int32_t il2cpp_method_is_generic_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_is_generic");
    return unity_mono_method_is_generic(method);
}

int32_t il2cpp_method_is_inflated_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_is_inflated");
    return unity_mono_method_is_inflated(method);
}

bool il2cpp_method_is_instance_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_is_instance");
    auto sig = mono_method_signature(method);
    return (bool)mono_signature_is_instance(sig);
}

uint32_t il2cpp_method_get_param_count_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_get_param_count");
    auto sig = mono_method_signature(method);
    return mono_signature_get_param_count(sig);
}

void* il2cpp_method_get_param_impl(void* method, uint32_t index)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_get_param");

    if (index < 0)
        return NULL;

    auto sig = mono_method_signature(method);
    void* iter = NULL;
    void* param = NULL;

    for (int i = 0; i <= index; i++) {
        param = mono_signature_get_params(sig, &iter);
    }

    return param;
}

void* il2cpp_method_get_class_impl(void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_method_get_class");
    return mono_method_get_class(method);
}

void il2cpp_method_has_attribute_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_method_has_attribute");
    exit(-1);
}

void il2cpp_method_get_flags_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_method_get_flags");
    exit(-2);
}

void il2cpp_method_get_token_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_method_get_token");
    exit(-2);
}

void il2cpp_method_get_param_name_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_method_get_param_name");
    exit(-2);
}

void il2cpp_profiler_install_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_install");
}

void il2cpp_profiler_set_events_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_set_events");
}

void il2cpp_profiler_install_enter_leave_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_install_enter_leave");
}

void il2cpp_profiler_install_allocation_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_install_allocation");
}

void il2cpp_profiler_install_gc_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_install_gc");
}

void il2cpp_profiler_install_fileio_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_install_fileio");
}

void il2cpp_profiler_install_thread_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_profiler_install_thread");
}

void il2cpp_property_get_flags_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_property_get_flags");
    exit(-2);
}

void il2cpp_property_get_get_method_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_property_get_get_method");
    exit(-2);
}

void il2cpp_property_get_set_method_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_property_get_set_method");
    exit(-2);
}

void il2cpp_property_get_name_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_property_get_name");
    exit(-2);
}

void il2cpp_property_get_parent_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_property_get_parent");
    exit(-2);
}

void* il2cpp_object_get_class_impl(void* obj)
{
    verbose("Monobridge", "Bridged call: il2cpp_object_get_class");
    return mono_object_get_class(obj);
}

void il2cpp_object_get_size_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_object_get_size");
    exit(-1);
}

void* il2cpp_object_get_virtual_method_impl(void* obj, void* method)
{
    verbose("Monobridge", "Bridged call: il2cpp_object_get_virtual_method");
    return mono_object_get_virtual_method(obj, method);
}

void* il2cpp_object_new_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_object_new");
    return mono_object_new(cached_domain, klass);
}

void il2cpp_object_unbox_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_object_unbox");
    exit(-1);
}

void il2cpp_value_box_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_value_box");
    exit(-1);
}

void il2cpp_monitor_enter_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_enter");
    exit(-2);
}

void il2cpp_monitor_try_enter_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_try_enter");
    exit(-2);
}

void il2cpp_monitor_exit_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_exit");
    exit(-2);
}

void il2cpp_monitor_pulse_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_pulse");
    exit(-2);
}

void il2cpp_monitor_pulse_all_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_pulse_all");
    exit(-2);
}

void il2cpp_monitor_wait_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_wait");
    exit(-2);
}

void il2cpp_monitor_try_wait_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_monitor_try_wait");
    exit(-2);
}

void* il2cpp_runtime_invoke_impl(void* method, void* obj, void* params, void* exc)
{
    verbose("Monobridge", "Bridged call: il2cpp_runtime_invoke");
    return mono_runtime_invoke(method, obj, params, exc);
}

void il2cpp_runtime_invoke_convert_args_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_runtime_invoke_convert_args");
    exit(-1);
}

void il2cpp_runtime_class_init_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_runtime_class_init");
    exit(-2);
}

void il2cpp_runtime_object_init_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_runtime_object_init");
    exit(-1);
}

void* il2cpp_thread_current_impl();
void il2cpp_runtime_object_init_exception_impl(void* obj, void** exc)
{
    verbose("Monobridge", "Bridged call: il2cpp_runtime_object_init_exception");
    *exc = nullptr;
    void* iter = nullptr;
    void* klass = mono_object_get_class(obj);
    if (!klass)
        return;
    verbose("Monobridge", "Class name: %s", mono_class_get_name(klass));
    
    void* currentMethod = NULL;

    void* constructorToInvoke = nullptr;
    while (currentMethod = mono_class_get_methods(klass, &iter)) {
        const char* name = mono_method_get_name(currentMethod);
        int argCount = il2cpp_method_get_param_count_impl(currentMethod);

        verbose("Monobridge", "Method: %s %d", name, argCount);
        if (strcmp(".ctor", name) == 0 && argCount == 0) {
            constructorToInvoke = currentMethod;
            break; // Found the constructor, so we can stop searching.
        }
    }

    if (constructorToInvoke) {
        if (il2cpp_thread_current_impl() == nullptr) {
            verbose("Monobridge", "ERROR: il2cpp_runtime_object_init_exception - constructorToInvoke found but no thread current");
        } else {
            // The actual unity implementation does profiling here, lets skip it for now. TODO
            auto result = mono_runtime_invoke(constructorToInvoke, obj, nullptr, exc);
            mono_gc_wbarrier_set_field(nullptr, &result, result);
        }
    }
}

void il2cpp_runtime_unhandled_exception_policy_set_impl(Il2CppRuntimeUnhandledExceptionPolicy value)
{
    verbose("Monobridge", "Bridged call: il2cpp_runtime_unhandled_exception_policy_set");
    return mono_runtime_unhandled_exception_policy_set(value);
}

int32_t il2cpp_string_length_impl(void* string)
{
    verbose("Monobridge", "Bridged call: il2cpp_string_length");
    return (int32_t)mono_string_length(string);
}

wchar_t* il2cpp_string_chars_impl(void* string)
{
    verbose("Monobridge", "Bridged call: il2cpp_string_chars");
    return (wchar_t*)mono_string_chars(string);
}

void il2cpp_string_new_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_string_new");
    exit(-2);
}

void* il2cpp_string_new_len_impl(const char* str, uint32_t length)
{
    verbose("Monobridge", "Bridged call: il2cpp_string_new_len");
    return mono_string_new_len(cached_domain, str, (unsigned int)length);
}

void il2cpp_string_new_utf16_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_string_new_utf16");
    exit(-1);
}

void* il2cpp_string_new_wrapper_impl(const char* string)
{
    verbose("Monobridge", "Bridged call: il2cpp_string_new_wrapper");
    return mono_string_new_wrapper(string);
}

void il2cpp_string_intern_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_string_intern");
    exit(-2);
}

void il2cpp_string_is_interned_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_string_is_interned");
    exit(-2);
}

void* il2cpp_thread_current_impl()
{
    verbose("Monobridge", "Bridged call: il2cpp_thread_current");

    auto domain = mono_domain_get();
    if (domain == NULL)
        return mono_thread_attach(cached_domain);

    return mono_thread_current();
}

void il2cpp_thread_attach_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_thread_attach");
    exit(-1);
}

void il2cpp_thread_detach_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_thread_detach");
    exit(-1);
}

void il2cpp_thread_get_all_attached_threads_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_thread_get_all_attached_threads");
    exit(-2);
}

void il2cpp_is_vm_thread_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_is_vm_thread");
    exit(-2);
}

void il2cpp_current_thread_walk_frame_stack_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_current_thread_walk_frame_stack");
    exit(-2);
}

void il2cpp_thread_walk_frame_stack_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_thread_walk_frame_stack");
    exit(-2);
}

void il2cpp_current_thread_get_top_frame_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_current_thread_get_top_frame");
    exit(-2);
}

void il2cpp_thread_get_top_frame_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_thread_get_top_frame");
    exit(-2);
}

void il2cpp_current_thread_get_frame_at_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_current_thread_get_frame_at");
    exit(-2);
}

void il2cpp_thread_get_frame_at_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_thread_get_frame_at");
    exit(-2);
}

void il2cpp_current_thread_get_stack_depth_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_current_thread_get_stack_depth");
    exit(-2);
}

void il2cpp_thread_get_stack_depth_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_thread_get_stack_depth");
    exit(-2);
}

void il2cpp_override_stack_backtrace_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_override_stack_backtrace");
}

void il2cpp_type_get_object_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_get_object");
    exit(-1);
}

int il2cpp_type_get_type_impl(void* type)
{
    verbose("Monobridge", "Bridged call: il2cpp_type_get_type");
    return mono_type_get_type(type);
}

void* il2cpp_type_get_class_or_element_class_impl(void* type)
{
    verbose("Monobridge", "Bridged call: il2cpp_type_get_class_or_element_class");
    auto klass = mono_class_from_mono_type(type);
    auto rank = mono_class_get_rank(klass);
    if (rank >= 0)
        return mono_class_get_element_class(klass);

    return klass;
}

void il2cpp_type_get_name_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_get_name");
    exit(-1);
}

bool il2cpp_type_is_byref_impl(void* type)
{
    verbose("Monobridge", "Bridged call: il2cpp_type_is_byref");
    return (bool)mono_type_is_byref(type);
}

void il2cpp_type_get_attrs_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_get_attrs");
    exit(-1);
}

void il2cpp_type_equals_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_equals");
    exit(-1);
}

void il2cpp_type_get_assembly_qualified_name_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_get_assembly_qualified_name");
    exit(-1);
}

void il2cpp_type_is_static_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_is_static");
    exit(-1);
}

void il2cpp_type_is_pointer_type_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_type_is_pointer_type");
    exit(-1);
}

void il2cpp_image_get_assembly_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_image_get_assembly");
    exit(-2);
}

void il2cpp_image_get_name_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_image_get_name");
    exit(-1);
}

void il2cpp_image_get_filename_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_image_get_filename");
    exit(-2);
}

void il2cpp_image_get_entry_point_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_image_get_entry_point");
    exit(-2);
}

size_t il2cpp_image_get_class_count_impl(void* image)
{
    verbose("Monobridge", "Bridged call: il2cpp_image_get_class_count");
    return (size_t)mono_image_get_table_rows(image, 2); // 2 is MONO_TABLE_TYPEDEF
}

void il2cpp_image_get_class_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_image_get_class");
    exit(-1);
}

void il2cpp_capture_memory_snapshot_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_capture_memory_snapshot");
    exit(-2);
}

void il2cpp_free_captured_memory_snapshot_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_free_captured_memory_snapshot");
    exit(-2);
}

void il2cpp_set_find_plugin_callback_impl(void* callback)
{
    verbose("Monobridge", "Bridged call: il2cpp_set_find_plugin_callback");
    return mono_set_find_plugin_callback(callback);
}

void il2cpp_register_log_callback_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_register_log_callback");
}

void il2cpp_debugger_set_agent_options_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_debugger_set_agent_options");
}

void il2cpp_is_debugger_attached_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_is_debugger_attached");
    exit(-2);
}

void il2cpp_register_debugger_agent_transport_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_register_debugger_agent_transport");
    exit(-2);
}

void il2cpp_debug_get_method_info_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_debug_get_method_info");
    exit(-1);
}

void il2cpp_unity_install_unitytls_interface_impl(void* interface)
{
    verbose("Monobridge", "Bridged call: il2cpp_unity_install_unitytls_interface");
    return mono_unity_install_unitytls_interface(interface);
}

void* il2cpp_custom_attrs_from_class_impl(void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_custom_attrs_from_class");
    return mono_custom_attrs_from_class(klass);
}

void il2cpp_custom_attrs_from_method_impl()
{
    verbose("Monobridge", "Unimplemented call: il2cpp_custom_attrs_from_method");
    exit(-1);
}

void* il2cpp_custom_attrs_get_attr_impl(void* ainfo, void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_custom_attrs_get_attr");
    return mono_custom_attrs_get_attr(ainfo, klass);
}

bool il2cpp_custom_attrs_has_attr_impl(void* ainfo, void* klass)
{
    verbose("Monobridge", "Bridged call: il2cpp_custom_attrs_has_attr");
    return (bool)mono_custom_attrs_has_attr(ainfo, klass);
}

void* il2cpp_custom_attrs_construct_impl(void* ainfo)
{
    verbose("Monobridge", "Bridged call: il2cpp_custom_attrs_construct");
    return mono_custom_attrs_construct(ainfo);
}

void il2cpp_custom_attrs_free_impl(void* ainfo)
{
    verbose("Monobridge", "Bridged call: il2cpp_custom_attrs_free");
    return mono_custom_attrs_free(ainfo);
}

void il2cpp_class_set_userdata_impl(void* klass, void* userdata)
{
    verbose("Monobridge", "Bridged call: il2cpp_class_set_userdata");
    return mono_class_set_userdata(klass, userdata);
}

int il2cpp_class_get_userdata_offset_impl()
{
    verbose("Monobridge", "Bridged call: il2cpp_class_get_userdata_offset");
    return mono_class_get_userdata_offset();
}

void il2cpp_set_default_thread_affinity_impl()
{
    verbose("Monobridge", "SHOULD BE UNUSED!!! Unimplemented call: il2cpp_set_default_thread_affinity");
    exit(-2);
}

void il2cpp_unity_set_android_network_up_state_func_impl()
{
    verbose("Monobridge", "Stubbed call: il2cpp_unity_set_android_network_up_state_func");
}


// NOT SURE WHAT THESE ARE YET

void il2cpp_class_is_inited_impl()
{
    printf("Monobridge Unimplemented call: il2cpp_class_is_inited");
    exit(-1);
}

DynLibFunction symtable_monobridge[] = {
    NO_THUNK("il2cpp_class_is_inited", (uintptr_t)&il2cpp_class_is_inited_impl),
    NO_THUNK("il2cpp_init", (uintptr_t)&il2cpp_init_impl),
    NO_THUNK("il2cpp_init_utf16", (uintptr_t)&il2cpp_init_utf16_impl),
    NO_THUNK("il2cpp_shutdown", (uintptr_t)&il2cpp_shutdown_impl),
    NO_THUNK("il2cpp_set_config_dir", (uintptr_t)&il2cpp_set_config_dir_impl),
    NO_THUNK("il2cpp_set_data_dir", (uintptr_t)&il2cpp_set_data_dir_impl),
    NO_THUNK("il2cpp_set_temp_dir", (uintptr_t)&il2cpp_set_temp_dir_impl),
    NO_THUNK("il2cpp_set_commandline_arguments", (uintptr_t)&il2cpp_set_commandline_arguments_impl),
    NO_THUNK("il2cpp_set_commandline_arguments_utf16", (uintptr_t)&il2cpp_set_commandline_arguments_utf16_impl),
    NO_THUNK("il2cpp_set_config_utf16", (uintptr_t)&il2cpp_set_config_utf16_impl),
    NO_THUNK("il2cpp_set_config", (uintptr_t)&il2cpp_set_config_impl),
    NO_THUNK("il2cpp_set_memory_callbacks", (uintptr_t)&il2cpp_set_memory_callbacks_impl),
    NO_THUNK("il2cpp_get_corlib", (uintptr_t)&il2cpp_get_corlib_impl),
    NO_THUNK("il2cpp_add_internal_call", (uintptr_t)&il2cpp_add_internal_call_impl),
    NO_THUNK("il2cpp_resolve_icall", (uintptr_t)&il2cpp_resolve_icall_impl),
    NO_THUNK("il2cpp_alloc", (uintptr_t)&il2cpp_alloc_impl),
    NO_THUNK("il2cpp_free", (uintptr_t)&il2cpp_free_impl),
    NO_THUNK("il2cpp_array_class_get", (uintptr_t)&il2cpp_array_class_get_impl),
    NO_THUNK("il2cpp_array_length", (uintptr_t)&il2cpp_array_length_impl),
    NO_THUNK("il2cpp_array_get_byte_length", (uintptr_t)&il2cpp_array_get_byte_length_impl),
    NO_THUNK("il2cpp_array_new", (uintptr_t)&il2cpp_array_new_impl),
    NO_THUNK("il2cpp_array_new_specific", (uintptr_t)&il2cpp_array_new_specific_impl),
    NO_THUNK("il2cpp_array_new_full", (uintptr_t)&il2cpp_array_new_full_impl),
    NO_THUNK("il2cpp_bounded_array_class_get", (uintptr_t)&il2cpp_bounded_array_class_get_impl),
    NO_THUNK("il2cpp_array_element_size", (uintptr_t)&il2cpp_array_element_size_impl),
    NO_THUNK("il2cpp_assembly_get_image", (uintptr_t)&il2cpp_assembly_get_image_impl),
    NO_THUNK("il2cpp_class_for_each", (uintptr_t)&il2cpp_class_for_each_impl),
    NO_THUNK("il2cpp_class_enum_basetype", (uintptr_t)&il2cpp_class_enum_basetype_impl),
    NO_THUNK("il2cpp_class_is_generic", (uintptr_t)&il2cpp_class_is_generic_impl),
    NO_THUNK("il2cpp_class_is_inflated", (uintptr_t)&il2cpp_class_is_inflated_impl),
    NO_THUNK("il2cpp_class_is_assignable_from", (uintptr_t)&il2cpp_class_is_assignable_from_impl),
    NO_THUNK("il2cpp_class_is_subclass_of", (uintptr_t)&il2cpp_class_is_subclass_of_impl),
    NO_THUNK("il2cpp_class_has_parent", (uintptr_t)&il2cpp_class_has_parent_impl),
    NO_THUNK("il2cpp_class_from_il2cpp_type", (uintptr_t)&il2cpp_class_from_il2cpp_type_impl),
    NO_THUNK("il2cpp_class_from_name", (uintptr_t)&il2cpp_class_from_name_impl),
    NO_THUNK("il2cpp_class_from_system_type", (uintptr_t)&il2cpp_class_from_system_type_impl),
    NO_THUNK("il2cpp_class_get_element_class", (uintptr_t)&il2cpp_class_get_element_class_impl),
    NO_THUNK("il2cpp_class_get_events", (uintptr_t)&il2cpp_class_get_events_impl),
    NO_THUNK("il2cpp_class_get_fields", (uintptr_t)&il2cpp_class_get_fields_impl),
    NO_THUNK("il2cpp_class_get_nested_types", (uintptr_t)&il2cpp_class_get_nested_types_impl),
    NO_THUNK("il2cpp_class_get_interfaces", (uintptr_t)&il2cpp_class_get_interfaces_impl),
    NO_THUNK("il2cpp_class_get_properties", (uintptr_t)&il2cpp_class_get_properties_impl),
    NO_THUNK("il2cpp_class_get_property_from_name", (uintptr_t)&il2cpp_class_get_property_from_name_impl),
    NO_THUNK("il2cpp_class_get_field_from_name", (uintptr_t)&il2cpp_class_get_field_from_name_impl),
    NO_THUNK("il2cpp_class_get_methods", (uintptr_t)&il2cpp_class_get_methods_impl),
    NO_THUNK("il2cpp_class_get_method_from_name", (uintptr_t)&il2cpp_class_get_method_from_name_impl),
    NO_THUNK("il2cpp_class_get_name", (uintptr_t)&il2cpp_class_get_name_impl),
    NO_THUNK("il2cpp_type_get_name_chunked", (uintptr_t)&il2cpp_type_get_name_chunked_impl),
    NO_THUNK("il2cpp_class_get_namespace", (uintptr_t)&il2cpp_class_get_namespace_impl),
    NO_THUNK("il2cpp_class_get_parent", (uintptr_t)&il2cpp_class_get_parent_impl),
    NO_THUNK("il2cpp_class_get_declaring_type", (uintptr_t)&il2cpp_class_get_declaring_type_impl),
    NO_THUNK("il2cpp_class_instance_size", (uintptr_t)&il2cpp_class_instance_size_impl),
    NO_THUNK("il2cpp_class_num_fields", (uintptr_t)&il2cpp_class_num_fields_impl),
    NO_THUNK("il2cpp_class_is_valuetype", (uintptr_t)&il2cpp_class_is_valuetype_impl),
    NO_THUNK("il2cpp_class_value_size", (uintptr_t)&il2cpp_class_value_size_impl),
    NO_THUNK("il2cpp_class_is_blittable", (uintptr_t)&il2cpp_class_is_blittable_impl),
    NO_THUNK("il2cpp_class_get_flags", (uintptr_t)&il2cpp_class_get_flags_impl),
    NO_THUNK("il2cpp_class_is_abstract", (uintptr_t)&il2cpp_class_is_abstract_impl),
    NO_THUNK("il2cpp_class_is_interface", (uintptr_t)&il2cpp_class_is_interface_impl),
    NO_THUNK("il2cpp_class_array_element_size", (uintptr_t)&il2cpp_class_array_element_size_impl),
    NO_THUNK("il2cpp_class_from_type", (uintptr_t)&il2cpp_class_from_type_impl),
    NO_THUNK("il2cpp_class_get_type", (uintptr_t)&il2cpp_class_get_type_impl),
    NO_THUNK("il2cpp_class_get_type_token", (uintptr_t)&il2cpp_class_get_type_token_impl),
    NO_THUNK("il2cpp_class_has_attribute", (uintptr_t)&il2cpp_class_has_attribute_impl),
    NO_THUNK("il2cpp_class_has_references", (uintptr_t)&il2cpp_class_has_references_impl),
    NO_THUNK("il2cpp_class_is_enum", (uintptr_t)&il2cpp_class_is_enum_impl),
    NO_THUNK("il2cpp_class_get_image", (uintptr_t)&il2cpp_class_get_image_impl),
    NO_THUNK("il2cpp_class_get_assemblyname", (uintptr_t)&il2cpp_class_get_assemblyname_impl),
    NO_THUNK("il2cpp_class_get_rank", (uintptr_t)&il2cpp_class_get_rank_impl),
    NO_THUNK("il2cpp_class_get_data_size", (uintptr_t)&il2cpp_class_get_data_size_impl),
    NO_THUNK("il2cpp_class_get_static_field_data", (uintptr_t)&il2cpp_class_get_static_field_data_impl),
    NO_THUNK("il2cpp_class_get_bitmap_size", (uintptr_t)&il2cpp_class_get_bitmap_size_impl),
    NO_THUNK("il2cpp_class_get_bitmap", (uintptr_t)&il2cpp_class_get_bitmap_impl),
    NO_THUNK("il2cpp_stats_dump_to_file", (uintptr_t)&il2cpp_stats_dump_to_file_impl),
    NO_THUNK("il2cpp_stats_get_value", (uintptr_t)&il2cpp_stats_get_value_impl),
    NO_THUNK("il2cpp_domain_get", (uintptr_t)&il2cpp_domain_get_impl),
    NO_THUNK("il2cpp_domain_assembly_open", (uintptr_t)&il2cpp_domain_assembly_open_impl),
    NO_THUNK("il2cpp_domain_get_assemblies", (uintptr_t)&il2cpp_domain_get_assemblies_impl),
    NO_THUNK("il2cpp_raise_exception", (uintptr_t)&il2cpp_raise_exception_impl),
    NO_THUNK("il2cpp_exception_from_name_msg", (uintptr_t)&il2cpp_exception_from_name_msg_impl),
    NO_THUNK("il2cpp_get_exception_argument_null", (uintptr_t)&il2cpp_get_exception_argument_null_impl),
    NO_THUNK("il2cpp_format_exception", (uintptr_t)&il2cpp_format_exception_impl),
    NO_THUNK("il2cpp_format_stack_trace", (uintptr_t)&il2cpp_format_stack_trace_impl),
    NO_THUNK("il2cpp_unhandled_exception", (uintptr_t)&il2cpp_unhandled_exception_impl),
    NO_THUNK("il2cpp_native_stack_trace", (uintptr_t)&il2cpp_native_stack_trace_impl),
    NO_THUNK("il2cpp_field_get_flags", (uintptr_t)&il2cpp_field_get_flags_impl),
    NO_THUNK("il2cpp_field_get_name", (uintptr_t)&il2cpp_field_get_name_impl),
    NO_THUNK("il2cpp_field_get_parent", (uintptr_t)&il2cpp_field_get_parent_impl),
    NO_THUNK("il2cpp_field_get_offset", (uintptr_t)&il2cpp_field_get_offset_impl),
    NO_THUNK("il2cpp_field_get_type", (uintptr_t)&il2cpp_field_get_type_impl),
    NO_THUNK("il2cpp_field_get_value", (uintptr_t)&il2cpp_field_get_value_impl),
    NO_THUNK("il2cpp_field_get_value_object", (uintptr_t)&il2cpp_field_get_value_object_impl),
    NO_THUNK("il2cpp_field_has_attribute", (uintptr_t)&il2cpp_field_has_attribute_impl),
    NO_THUNK("il2cpp_field_set_value", (uintptr_t)&il2cpp_field_set_value_impl),
    NO_THUNK("il2cpp_field_static_get_value", (uintptr_t)&il2cpp_field_static_get_value_impl),
    NO_THUNK("il2cpp_field_static_set_value", (uintptr_t)&il2cpp_field_static_set_value_impl),
    NO_THUNK("il2cpp_field_set_value_object", (uintptr_t)&il2cpp_field_set_value_object_impl),
    NO_THUNK("il2cpp_field_is_literal", (uintptr_t)&il2cpp_field_is_literal_impl),
    NO_THUNK("il2cpp_gc_collect", (uintptr_t)&il2cpp_gc_collect_impl),
    NO_THUNK("il2cpp_gc_collect_a_little", (uintptr_t)&il2cpp_gc_collect_a_little_impl),
    NO_THUNK("il2cpp_gc_start_incremental_collection", (uintptr_t)&il2cpp_gc_start_incremental_collection_impl),
    NO_THUNK("il2cpp_gc_disable", (uintptr_t)&il2cpp_gc_disable_impl),
    NO_THUNK("il2cpp_gc_enable", (uintptr_t)&il2cpp_gc_enable_impl),
    NO_THUNK("il2cpp_gc_is_disabled", (uintptr_t)&il2cpp_gc_is_disabled_impl),
    NO_THUNK("il2cpp_gc_set_mode", (uintptr_t)&il2cpp_gc_set_mode_impl),
    NO_THUNK("il2cpp_gc_get_max_time_slice_ns", (uintptr_t)&il2cpp_gc_get_max_time_slice_ns_impl),
    NO_THUNK("il2cpp_gc_set_max_time_slice_ns", (uintptr_t)&il2cpp_gc_set_max_time_slice_ns_impl),
    NO_THUNK("il2cpp_gc_is_incremental", (uintptr_t)&il2cpp_gc_is_incremental_impl),
    NO_THUNK("il2cpp_gc_get_used_size", (uintptr_t)&il2cpp_gc_get_used_size_impl),
    NO_THUNK("il2cpp_gc_get_heap_size", (uintptr_t)&il2cpp_gc_get_heap_size_impl),
    NO_THUNK("il2cpp_gc_wbarrier_set_field", (uintptr_t)&il2cpp_gc_wbarrier_set_field_impl),
    NO_THUNK("il2cpp_gc_has_strict_wbarriers", (uintptr_t)&il2cpp_gc_has_strict_wbarriers_impl),
    NO_THUNK("il2cpp_gc_set_external_allocation_tracker", (uintptr_t)&il2cpp_gc_set_external_allocation_tracker_impl),
    NO_THUNK("il2cpp_gc_set_external_wbarrier_tracker", (uintptr_t)&il2cpp_gc_set_external_wbarrier_tracker_impl),
    NO_THUNK("il2cpp_gc_foreach_heap", (uintptr_t)&il2cpp_gc_foreach_heap_impl),
    NO_THUNK("il2cpp_stop_gc_world", (uintptr_t)&il2cpp_stop_gc_world_impl),
    NO_THUNK("il2cpp_start_gc_world", (uintptr_t)&il2cpp_start_gc_world_impl),
    NO_THUNK("il2cpp_gc_alloc_fixed", (uintptr_t)&il2cpp_gc_alloc_fixed_impl),
    NO_THUNK("il2cpp_gc_free_fixed", (uintptr_t)&il2cpp_gc_free_fixed_impl),
    NO_THUNK("il2cpp_gchandle_new", (uintptr_t)&il2cpp_gchandle_new_impl),
    NO_THUNK("il2cpp_gchandle_new_weakref", (uintptr_t)&il2cpp_gchandle_new_weakref_impl),
    NO_THUNK("il2cpp_gchandle_get_target", (uintptr_t)&il2cpp_gchandle_get_target_impl),
    NO_THUNK("il2cpp_gchandle_free", (uintptr_t)&il2cpp_gchandle_free_impl),
    NO_THUNK("il2cpp_gchandle_foreach_get_target", (uintptr_t)&il2cpp_gchandle_foreach_get_target_impl),
    NO_THUNK("il2cpp_object_header_size", (uintptr_t)&il2cpp_object_header_size_impl),
    NO_THUNK("il2cpp_array_object_header_size", (uintptr_t)&il2cpp_array_object_header_size_impl),
    NO_THUNK("il2cpp_offset_of_array_length_in_array_object_header", (uintptr_t)&il2cpp_offset_of_array_length_in_array_object_header_impl),
    NO_THUNK("il2cpp_offset_of_array_bounds_in_array_object_header", (uintptr_t)&il2cpp_offset_of_array_bounds_in_array_object_header_impl),
    NO_THUNK("il2cpp_allocation_granularity", (uintptr_t)&il2cpp_allocation_granularity_impl),
    NO_THUNK("il2cpp_unity_liveness_allocate_struct", (uintptr_t)&il2cpp_unity_liveness_allocate_struct_impl),
    NO_THUNK("il2cpp_unity_liveness_calculation_from_root", (uintptr_t)&il2cpp_unity_liveness_calculation_from_root_impl),
    NO_THUNK("il2cpp_unity_liveness_calculation_from_statics", (uintptr_t)&il2cpp_unity_liveness_calculation_from_statics_impl),
    NO_THUNK("il2cpp_unity_liveness_finalize", (uintptr_t)&il2cpp_unity_liveness_finalize_impl),
    NO_THUNK("il2cpp_unity_liveness_free_struct", (uintptr_t)&il2cpp_unity_liveness_free_struct_impl),
    NO_THUNK("il2cpp_method_get_return_type", (uintptr_t)&il2cpp_method_get_return_type_impl),
    NO_THUNK("il2cpp_method_get_declaring_type", (uintptr_t)&il2cpp_method_get_declaring_type_impl),
    NO_THUNK("il2cpp_method_get_name", (uintptr_t)&il2cpp_method_get_name_impl),
    NO_THUNK("il2cpp_method_get_from_reflection", (uintptr_t)&il2cpp_method_get_from_reflection_impl),
    NO_THUNK("il2cpp_method_get_object", (uintptr_t)&il2cpp_method_get_object_impl),
    NO_THUNK("il2cpp_method_is_generic", (uintptr_t)&il2cpp_method_is_generic_impl),
    NO_THUNK("il2cpp_method_is_inflated", (uintptr_t)&il2cpp_method_is_inflated_impl),
    NO_THUNK("il2cpp_method_is_instance", (uintptr_t)&il2cpp_method_is_instance_impl),
    NO_THUNK("il2cpp_method_get_param_count", (uintptr_t)&il2cpp_method_get_param_count_impl),
    NO_THUNK("il2cpp_method_get_param", (uintptr_t)&il2cpp_method_get_param_impl),
    NO_THUNK("il2cpp_method_get_class", (uintptr_t)&il2cpp_method_get_class_impl),
    NO_THUNK("il2cpp_method_has_attribute", (uintptr_t)&il2cpp_method_has_attribute_impl),
    NO_THUNK("il2cpp_method_get_flags", (uintptr_t)&il2cpp_method_get_flags_impl),
    NO_THUNK("il2cpp_method_get_token", (uintptr_t)&il2cpp_method_get_token_impl),
    NO_THUNK("il2cpp_method_get_param_name", (uintptr_t)&il2cpp_method_get_param_name_impl),
    NO_THUNK("il2cpp_profiler_install", (uintptr_t)&il2cpp_profiler_install_impl),
    NO_THUNK("il2cpp_profiler_set_events", (uintptr_t)&il2cpp_profiler_set_events_impl),
    NO_THUNK("il2cpp_profiler_install_enter_leave", (uintptr_t)&il2cpp_profiler_install_enter_leave_impl),
    NO_THUNK("il2cpp_profiler_install_allocation", (uintptr_t)&il2cpp_profiler_install_allocation_impl),
    NO_THUNK("il2cpp_profiler_install_gc", (uintptr_t)&il2cpp_profiler_install_gc_impl),
    NO_THUNK("il2cpp_profiler_install_fileio", (uintptr_t)&il2cpp_profiler_install_fileio_impl),
    NO_THUNK("il2cpp_profiler_install_thread", (uintptr_t)&il2cpp_profiler_install_thread_impl),
    NO_THUNK("il2cpp_property_get_flags", (uintptr_t)&il2cpp_property_get_flags_impl),
    NO_THUNK("il2cpp_property_get_get_method", (uintptr_t)&il2cpp_property_get_get_method_impl),
    NO_THUNK("il2cpp_property_get_set_method", (uintptr_t)&il2cpp_property_get_set_method_impl),
    NO_THUNK("il2cpp_property_get_name", (uintptr_t)&il2cpp_property_get_name_impl),
    NO_THUNK("il2cpp_property_get_parent", (uintptr_t)&il2cpp_property_get_parent_impl),
    NO_THUNK("il2cpp_object_get_class", (uintptr_t)&il2cpp_object_get_class_impl),
    NO_THUNK("il2cpp_object_get_size", (uintptr_t)&il2cpp_object_get_size_impl),
    NO_THUNK("il2cpp_object_get_virtual_method", (uintptr_t)&il2cpp_object_get_virtual_method_impl),
    NO_THUNK("il2cpp_object_new", (uintptr_t)&il2cpp_object_new_impl),
    NO_THUNK("il2cpp_object_unbox", (uintptr_t)&il2cpp_object_unbox_impl),
    NO_THUNK("il2cpp_value_box", (uintptr_t)&il2cpp_value_box_impl),
    NO_THUNK("il2cpp_monitor_enter", (uintptr_t)&il2cpp_monitor_enter_impl),
    NO_THUNK("il2cpp_monitor_try_enter", (uintptr_t)&il2cpp_monitor_try_enter_impl),
    NO_THUNK("il2cpp_monitor_exit", (uintptr_t)&il2cpp_monitor_exit_impl),
    NO_THUNK("il2cpp_monitor_pulse", (uintptr_t)&il2cpp_monitor_pulse_impl),
    NO_THUNK("il2cpp_monitor_pulse_all", (uintptr_t)&il2cpp_monitor_pulse_all_impl),
    NO_THUNK("il2cpp_monitor_wait", (uintptr_t)&il2cpp_monitor_wait_impl),
    NO_THUNK("il2cpp_monitor_try_wait", (uintptr_t)&il2cpp_monitor_try_wait_impl),
    NO_THUNK("il2cpp_runtime_invoke", (uintptr_t)&il2cpp_runtime_invoke_impl),
    NO_THUNK("il2cpp_runtime_invoke_convert_args", (uintptr_t)&il2cpp_runtime_invoke_convert_args_impl),
    NO_THUNK("il2cpp_runtime_class_init", (uintptr_t)&il2cpp_runtime_class_init_impl),
    NO_THUNK("il2cpp_runtime_object_init", (uintptr_t)&il2cpp_runtime_object_init_impl),
    NO_THUNK("il2cpp_runtime_object_init_exception", (uintptr_t)&il2cpp_runtime_object_init_exception_impl),
    NO_THUNK("il2cpp_runtime_unhandled_exception_policy_set", (uintptr_t)&il2cpp_runtime_unhandled_exception_policy_set_impl),
    NO_THUNK("il2cpp_string_length", (uintptr_t)&il2cpp_string_length_impl),
    NO_THUNK("il2cpp_string_chars", (uintptr_t)&il2cpp_string_chars_impl),
    NO_THUNK("il2cpp_string_new", (uintptr_t)&il2cpp_string_new_impl),
    NO_THUNK("il2cpp_string_new_len", (uintptr_t)&il2cpp_string_new_len_impl),
    NO_THUNK("il2cpp_string_new_utf16", (uintptr_t)&il2cpp_string_new_utf16_impl),
    NO_THUNK("il2cpp_string_new_wrapper", (uintptr_t)&il2cpp_string_new_wrapper_impl),
    NO_THUNK("il2cpp_string_intern", (uintptr_t)&il2cpp_string_intern_impl),
    NO_THUNK("il2cpp_string_is_interned", (uintptr_t)&il2cpp_string_is_interned_impl),
    NO_THUNK("il2cpp_thread_current", (uintptr_t)&il2cpp_thread_current_impl),
    NO_THUNK("il2cpp_thread_attach", (uintptr_t)&il2cpp_thread_attach_impl),
    NO_THUNK("il2cpp_thread_detach", (uintptr_t)&il2cpp_thread_detach_impl),
    NO_THUNK("il2cpp_thread_get_all_attached_threads", (uintptr_t)&il2cpp_thread_get_all_attached_threads_impl),
    NO_THUNK("il2cpp_is_vm_thread", (uintptr_t)&il2cpp_is_vm_thread_impl),
    NO_THUNK("il2cpp_current_thread_walk_frame_stack", (uintptr_t)&il2cpp_current_thread_walk_frame_stack_impl),
    NO_THUNK("il2cpp_thread_walk_frame_stack", (uintptr_t)&il2cpp_thread_walk_frame_stack_impl),
    NO_THUNK("il2cpp_current_thread_get_top_frame", (uintptr_t)&il2cpp_current_thread_get_top_frame_impl),
    NO_THUNK("il2cpp_thread_get_top_frame", (uintptr_t)&il2cpp_thread_get_top_frame_impl),
    NO_THUNK("il2cpp_current_thread_get_frame_at", (uintptr_t)&il2cpp_current_thread_get_frame_at_impl),
    NO_THUNK("il2cpp_thread_get_frame_at", (uintptr_t)&il2cpp_thread_get_frame_at_impl),
    NO_THUNK("il2cpp_current_thread_get_stack_depth", (uintptr_t)&il2cpp_current_thread_get_stack_depth_impl),
    NO_THUNK("il2cpp_thread_get_stack_depth", (uintptr_t)&il2cpp_thread_get_stack_depth_impl),
    NO_THUNK("il2cpp_override_stack_backtrace", (uintptr_t)&il2cpp_override_stack_backtrace_impl),
    NO_THUNK("il2cpp_type_get_object", (uintptr_t)&il2cpp_type_get_object_impl),
    NO_THUNK("il2cpp_type_get_type", (uintptr_t)&il2cpp_type_get_type_impl),
    NO_THUNK("il2cpp_type_get_class_or_element_class", (uintptr_t)&il2cpp_type_get_class_or_element_class_impl),
    NO_THUNK("il2cpp_type_get_name", (uintptr_t)&il2cpp_type_get_name_impl),
    NO_THUNK("il2cpp_type_is_byref", (uintptr_t)&il2cpp_type_is_byref_impl),
    NO_THUNK("il2cpp_type_get_attrs", (uintptr_t)&il2cpp_type_get_attrs_impl),
    NO_THUNK("il2cpp_type_equals", (uintptr_t)&il2cpp_type_equals_impl),
    NO_THUNK("il2cpp_type_get_assembly_qualified_name", (uintptr_t)&il2cpp_type_get_assembly_qualified_name_impl),
    NO_THUNK("il2cpp_type_is_static", (uintptr_t)&il2cpp_type_is_static_impl),
    NO_THUNK("il2cpp_type_is_pointer_type", (uintptr_t)&il2cpp_type_is_pointer_type_impl),
    NO_THUNK("il2cpp_image_get_assembly", (uintptr_t)&il2cpp_image_get_assembly_impl),
    NO_THUNK("il2cpp_image_get_name", (uintptr_t)&il2cpp_image_get_name_impl),
    NO_THUNK("il2cpp_image_get_filename", (uintptr_t)&il2cpp_image_get_filename_impl),
    NO_THUNK("il2cpp_image_get_entry_point", (uintptr_t)&il2cpp_image_get_entry_point_impl),
    NO_THUNK("il2cpp_image_get_class_count", (uintptr_t)&il2cpp_image_get_class_count_impl),
    NO_THUNK("il2cpp_image_get_class", (uintptr_t)&il2cpp_image_get_class_impl),
    NO_THUNK("il2cpp_capture_memory_snapshot", (uintptr_t)&il2cpp_capture_memory_snapshot_impl),
    NO_THUNK("il2cpp_free_captured_memory_snapshot", (uintptr_t)&il2cpp_free_captured_memory_snapshot_impl),
    NO_THUNK("il2cpp_set_find_plugin_callback", (uintptr_t)&il2cpp_set_find_plugin_callback_impl),
    NO_THUNK("il2cpp_register_log_callback", (uintptr_t)&il2cpp_register_log_callback_impl),
    NO_THUNK("il2cpp_debugger_set_agent_options", (uintptr_t)&il2cpp_debugger_set_agent_options_impl),
    NO_THUNK("il2cpp_is_debugger_attached", (uintptr_t)&il2cpp_is_debugger_attached_impl),
    NO_THUNK("il2cpp_register_debugger_agent_transport", (uintptr_t)&il2cpp_register_debugger_agent_transport_impl),
    NO_THUNK("il2cpp_debug_get_method_info", (uintptr_t)&il2cpp_debug_get_method_info_impl),
    NO_THUNK("il2cpp_unity_install_unitytls_interface", (uintptr_t)&il2cpp_unity_install_unitytls_interface_impl),
    NO_THUNK("il2cpp_custom_attrs_from_class", (uintptr_t)&il2cpp_custom_attrs_from_class_impl),
    NO_THUNK("il2cpp_custom_attrs_from_method", (uintptr_t)&il2cpp_custom_attrs_from_method_impl),
    NO_THUNK("il2cpp_custom_attrs_get_attr", (uintptr_t)&il2cpp_custom_attrs_get_attr_impl),
    NO_THUNK("il2cpp_custom_attrs_has_attr", (uintptr_t)&il2cpp_custom_attrs_has_attr_impl),
    NO_THUNK("il2cpp_custom_attrs_construct", (uintptr_t)&il2cpp_custom_attrs_construct_impl),
    NO_THUNK("il2cpp_custom_attrs_free", (uintptr_t)&il2cpp_custom_attrs_free_impl),
    NO_THUNK("il2cpp_class_set_userdata", (uintptr_t)&il2cpp_class_set_userdata_impl),
    NO_THUNK("il2cpp_class_get_userdata_offset", (uintptr_t)&il2cpp_class_get_userdata_offset_impl),
    NO_THUNK("il2cpp_set_default_thread_affinity", (uintptr_t)&il2cpp_set_default_thread_affinity_impl),
    NO_THUNK("il2cpp_unity_set_android_network_up_state_func", (uintptr_t)&il2cpp_unity_set_android_network_up_state_func_impl),
    { NULL, (uintptr_t)NULL }
};