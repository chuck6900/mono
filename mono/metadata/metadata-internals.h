
#ifndef __MONO_METADATA_INTERNALS_H__
#define __MONO_METADATA_INTERNALS_H__

#include "mono/metadata/image.h"
#include "mono/metadata/blob.h"
#include "mono/metadata/mempool.h"
#include "mono/metadata/domain-internals.h"
#include "mono/utils/mono-hash.h"
#include "mono/utils/mono-compiler.h"
#include "mono/utils/mono-dl.h"
#include "mono/utils/monobitset.h"
#include "mono/utils/mono-property-hash.h"
#include "mono/utils/mono-value-hash.h"
#include "mono/utils/mono-error.h"

/* Big-endian bitfield accessors. Define mono_use_be_bitfield_accessors once
 * in a .c/.cpp file. When FALSE, the legacy bitfield members remain usable. */
#ifndef MONO_BE_BITFIELD_ACCESSORS_DEFINED
#define MONO_BE_BITFIELD_ACCESSORS_DEFINED
extern gboolean mono_use_be_bitfield_accessors;

static inline guint32
mono_be_bitfield_get (const guint8 *data, guint32 size, guint32 shift, guint32 width)
{
    guint32 value = 0;
    guint32 i;
    for (i = 0; i < size; ++i)
        value = (value << 8) | data[i];
    if (width == 32)
        return value;
    return (value >> shift) & ((((guint32)1) << width) - 1);
}

static inline void
mono_be_bitfield_set (guint8 *data, guint32 size, guint32 shift, guint32 width, guint32 field_value)
{
    guint32 value = 0;
    guint32 i;
    guint32 mask = width == 32 ? 0xffffffffu : ((((guint32)1) << width) - 1);
    for (i = 0; i < size; ++i)
        value = (value << 8) | data[i];
    value = (value & ~(mask << shift)) | ((field_value & mask) << shift);
    for (i = size; i > 0; --i) {
        data[i - 1] = (guint8)(value & 0xff);
        value >>= 8;
    }
}

#define MONO_DEFINE_BE_BITFIELD_ACCESSORS(struct_type, prefix, storage_member, field, value_type, shift, width) \
    static inline value_type prefix##_get_##field (const struct_type *instance) \
    { \
        return mono_use_be_bitfield_accessors \
            ? (value_type)mono_be_bitfield_get (instance->storage_member, sizeof (instance->storage_member), shift, width) \
            : (value_type)instance->field; \
    } \
    static inline void prefix##_set_##field (struct_type *instance, value_type value) \
    { \
        if (mono_use_be_bitfield_accessors) \
            mono_be_bitfield_set (instance->storage_member, sizeof (instance->storage_member), shift, width, (guint32)value); \
        else \
            instance->field = value; \
    }
#endif /* MONO_BE_BITFIELD_ACCESSORS_DEFINED */

#define MONO_SECMAN_FLAG_INIT(x)       (x & 0x2)
#define MONO_SECMAN_FLAG_GET_VALUE(x)      (x & 0x1)
#define MONO_SECMAN_FLAG_SET_VALUE(x,y)    do { x = ((y) ? 0x3 : 0x2); } while (0)

struct _MonoAssembly {
    /* 
     * The number of appdomains which have this assembly loaded plus the number of 
     * assemblies referencing this assembly through an entry in their image->references
     * arrays. The later is needed because entries in the image->references array
     * might point to assemblies which are only loaded in some appdomains, and without
     * the additional reference, they can be freed at any time.
     * The ref_count is initially 0.
     */
    int ref_count; /* use atomic operations only */
    char *basedir;
    MonoAssemblyName aname;
    MonoImage *image;
    GSList *friend_assembly_names; /* Computed by mono_assembly_load_friends () */
    guint8 friend_assembly_names_inited;
    guint8 in_gac;
    guint8 dynamic;
    guint8 corlib_internal;
    gboolean ref_only;
    /* security manager flags (one bit is for lazy initialization) */
    union {
        guint8 be_secman_flags[4];
        struct {
            guint32 ecma:2;       /* Has the ECMA key */
            guint32 aptc:2;       /* Has the [AllowPartiallyTrustedCallers] attributes */
            guint32 fulltrust:2;  /* Has FullTrust permission */
            guint32 unmanaged:2;  /* Has SecurityPermissionFlag.UnmanagedCode permission */
            guint32 skipverification:2; /* Has SecurityPermissionFlag.SkipVerification permission */
        };
    };
};

typedef struct {
    const char* data;
    guint32  size;
} MonoStreamHeader;

struct _MonoTableInfo {
    const char *base;
    union {
        guint8 be_row_info[4];
        struct {
            guint rows     : 24;
            guint row_size : 8;
        };
    };

    /*
     * Tables contain up to 9 columns and the possible sizes of the
     * fields in the documentation are 1, 2 and 4 bytes.  So we
     * can encode in 2 bits the size.
     *
     * A 32 bit value can encode the resulting size
     *
     * The top eight bits encode the number of columns in the table.
     * we only need 4, but 8 is aligned no shift required. 
     */
    guint32   size_bitfield;
};

#define REFERENCE_MISSING ((gpointer) -1)

typedef struct _MonoDllMap MonoDllMap;

struct _MonoImage {
    /*
     * The number of assemblies which reference this MonoImage though their 'image'
     * field plus the number of images which reference this MonoImage through their 
     * 'modules' field, plus the number of threads holding temporary references to
     * this image between calls of mono_image_open () and mono_image_close ().
     */
    int   ref_count;
    void *raw_data_handle;
    char *raw_data;
    guint32 raw_data_len;
    union {
        guint8 be_image_flags[2];
        struct {
            guint8 raw_buffer_used    : 1;
            guint8 raw_data_allocated : 1;

#ifdef USE_COREE
            /* Module was loaded using LoadLibrary. */
            guint8 is_module_handle : 1;

            /* Module entry point is _CorDllMain. */
            guint8 has_entry_point : 1;
#endif

            /* Whenever this is a dynamically emitted module */
            guint8 dynamic : 1;

            /* Whenever this is a reflection only image */
            guint8 ref_only : 1;

            /* Whenever this image contains uncompressed metadata */
            guint8 uncompressed_metadata : 1;

            guint8 checked_module_cctor : 1;
            guint8 has_module_cctor : 1;

            guint8 idx_string_wide : 1;
            guint8 idx_guid_wide : 1;
            guint8 idx_blob_wide : 1;

            /* Whenever this image is considered as platform code for the CoreCLR security model */
            guint8 core_clr_platform_code : 1;
        };
    };
              
    char *name;
    const char *assembly_name;
    const char *module_name;
    char *version;
    gint16 md_version_major, md_version_minor;
    char *guid;
    void *image_info;
    MonoMemPool         *mempool; /*protected by the image lock*/

    char                *raw_metadata;
              
    MonoStreamHeader     heap_strings;
    MonoStreamHeader     heap_us;
    MonoStreamHeader     heap_blob;
    MonoStreamHeader     heap_guid;
    MonoStreamHeader     heap_tables;
              
    const char          *tables_base;

    /**/
    MonoTableInfo        tables [MONO_TABLE_NUM];

    /*
     * references is initialized only by using the mono_assembly_open
     * function, and not by using the lowlevel mono_image_open.
     *
     * It is NULL terminated.
     */
    MonoAssembly **references;

    MonoImage **modules;
    guint32 module_count;
    gboolean *modules_loaded;

    MonoImage **files;

    gpointer aot_module;

    /*
     * The Assembly this image was loaded from.
     */
    MonoAssembly *assembly;

    /*
     * Indexed by method tokens and typedef tokens.
     */
    GHashTable *method_cache; /*protected by the image lock*/
    MonoInternalHashTable class_cache;

    /* Indexed by memberref + methodspec tokens */
    GHashTable *methodref_cache; /*protected by the image lock*/

    /*
     * Indexed by fielddef and memberref tokens
     */
    GHashTable *field_cache;

    /* indexed by typespec tokens. */
    GHashTable *typespec_cache;
    /* indexed by token */
    GHashTable *memberref_signatures;
    GHashTable *helper_signatures;

    /* Indexed by blob heap indexes */
    GHashTable *method_signatures;

    /*
     * Indexes namespaces to hash tables that map class name to typedef token.
     */
    GHashTable *name_cache;  /*protected by the image lock*/

    /*
     * Indexed by MonoClass
     */
    GHashTable *array_cache;
    GHashTable *ptr_cache;

    GHashTable *szarray_cache;
    /* This has a separate lock to improve scalability */
    WapiCriticalSection szarray_cache_lock;

    /*
     * indexed by MonoMethodSignature 
     */
    GHashTable *delegate_begin_invoke_cache;
    GHashTable *delegate_end_invoke_cache;
    GHashTable *delegate_invoke_cache;
    GHashTable *runtime_invoke_cache;

    /*
     * indexed by SignatureMethodPair
     */
    GHashTable *delegate_abstract_invoke_cache;

    /*
     * indexed by MonoMethod pointers 
     */
    GHashTable *runtime_invoke_direct_cache;
    GHashTable *runtime_invoke_vcall_cache;
    GHashTable *managed_wrapper_cache;
    GHashTable *native_wrapper_cache;
    GHashTable *native_wrapper_aot_cache;
    GHashTable *remoting_invoke_cache;
    GHashTable *synchronized_cache;
    GHashTable *unbox_wrapper_cache;
    GHashTable *cominterop_invoke_cache;
    GHashTable *cominterop_wrapper_cache; /* LOCKING: marshal lock */
    GHashTable *thunk_invoke_cache;

    /*
     * indexed by MonoClass pointers
     */
    GHashTable *ldfld_wrapper_cache;
    GHashTable *ldflda_wrapper_cache;
    GHashTable *stfld_wrapper_cache;
    GHashTable *isinst_cache;
    GHashTable *castclass_cache;
    GHashTable *proxy_isinst_cache;
    GHashTable *rgctx_template_hash; /* LOCKING: templates lock */

    /*
     * indexed by token and MonoGenericContext pointer
     */
    GHashTable *generic_class_cache;

    /* Contains rarely used fields of runtime structures belonging to this image */
    MonoPropertyHash *property_hash;

    void *reflection_info;

    /*
     * user_info is a public field and is not touched by the
     * metadata engine
     */
    void *user_info;

    /* dll map entries */
    MonoDllMap *dll_map;

    /* interfaces IDs from this image */
    MonoBitSet *interface_bitset;

    GSList *reflection_info_unregister_classes;

    /*
     * No other runtime locks must be taken while holding this lock.
     * It's meant to be used only to mutate and query structures part of this image.
     */
    WapiCriticalSection   lock;
};

enum {
    MONO_SECTION_TEXT,
    MONO_SECTION_RSRC,
    MONO_SECTION_RELOC,
    MONO_SECTION_MAX
};

typedef struct {
    GHashTable *hash;
    char *data;
    guint32 alloc_size; /* malloced bytes */
    guint32 index;
    guint32 offset; /* from start of metadata */
} MonoDynamicStream;

typedef struct {
    guint32 alloc_rows;
    guint32 rows;
    guint8  row_size; /*  calculated later with column_sizes */
    guint8  columns;
    guint32 next_idx;
    guint32 *values; /* rows * columns */
} MonoDynamicTable;

struct _MonoDynamicAssembly {
    MonoAssembly assembly;
    char *strong_name;
    guint32 strong_name_size;
    guint8 run;
    guint8 save;
    MonoDomain *domain;
};

struct _MonoDynamicImage {
    MonoImage image;
    guint32 meta_size;
    guint32 text_rva;
    guint32 metadata_rva;
    guint32 image_base;
    guint32 cli_header_offset;
    guint32 iat_offset;
    guint32 idt_offset;
    guint32 ilt_offset;
    guint32 imp_names_offset;
    struct {
       guint32 rva;
       guint32 size;
       guint32 offset;
       guint32 attrs;
    } sections [MONO_SECTION_MAX];
    GHashTable *typespec;
    GHashTable *typeref;
    GHashTable *handleref;
    MonoGHashTable *tokens;
    GHashTable *blob_cache;
    GHashTable *standalonesig_cache;
    GList *array_methods;
    GPtrArray *gen_params;
    MonoGHashTable *token_fixups;
    GHashTable *method_to_table_idx;
    GHashTable *field_to_table_idx;
    GHashTable *method_aux_hash;
    MonoGHashTable *generic_def_objects;
    MonoGHashTable *methodspec;
    gboolean run;
    gboolean save;
    gboolean initial_image;
    guint32 pe_kind, machine;
    char *strong_name;
    guint32 strong_name_size;
    char *win32_res;
    guint32 win32_res_size;
    guint8 *public_key;
    int public_key_len;
    MonoDynamicStream sheap;
    MonoDynamicStream code; /* used to store method headers and bytecode */
    MonoDynamicStream resources; /* managed embedded resources */
    MonoDynamicStream us;
    MonoDynamicStream blob;
    MonoDynamicStream tstream;
    MonoDynamicStream guid;
    MonoDynamicTable tables [MONO_TABLE_NUM];
    MonoClass *wrappers_type; /*wrappers are bound to this type instead of <Module>*/
};

/* Contains information about assembly binding */
typedef struct _MonoAssemblyBindingInfo {
    char *name;
    char *culture;
    guchar public_key_token [MONO_PUBLIC_KEY_TOKEN_LENGTH];
    int major;
    int minor;
    AssemblyVersionSet old_version_bottom;
    AssemblyVersionSet old_version_top;
    AssemblyVersionSet new_version;
    union {
        guint8 be_binding_flags[4];
        struct {
            guint has_old_version_bottom : 1;
            guint has_old_version_top : 1;
            guint has_new_version : 1;
            guint is_valid : 1;
        };
    };
} MonoAssemblyBindingInfo;

struct _MonoMethodHeader {
    guint32      code_size;
    const unsigned char  *code;
    guint16      max_stack;
    union {
        guint8 be_clause_flags[4];
        struct {
            unsigned int num_clauses : 15;
            /* if num_locals != 0, then the following apply: */
            unsigned int init_locals : 1;
        };
    };
    guint16      num_locals;
    MonoExceptionClause *clauses;
    MonoType    *locals [MONO_ZERO_LEN_ARRAY];
};

/* MonoAssembly security-manager flags: big-endian bit positions in 32 bits. */
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssembly, mono_assembly, be_secman_flags, ecma, guint32, 30, 2)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssembly, mono_assembly, be_secman_flags, aptc, guint32, 28, 2)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssembly, mono_assembly, be_secman_flags, fulltrust, guint32, 26, 2)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssembly, mono_assembly, be_secman_flags, unmanaged, guint32, 24, 2)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssembly, mono_assembly, be_secman_flags, skipverification, guint32, 22, 2)

/* MonoTableInfo: rows occupy the high 24 bits, row_size the low 8 bits. */
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoTableInfo, mono_table_info_accessor, be_row_info, rows, guint, 8, 24)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoTableInfo, mono_table_info_accessor, be_row_info, row_size, guint, 0, 8)

/* MonoImage flags: guint8 bitfields are packed most-significant bit first. */
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, raw_buffer_used, guint8, 15, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, raw_data_allocated, guint8, 14, 1)
#ifdef USE_COREE
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, is_module_handle, guint8, 13, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, has_entry_point, guint8, 12, 1)
#endif
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, dynamic, guint8, 11, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, ref_only, guint8, 10, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, uncompressed_metadata, guint8, 9, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, checked_module_cctor, guint8, 8, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, has_module_cctor, guint8, 7, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, idx_string_wide, guint8, 6, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, idx_guid_wide, guint8, 5, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, idx_blob_wide, guint8, 4, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoImage, mono_image, be_image_flags, core_clr_platform_code, guint8, 3, 1)

/* MonoAssemblyBindingInfo flags. */
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssemblyBindingInfo, mono_assembly_binding_info, be_binding_flags, has_old_version_bottom, guint, 31, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssemblyBindingInfo, mono_assembly_binding_info, be_binding_flags, has_old_version_top, guint, 30, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssemblyBindingInfo, mono_assembly_binding_info, be_binding_flags, has_new_version, guint, 29, 1)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoAssemblyBindingInfo, mono_assembly_binding_info, be_binding_flags, is_valid, guint, 28, 1)

/* MonoMethodHeader flags. */
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoMethodHeader, mono_method_header_accessor, be_clause_flags, num_clauses, guint32, 17, 15)
MONO_DEFINE_BE_BITFIELD_ACCESSORS (MonoMethodHeader, mono_method_header_accessor, be_clause_flags, init_locals, guint32, 16, 1)

typedef struct {
    guint32      code_size;
    gboolean     has_clauses;
} MonoMethodHeaderSummary;

#define MONO_SIZEOF_METHOD_HEADER (sizeof (struct _MonoMethodHeader) - MONO_ZERO_LEN_ARRAY * SIZEOF_VOID_P)

/* for use with allocated memory blocks (assumes alignment is to 8 bytes) */
guint mono_aligned_addr_hash (gconstpointer ptr) MONO_INTERNAL;

void
mono_image_check_for_module_cctor (MonoImage *image) MONO_INTERNAL;

gpointer
mono_image_alloc  (MonoImage *image, guint size) MONO_INTERNAL;

gpointer
mono_image_alloc0 (MonoImage *image, guint size) MONO_INTERNAL;

char*
mono_image_strdup (MonoImage *image, const char *s) MONO_INTERNAL;

GList*
g_list_prepend_image (MonoImage *image, GList *list, gpointer data) MONO_INTERNAL;

GSList*
g_slist_append_image (MonoImage *image, GSList *list, gpointer data) MONO_INTERNAL;

void
mono_image_lock (MonoImage *image) MONO_INTERNAL;

void
mono_image_unlock (MonoImage *image) MONO_INTERNAL;

gpointer
mono_image_property_lookup (MonoImage *image, gpointer subject, guint32 property) MONO_INTERNAL;

void
mono_image_property_insert (MonoImage *image, gpointer subject, guint32 property, gpointer value) MONO_INTERNAL;

void
mono_image_property_remove (MonoImage *image, gpointer subject) MONO_INTERNAL;


MonoType*
mono_metadata_get_shared_type (MonoType *type) MONO_INTERNAL;

void
mono_metadata_clean_for_image (MonoImage *image) MONO_INTERNAL;

void
mono_metadata_cleanup (void);

const char *   mono_meta_table_name              (int table) MONO_INTERNAL;
void           mono_metadata_compute_table_bases (MonoImage *meta) MONO_INTERNAL;

gboolean
mono_metadata_interfaces_from_typedef_full  (MonoImage             *image,
                                   guint32                table_index,
                                   MonoClass           ***interfaces,
                                   guint                 *count,
                                   gboolean               heap_alloc_result,
                                   MonoGenericContext    *context) MONO_INTERNAL;

MonoArrayType *
mono_metadata_parse_array_full              (MonoImage             *image,
                     MonoGenericContainer  *container,
                     const char            *ptr,
                     const char           **rptr) MONO_INTERNAL;

MonoType *
mono_metadata_parse_type_full               (MonoImage             *image,
                     MonoGenericContainer  *container,
                     MonoParseTypeMode      mode,
                     short                  opt_attrs,
                     const char            *ptr,
                     const char           **rptr);

MonoMethodSignature *
mono_metadata_parse_signature_full          (MonoImage             *image,
                     MonoGenericContainer  *generic_container,
                     guint32                token) MONO_INTERNAL;

MonoMethodSignature *
mono_metadata_parse_method_signature_full   (MonoImage             *image,
                     MonoGenericContainer  *generic_container,
                     int                     def,
                     const char             *ptr,
                     const char            **rptr);

MonoMethodHeader *
mono_metadata_parse_mh_full                 (MonoImage             *image,
                     MonoGenericContainer  *container,
                     const char            *ptr);

gboolean
mono_method_get_header_summary (MonoMethod *method, MonoMethodHeaderSummary *summary) MONO_INTERNAL;

int* mono_metadata_get_param_attrs          (MonoImage *m, int def, int param_count) MONO_INTERNAL;
gboolean mono_metadata_method_has_param_attrs (MonoImage *m, int def) MONO_INTERNAL;

guint
mono_metadata_generic_context_hash          (const MonoGenericContext *context) MONO_INTERNAL;

gboolean
mono_metadata_generic_context_equal         (const MonoGenericContext *g1,
                     const MonoGenericContext *g2) MONO_INTERNAL;

MonoGenericInst *
mono_metadata_parse_generic_inst            (MonoImage             *image,
                     MonoGenericContainer  *container,
                     int                    count,
                     const char            *ptr,
                     const char           **rptr) MONO_INTERNAL;

MonoGenericInst *
mono_metadata_get_generic_inst              (int           type_argc,
                     MonoType       **type_argv) MONO_INTERNAL;

MonoGenericClass *
mono_metadata_lookup_generic_class          (MonoClass        *gclass,
                     MonoGenericInst      *inst,
                     gboolean         is_dynamic) MONO_INTERNAL;

MonoGenericInst * mono_metadata_inflate_generic_inst  (MonoGenericInst *ginst, MonoGenericContext *context, MonoError *error) MONO_INTERNAL;

void mono_dynamic_stream_reset  (MonoDynamicStream* stream) MONO_INTERNAL;
void mono_assembly_addref       (MonoAssembly *assembly) MONO_INTERNAL;
void mono_assembly_load_friends (MonoAssembly* ass) MONO_INTERNAL;
gboolean mono_assembly_has_skip_verification (MonoAssembly* ass) MONO_INTERNAL;

gboolean mono_public_tokens_are_equal (const unsigned char *pubt1, const unsigned char *pubt2) MONO_INTERNAL;

void mono_config_parse_publisher_policy (const char *filename, MonoAssemblyBindingInfo *binding_info) MONO_INTERNAL;

gboolean
mono_assembly_name_parse_full           (const char      *name,
                      MonoAssemblyName    *aname,
                      gboolean save_public_key,
                      gboolean *is_version_defined,
                     gboolean *is_token_defined) MONO_INTERNAL;

guint32 mono_metadata_get_generic_param_row (MonoImage *image, guint32 token, guint32 *owner);

void mono_unload_interface_ids (MonoBitSet *bitset) MONO_INTERNAL;


MonoType *mono_metadata_type_dup (MonoImage *image, const MonoType *original) MONO_INTERNAL;
MonoMethodSignature  *mono_metadata_signature_dup_full (MonoImage *image,MonoMethodSignature *sig) MONO_INTERNAL;
MonoMethodSignature  *mono_metadata_signature_dup_mempool (MonoMemPool *mp, MonoMethodSignature *sig) MONO_INTERNAL;

MonoGenericInst *
mono_get_shared_generic_inst (MonoGenericContainer *container) MONO_INTERNAL;

int
mono_type_stack_size_internal (MonoType *t, int *align, gboolean allow_open) MONO_INTERNAL;

gboolean
mono_metadata_type_equal_full (MonoType *t1, MonoType *t2, gboolean signature_only) MONO_INTERNAL;

MonoMarshalSpec *
mono_metadata_parse_marshal_spec_full (MonoImage *image, const char *ptr) MONO_INTERNAL;

guint          mono_metadata_generic_inst_hash (gconstpointer data) MONO_INTERNAL;
gboolean       mono_metadata_generic_inst_equal (gconstpointer ka, gconstpointer kb) MONO_INTERNAL;

void
mono_metadata_field_info_with_mempool (
                  MonoImage *meta, 
                   guint32       table_index,
                   guint32      *offset,
                   guint32      *rva,
                   MonoMarshalSpec **marshal_spec);

MonoClassField*
mono_metadata_get_corresponding_field_from_generic_type_definition (MonoClassField *field) MONO_INTERNAL;

MonoEvent*
mono_metadata_get_corresponding_event_from_generic_type_definition (MonoEvent *event) MONO_INTERNAL;

MonoProperty*
mono_metadata_get_corresponding_property_from_generic_type_definition (MonoProperty *property) MONO_INTERNAL;

guint32
mono_metadata_signature_size (MonoMethodSignature *sig) MONO_INTERNAL;

gboolean mono_image_load_pe_data (MonoImage *image) MONO_INTERNAL;

gboolean mono_image_load_cli_data (MonoImage *image) MONO_INTERNAL;

void mono_image_load_names (MonoImage *image) MONO_INTERNAL;

MonoImage *mono_image_open_raw (const char *fname, MonoImageOpenStatus *status) MONO_INTERNAL;

#endif /* __MONO_METADATA_INTERNALS_H__ */
