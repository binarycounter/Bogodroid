/* jni.c -- fake JNI environment for libll1.so (Level-5 LT1R)
 *
 * libll1.so talks to the "Java" MainActivity entirely through the JNIEnv we
 * hand it: it resolves method ids by name (GetMethodID / GetStaticMethodID)
 * and then dispatches platform services (movie playback, PNG decoding, the
 * software keyboard, save directory) through Call*Method. We intercept those
 * at the JNI boundary instead of running the real native method bodies.
 *
 * The env function table is indexed exactly as the JNI 1.6 spec lays it out,
 * which matches the slot offsets the Vita port found for this same binary.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <switch.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#include "jni.h"
#include "util.h"
#include "movie.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#define JNI_OK 0
#define JNI_VERSION_1_6 0x00010006

typedef uint64_t juint;

// ---------------------------------------------------------------------------
// fake object model
// ---------------------------------------------------------------------------

enum {
  TAG_OBJECT = 0x4f424a31,
  TAG_STRING = 0x53545231,
  TAG_OBJARR = 0x4f415231,
  TAG_PRIARR = 0x50415231,
  TAG_ID     = 0x4d494431,
};

typedef struct { uint32_t tag; char label[64]; } FakeObject;
typedef struct { uint32_t tag; char *utf; } FakeString;
typedef struct { uint32_t tag; int len; void **items; } FakeObjArray;
typedef struct { uint32_t tag; int len; int elem_size; void *data; } FakePriArray;
typedef struct { uint32_t tag; char name[64]; char sig[96]; } FakeID;

// ---------------------------------------------------------------------------
// live-object registry
//
// the engine manages local refs explicitly (DeleteLocalRef and
// Release*ArrayElements) -- most importantly for GL_LoadPNG, whose int[]
// results are ~4 MB per texture and leaked the whole heap before this.
// tracking every object we hand out lets both paths free aggressively while
// staying safe if the engine calls more than one of them on the same object.
// ---------------------------------------------------------------------------

#define REG_MAX 8192

static struct { void *ptr; uint8_t pinned; } reg_live[REG_MAX];
static int reg_count = 0;
static Mutex reg_lock;

static void reg_add(void *p) {
  if (!p)
    return;
  mutexLock(&reg_lock);
  if (reg_count < REG_MAX) {
    reg_live[reg_count].ptr = p;
    reg_live[reg_count].pinned = 0;
    reg_count++;
  } else {
    debugPrintf("JNI: object registry full, %p untracked\n", p);
  }
  mutexUnlock(&reg_lock);
}

static int reg_index(void *p) {
  for (int i = 0; i < reg_count; i++)
    if (reg_live[i].ptr == p)
      return i;
  return -1;
}

static void reg_set_pinned(void *p, int pinned) {
  mutexLock(&reg_lock);
  const int i = reg_index(p);
  if (i >= 0)
    reg_live[i].pinned = (uint8_t)pinned;
  mutexUnlock(&reg_lock);
}

// removes p from the registry; 0 means "don't free" (unknown, already freed,
// or pinned by a global ref)
static int reg_take(void *p) {
  if (!p)
    return 0;
  mutexLock(&reg_lock);
  const int i = reg_index(p);
  const int ok = (i >= 0 && !reg_live[i].pinned);
  if (ok) {
    reg_live[i] = reg_live[reg_count - 1];
    reg_count--;
  }
  mutexUnlock(&reg_lock);
  return ok;
}

// free any object we handed to the game, exactly once
static void obj_free(void *obj) {
  if (!reg_take(obj))
    return;
  switch (*(uint32_t *)obj) {
    case TAG_STRING: free(((FakeString *)obj)->utf); break;
    case TAG_PRIARR: free(((FakePriArray *)obj)->data); break;
    case TAG_OBJARR: free(((FakeObjArray *)obj)->items); break;
    default: break;
  }
  free(obj);
}

static void *jni_make_object(const char *label) {
  FakeObject *o = calloc(1, sizeof(*o));
  o->tag = TAG_OBJECT;
  strncpy(o->label, label, sizeof(o->label) - 1);
  reg_add(o);
  return o;
}

static void *jni_make_string(const char *utf) {
  FakeString *s = calloc(1, sizeof(*s));
  s->tag = TAG_STRING;
  s->utf = strdup(utf ? utf : "");
  reg_add(s);
  return s;
}

static const char *obj_str(void *jstr) {
  FakeString *s = jstr;
  if (s && s->tag == TAG_STRING)
    return s->utf;
  return "";
}

// ---------------------------------------------------------------------------
// method/field id pool (ids are pointers to these; dispatch is by name)
// ---------------------------------------------------------------------------

#define MAX_IDS 128
static FakeID id_pool[MAX_IDS];
static int id_count = 0;

static FakeID *get_id(const char *name, const char *sig) {
  for (int i = 0; i < id_count; i++)
    if (!strcmp(id_pool[i].name, name) && !strcmp(id_pool[i].sig, sig ? sig : ""))
      return &id_pool[i];
  if (id_count >= MAX_IDS)
    return &id_pool[0];
  FakeID *id = &id_pool[id_count++];
  id->tag = TAG_ID;
  strncpy(id->name, name, sizeof(id->name) - 1);
  strncpy(id->sig, sig ? sig : "", sizeof(id->sig) - 1);
  return id;
}

// ---------------------------------------------------------------------------
// software keyboard (UI_*EditText)
// ---------------------------------------------------------------------------

static char ime_text[512] = "";
static int ime_editing = 0;

#define IME_TEXT_MAX ((int)sizeof(ime_text) - 1)

static void ime_start(const char *initial, int edit_type) {
  SwkbdConfig kbd;
  if (R_FAILED(swkbdCreate(&kbd, 0))) {
    ime_text[0] = 0;
    ime_editing = 0;
    return;
  }
  swkbdConfigMakePresetDefault(&kbd);
  if (edit_type != 0)
    swkbdConfigSetType(&kbd, SwkbdType_NumPad);
  if (initial && initial[0])
    swkbdConfigSetInitialText(&kbd, initial);
  swkbdConfigSetStringLenMax(&kbd, IME_TEXT_MAX);
  // blocks on the system applet; the game polls UI_GetEditState afterwards
  ime_editing = 1;
  if (R_FAILED(swkbdShow(&kbd, ime_text, sizeof(ime_text))))
    ime_text[0] = 0;
  swkbdClose(&kbd);
  ime_editing = 0;
}

// ---------------------------------------------------------------------------
// Call*Method dispatch, by method name
// ---------------------------------------------------------------------------

static juint call_boolean(FakeID *id, va_list va) {
  const char *name = id->name;
  if (!strcmp(name, "MO_PlayMovie")) {
    const char *file = obj_str(va_arg(va, void *));
    int64_t offs = 0, size = 0;
    // the (String,int,int) overload selects a region; (String) plays whole
    if (strstr(id->sig, "II)")) {
      offs = va_arg(va, int);
      size = va_arg(va, int);
    }
    debugPrintf("JNI: MO_PlayMovie(%s, %lld, %lld)\n", file, (long long)offs, (long long)size);
    return movie_open(file, offs, size);
  }
  if (!strcmp(name, "MO_GetState"))
    return movie_active();
  if (!strcmp(name, "UI_GetEditState"))
    return ime_editing;
  if (!strcmp(name, "L5iD_IsEndRequest"))
    return 1;
  // L5iD_* sign-in flow: report everything done/declined so nothing blocks
  if (!strncmp(name, "L5iD_", 5))
    return 0;
  debugPrintf("JNI: CallBoolean(%s) -> 0\n", name);
  return 0;
}

static juint call_int(FakeID *id, va_list va) {
  (void)va;
  const char *name = id->name;
  // LVL/LSH/SBS license & purchase state: 2 == success/licensed (per Vita)
  if (!strcmp(name, "LVL_GetState") || !strcmp(name, "LSH_GetState") ||
      !strcmp(name, "SBS_GetState"))
    return 2;
  if (!strcmp(name, "MO_GetPosition"))
    return movie_position_ms();
  debugPrintf("JNI: CallInt(%s) -> 0\n", name);
  return 0;
}

// constant returns reuse one pinned string each: these get requested every
// frame in some scenes and per-call allocations would leak (the engine does
// not always DeleteLocalRef them)
static void *singleton_str(void **slot, const char *utf) {
  if (!*slot) {
    *slot = jni_make_string(utf);
    reg_set_pinned(*slot, 1);
  }
  return *slot;
}

static void *call_object(FakeID *id, va_list va) {
  (void)va;
  static void *s_dot, *s_empty, *s_version, *s_movie_mat;
  const char *name = id->name;
  if (!strcmp(name, "MO_UpdateTexture")) {
    // SurfaceTexture transform for the movie frame. GL_DrawMovie derives its
    // UV corners as T=(m[3],m[7]), A=(m[0],m[4]), B=(-m[1],-m[5]); Android's
    // standard flip matrix yields the valid (0,0)..(1,1) quad (identity would
    // produce negative V -> the clamped smear). pinned: the engine
    // Get/ReleaseFloatArrayElements this every MO_Render.
    if (!s_movie_mat) {
      static const float surface_mat[16] = {
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  1.0f, 0.0f, 1.0f,
      };
      FakePriArray *a = calloc(1, sizeof(*a));
      a->tag = TAG_PRIARR;
      a->len = 16;
      a->elem_size = sizeof(float);
      a->data = malloc(sizeof(surface_mat));
      memcpy(a->data, surface_mat, sizeof(surface_mat));
      reg_add(a);
      reg_set_pinned(a, 1);
      s_movie_mat = a;
    }
    return s_movie_mat;
  }
  if (!strcmp(name, "CARD_GetFilesDirName") || !strcmp(name, "CARD_GetExternalFilesDirName"))
    return singleton_str(&s_dot, ".");
  if (!strcmp(name, "UI_GetEditText"))
    return jni_make_string(ime_text);
  if (!strcmp(name, "DL_GetFileName"))
    return singleton_str(&s_empty, "");
  if (!strcmp(name, "OS_GetAppVersion"))
    return singleton_str(&s_version, "1.0.8");
  debugPrintf("JNI: CallObject(%s) -> empty string\n", name);
  return singleton_str(&s_empty, "");
}

static void call_void(FakeID *id, va_list va) {
  const char *name = id->name;
  if (!strcmp(name, "UI_SetIdleTimerDisabled")) {
    // the game disables the idle timer around cutscenes; map it to the
    // console's auto-sleep prevention
    const int disabled = va_arg(va, int);
    appletSetMediaPlaybackState(disabled != 0);
    return;
  }
  if (!strcmp(name, "CARD_CreateDirectory")) {
    const char *dir = obj_str(va_arg(va, void *));
    debugPrintf("JNI: CARD_CreateDirectory(%s)\n", dir);
    if (dir && dir[0])
      mkdir(dir, 0777); // best effort; the save dir is relative to "."
    return;
  }
  if (!strcmp(name, "UI_StartEditText")) {
    const char *initial = obj_str(va_arg(va, void *));
    const int edit_type = va_arg(va, int);
    ime_start(initial, edit_type);
    return;
  }
  if (!strcmp(name, "MO_PauseMovie")) {
    movie_pause(va_arg(va, int));
    return;
  }
  if (!strcmp(name, "MO_ReleaseMovie")) {
    movie_close();
    return;
  }
  if (!strcmp(name, "MO_SetVolume")) {
    movie_set_volume((float)va_arg(va, double));
    return;
  }
  debugPrintf("JNI: CallVoid(%s) ignored\n", name);
}

// static GL_LoadPNG(byte[]) -> int[]{ w, h, rgba... }
static void *call_static_object(FakeID *id, va_list va) {
  if (!strcmp(id->name, "GL_LoadPNG")) {
    FakePriArray *arr = va_arg(va, void *);
    if (!arr || arr->tag != TAG_PRIARR)
      return NULL;
    int w = 0, h = 0;
    uint8_t *px = stbi_load_from_memory(arr->data, arr->len, &w, &h, NULL, 4);
    if (!px)
      return NULL;
    // the engine uploads these as BGRA, so swap R/B (matches the Vita port)
    for (int i = 0; i < w * h; i++) {
      uint8_t t = px[i * 4 + 0];
      px[i * 4 + 0] = px[i * 4 + 2];
      px[i * 4 + 2] = t;
    }
    FakePriArray *res = calloc(1, sizeof(*res));
    res->tag = TAG_PRIARR;
    res->elem_size = sizeof(int);
    res->len = w * h + 2;
    res->data = malloc((size_t)(w * h + 2) * sizeof(int));
    int *out = res->data;
    out[0] = w;
    out[1] = h;
    memcpy(&out[2], px, (size_t)w * h * 4);
    stbi_image_free(px);
    // ~4 MB per texture: freed when the engine releases/deletes its ref
    reg_add(res);
    return res;
  }
  debugPrintf("JNI: CallStaticObject(%s) -> null\n", id->name);
  return NULL;
}

// ---------------------------------------------------------------------------
// JNIEnv function table
// ---------------------------------------------------------------------------

static juint j_GetVersion(void *env) { (void)env; return JNI_VERSION_1_6; }
static void *j_FindClass(void *env, const char *name) { (void)env; return jni_make_object(name); }
static void *j_GetMethodID(void *env, void *cls, const char *name, const char *sig) { (void)env; (void)cls; return get_id(name, sig); }
static void *j_GetFieldID(void *env, void *cls, const char *name, const char *sig) { (void)env; (void)cls; return get_id(name, sig); }
static void *j_GetObjectClass(void *env, void *obj) { (void)env; (void)obj; return jni_make_object("class"); }
// a global ref pins the object so a later DeleteLocalRef can't free it from
// under the engine (FindClass -> NewGlobalRef -> DeleteLocalRef pattern)
static void *j_NewGlobalRef(void *env, void *obj) {
  (void)env;
  reg_set_pinned(obj, 1);
  return obj;
}
static void *j_NewLocalRef(void *env, void *obj) { (void)env; return obj; }
static juint j_ret0_2(void *env, void *a) { (void)env; (void)a; return 0; }
static juint j_ret0_3(void *env, void *a, void *b) { (void)env; (void)a; (void)b; return 0; }

static void  j_DeleteGlobalRef(void *env, void *obj) {
  (void)env;
  reg_set_pinned(obj, 0);
  obj_free(obj);
}

static void  j_DeleteLocalRef(void *env, void *obj) {
  (void)env;
  obj_free(obj);
}

static juint j_CallBooleanMethodV(void *env, void *obj, FakeID *id, va_list va) { (void)env; (void)obj; return call_boolean(id, va); }
static juint j_CallBooleanMethod(void *env, void *obj, FakeID *id, ...) {
  va_list va; va_start(va, id); juint r = call_boolean(id, va); va_end(va); return r;
}
static juint j_CallIntMethodV(void *env, void *obj, FakeID *id, va_list va) { (void)env; (void)obj; return call_int(id, va); }
static juint j_CallIntMethod(void *env, void *obj, FakeID *id, ...) {
  va_list va; va_start(va, id); juint r = call_int(id, va); va_end(va); return r;
}
static void *j_CallObjectMethodV(void *env, void *obj, FakeID *id, va_list va) { (void)env; (void)obj; return call_object(id, va); }
static void *j_CallObjectMethod(void *env, void *obj, FakeID *id, ...) {
  va_list va; va_start(va, id); void *r = call_object(id, va); va_end(va); return r;
}
static void j_CallVoidMethodV(void *env, void *obj, FakeID *id, va_list va) { (void)env; (void)obj; call_void(id, va); }
static void j_CallVoidMethod(void *env, void *obj, FakeID *id, ...) {
  va_list va; va_start(va, id); call_void(id, va); va_end(va);
}
static juint j_CallLongMethodV(void *env, void *obj, FakeID *id, va_list va) {
  (void)env; (void)obj; (void)va;
  // free space for the save-data checks; returning 0 here could block saving
  if (!strcmp(id->name, "CARD_GetAvailableBytes"))
    return 1ull << 30; // report 1 GB free
  debugPrintf("JNI: CallLong(%s) -> 0\n", id->name);
  return 0;
}

static void *j_CallStaticObjectMethodV(void *env, void *cls, FakeID *id, va_list va) { (void)env; (void)cls; return call_static_object(id, va); }
static void *j_CallStaticObjectMethod(void *env, void *cls, FakeID *id, ...) {
  va_list va; va_start(va, id); void *r = call_static_object(id, va); va_end(va); return r;
}
static juint j_CallStaticBoolIntMethodV(void *env, void *cls, FakeID *id, va_list va) { (void)env; (void)cls; (void)id; (void)va; return 0; }
static juint j_CallStaticBoolIntMethod(void *env, void *cls, FakeID *id, ...) { (void)env; (void)cls; (void)id; return 0; }
static void j_CallStaticVoidMethodV(void *env, void *cls, FakeID *id, va_list va) { (void)env; (void)cls; (void)id; (void)va; }
static void j_CallStaticVoidMethod(void *env, void *cls, FakeID *id, ...) { (void)env; (void)cls; (void)id; }

static void *j_NewObjectV(void *env, void *cls, FakeID *id, va_list va) { (void)env; (void)cls; (void)id; (void)va; return jni_make_object("obj"); }
static void *j_NewObject(void *env, void *cls, FakeID *id, ...) { (void)env; (void)cls; (void)id; return jni_make_object("obj"); }

static void *j_GetObjectField(void *env, void *obj, FakeID *id) { (void)env; (void)obj; (void)id; return NULL; }
static juint j_GetBooleanField(void *env, void *obj, FakeID *id) { (void)env; (void)obj; (void)id; return 0; }
static juint j_GetIntField(void *env, void *obj, FakeID *id) { (void)env; (void)obj; (void)id; return 0; }

static void *j_NewStringUTF(void *env, const char *utf) { (void)env; return jni_make_string(utf); }
static const char *j_GetStringUTFChars(void *env, void *jstr, uint8_t *is_copy) { (void)env; if (is_copy) *is_copy = 0; return obj_str(jstr); }
static void j_ReleaseStringUTFChars(void *env, void *jstr, const char *utf) { (void)env; (void)jstr; (void)utf; }
static juint j_GetStringUTFLength(void *env, void *jstr) { (void)env; return strlen(obj_str(jstr)); }
static juint j_GetStringLength(void *env, void *jstr) { (void)env; return strlen(obj_str(jstr)); }

static juint j_GetArrayLength(void *env, void *arr) {
  (void)env;
  FakeObjArray *a = arr;
  if (a && (a->tag == TAG_OBJARR || a->tag == TAG_PRIARR))
    return a->len;
  return 0;
}

static void *new_pri_array(int len, int elem_size) {
  FakePriArray *a = calloc(1, sizeof(*a));
  a->tag = TAG_PRIARR;
  a->len = len;
  a->elem_size = elem_size;
  a->data = calloc(len ? len : 1, elem_size);
  reg_add(a);
  return a;
}
static void *j_NewByteArray(void *env, int len) { (void)env; return new_pri_array(len, 1); }
static void *j_NewIntArray(void *env, int len) { (void)env; return new_pri_array(len, 4); }
static void *j_NewFloatArray(void *env, int len) { (void)env; return new_pri_array(len, 4); }

static void *j_GetPriArrayElements(void *env, void *arr, uint8_t *is_copy) {
  (void)env;
  if (is_copy) *is_copy = 0;
  FakePriArray *a = arr;
  return (a && a->tag == TAG_PRIARR) ? a->data : NULL;
}
// the engine drives array lifetime through this call (the Vita port freed
// here too); JNI_COMMIT (1) means it will keep using the buffer
static void j_ReleasePriArrayElements(void *env, void *arr, void *elems, int mode) {
  (void)env; (void)elems;
  if (mode != 1 /* JNI_COMMIT */)
    obj_free(arr);
}

static void j_GetPriArrayRegion(void *env, void *arr, int start, int len, void *buf) {
  (void)env;
  FakePriArray *a = arr;
  if (a && a->tag == TAG_PRIARR && start >= 0 && start + len <= a->len)
    memcpy(buf, (char *)a->data + start * a->elem_size, (size_t)len * a->elem_size);
}
static void j_SetPriArrayRegion(void *env, void *arr, int start, int len, const void *buf) {
  (void)env;
  FakePriArray *a = arr;
  if (a && a->tag == TAG_PRIARR && start >= 0 && start + len <= a->len)
    memcpy((char *)a->data + start * a->elem_size, buf, (size_t)len * a->elem_size);
}

static juint j_RegisterNatives(void *env, void *cls, void *methods, int n) { (void)env; (void)cls; (void)methods; (void)n; return 0; }
static juint j_GetJavaVM(void *env, void **vm) { (void)env; *vm = fake_vm; return JNI_OK; }
static juint j_ExceptionCheck(void *env) { (void)env; return 0; }
static void *j_ExceptionOccurred(void *env) { (void)env; return NULL; }
static void j_void_1(void *env) { (void)env; }
static juint j_PushLocalFrame(void *env, int cap) { (void)env; (void)cap; return 0; }
static void *j_PopLocalFrame(void *env, void *result) { (void)env; return result; }

static juint j_unimplemented(void) {
  debugPrintf("JNI: call to unimplemented slot\n");
  return 0;
}

static void *env_table[233];
static void **env_table_ptr = env_table;
void *fake_env = &env_table_ptr;

static juint vm_GetEnv(void *vm, void **env, int version) { (void)vm; (void)version; if (env) *env = fake_env; return JNI_OK; }
static juint vm_AttachCurrentThread(void *vm, void **env, void *args) { (void)vm; (void)args; if (env) *env = fake_env; return JNI_OK; }
static juint vm_ret_ok(void *vm) { (void)vm; return JNI_OK; }

static void *vm_table[8];
static void **vm_table_ptr = vm_table;
void *fake_vm = &vm_table_ptr;

void jni_init(void) {
  for (int i = 0; i < 233; i++)
    env_table[i] = (void *)j_unimplemented;

  env_table[4]  = (void *)j_GetVersion;
  env_table[6]  = (void *)j_FindClass;
  env_table[15] = (void *)j_ExceptionOccurred;
  env_table[16] = (void *)j_void_1; // ExceptionDescribe
  env_table[17] = (void *)j_void_1; // ExceptionClear
  env_table[19] = (void *)j_PushLocalFrame;
  env_table[20] = (void *)j_PopLocalFrame;
  env_table[21] = (void *)j_NewGlobalRef;
  env_table[22] = (void *)j_DeleteGlobalRef;
  env_table[23] = (void *)j_DeleteLocalRef;
  env_table[24] = (void *)j_ret0_3;    // IsSameObject
  env_table[25] = (void *)j_NewLocalRef;
  env_table[26] = (void *)j_ret0_2;    // EnsureLocalCapacity
  env_table[28] = (void *)j_NewObject;
  env_table[29] = (void *)j_NewObjectV;
  env_table[31] = (void *)j_GetObjectClass;
  env_table[33] = (void *)j_GetMethodID;
  env_table[34] = (void *)j_CallObjectMethod;
  env_table[35] = (void *)j_CallObjectMethodV;
  env_table[37] = (void *)j_CallBooleanMethod;
  env_table[38] = (void *)j_CallBooleanMethodV;
  env_table[49] = (void *)j_CallIntMethod;
  env_table[50] = (void *)j_CallIntMethodV;
  env_table[53] = (void *)j_CallLongMethodV;
  env_table[61] = (void *)j_CallVoidMethod;
  env_table[62] = (void *)j_CallVoidMethodV;
  env_table[94] = (void *)j_GetFieldID;
  env_table[95] = (void *)j_GetObjectField;
  env_table[96] = (void *)j_GetBooleanField;
  env_table[100] = (void *)j_GetIntField;
  env_table[113] = (void *)j_GetMethodID; // GetStaticMethodID
  env_table[114] = (void *)j_CallStaticObjectMethod;
  env_table[115] = (void *)j_CallStaticObjectMethodV;
  env_table[117] = (void *)j_CallStaticBoolIntMethod;  // CallStaticBooleanMethod
  env_table[118] = (void *)j_CallStaticBoolIntMethodV; // CallStaticBooleanMethodV
  env_table[129] = (void *)j_CallStaticBoolIntMethod;  // CallStaticIntMethod
  env_table[130] = (void *)j_CallStaticBoolIntMethodV; // CallStaticIntMethodV
  env_table[141] = (void *)j_CallStaticVoidMethod;
  env_table[142] = (void *)j_CallStaticVoidMethodV;
  env_table[144] = (void *)j_GetFieldID; // GetStaticFieldID
  env_table[164] = (void *)j_GetStringLength;
  env_table[167] = (void *)j_NewStringUTF;
  env_table[168] = (void *)j_GetStringUTFLength;
  env_table[169] = (void *)j_GetStringUTFChars;
  env_table[170] = (void *)j_ReleaseStringUTFChars;
  env_table[171] = (void *)j_GetArrayLength;
  env_table[176] = (void *)j_NewByteArray;
  env_table[179] = (void *)j_NewIntArray;
  env_table[181] = (void *)j_NewFloatArray;
  for (int i = 183; i <= 190; i++)
    env_table[i] = (void *)j_GetPriArrayElements;
  for (int i = 191; i <= 198; i++)
    env_table[i] = (void *)j_ReleasePriArrayElements;
  for (int i = 199; i <= 206; i++)
    env_table[i] = (void *)j_GetPriArrayRegion;
  for (int i = 207; i <= 214; i++)
    env_table[i] = (void *)j_SetPriArrayRegion;
  env_table[215] = (void *)j_RegisterNatives;
  env_table[216] = (void *)j_ret0_2; // UnregisterNatives
  env_table[217] = (void *)j_ret0_2; // MonitorEnter
  env_table[218] = (void *)j_ret0_2; // MonitorExit
  env_table[219] = (void *)j_GetJavaVM;
  env_table[222] = (void *)j_GetPriArrayElements;     // GetPrimitiveArrayCritical
  env_table[223] = (void *)j_ReleasePriArrayElements; // ReleasePrimitiveArrayCritical
  env_table[228] = (void *)j_ExceptionCheck;

  vm_table[3] = (void *)vm_ret_ok;             // DestroyJavaVM
  vm_table[4] = (void *)vm_AttachCurrentThread;
  vm_table[5] = (void *)vm_ret_ok;             // DetachCurrentThread
  vm_table[6] = (void *)vm_GetEnv;
  vm_table[7] = (void *)vm_AttachCurrentThread; // AttachCurrentThreadAsDaemon

  debugPrintf("JNI: env=%p vm=%p\n", fake_env, fake_vm);
}
