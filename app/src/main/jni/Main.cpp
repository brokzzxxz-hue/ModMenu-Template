#include <list>
#include <vector>
#include <string.h>
#include <pthread.h>
#include <cstring>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include <cmath>

#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.h"
#include "KittyMemory/MemoryPatch.h"
#include "Menu.h"

// Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

#include "Includes/Macros.h"

// ==========================================
// OFFSETS DO JOGO (Atualize a cada patch)
// ==========================================
#define OFF_CURRENT_LOCAL_PLAYER 0x661bcdc
#define OFF_CURRENT_CAMERA_MANAGER 0x6629d20
#define OFF_GET_POSITION 0x7519f20
#define OFF_SET_EULER_ANGLES 0x751ae5c
#define OFF_SKELETON_MANAGER 0xEC4         // Offset onde fica o gerenciador de ossos no Player
#define OFF_GET_BONE_BY_NAME 0x64fdf30     // Offset da função BODJPJHIMAL
#define OFF_BONE_TRANSFORM 0x8             // Offset do Transform dentro do BFOONIGAIFK

// Fancy struct for patches for kittyMemory
struct My_Patches {
    MemoryPatch GodMode, GodMode2, SliderExample;
} hexPatches;

// Variáveis do Menu
bool feature1 = false;
bool feature2 = false;
bool featureHookToggle = false; // Aimbot Toggle
bool Health = false;            // ESP Toggle
int aimBone = 0;                // 0 = Cabeça, 1 = Peito
int sliderValue = 1;
int level = 0;
void *instanceBtn;

// ==========================================
// ESTRUTURAS E MATEMÁTICA
// ==========================================
struct Vector3 {
    float x, y, z;
    Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
};

Vector3 CalculateAngle(Vector3 src, Vector3 dst) {
    Vector3 delta = dst - src;
    float hyp = sqrt(delta.x * delta.x + delta.z * delta.z);
    Vector3 angles;
    angles.x = atan2(delta.y, hyp) * 180.0f / 3.14159265f; // Pitch
    angles.y = atan2(delta.x, delta.z) * 180.0f / 3.14159265f; // Yaw
    angles.z = 0.0f;
    return angles;
}

// ==========================================
// TYPEDEFS (Ponteiros para as funções)
// ==========================================
typedef Vector3(*get_Position_t)(void* transform);
typedef void(*set_EulerAngles_t)(void* transform, Vector3 angles);
typedef void* (*CurrentLocalPlayer_t)();
typedef void* (*CurrentCameraManager_t)();
typedef void* (*il2cpp_class_from_name_t)(const char* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_string_new_t)(const char* str);
typedef void* (*GetBoneByName_t)(void* skeletonManager, void* name);

// Função global para pegar a base
uintptr_t il2cppBase = 0;

// ==========================================
// FUNÇÃO PARA PEGAR A POSIÇÃO DO OSSO
// ==========================================
Vector3 GetBonePosition(uintptr_t playerEntity, const char* boneName) {
    // 1. Pega o SkeletonManager
    uintptr_t skeletonManager = *(uintptr_t*)(playerEntity + OFF_SKELETON_MANAGER);
    if (!skeletonManager) return {0,0,0};

    // 2. Resolve as funções
    il2cpp_string_new_t il2cpp_string_new = (il2cpp_string_new_t)dlsym(RTLD_DEFAULT, "il2cpp_string_new");
    if (!il2cpp_string_new) return {0,0,0};

    GetBoneByName_t GetBoneByName = (GetBoneByName_t)(il2cppBase + OFF_GET_BONE_BY_NAME);
    get_Position_t get_Position = (get_Position_t)(il2cppBase + OFF_GET_POSITION);

    // 3. Cria a string na memória IL2CPP (Ex: "Head")
    void* str = il2cpp_string_new(boneName);

    // 4. Chama a função do jogo
    uintptr_t boneNode = (uintptr_t)GetBoneByName((void*)skeletonManager, str);
    if (!boneNode) return {0,0,0};

    // 5. Pega a Transform do osso
    uintptr_t transform = *(uintptr_t*)(boneNode + OFF_BONE_TRANSFORM);
    if (!transform) return {0,0,0};

    // 6. Pega a posição
    return get_Position((void*)transform);
}

// ==========================================
// THREAD PRINCIPAL (O Loop do Hack)
// ==========================================
void *hack_thread(void *arg) {
    LOGI(OBFUSCATE("pthread created"));

    do {
        sleep(1);
    } while (!isLibraryLoaded(targetLibName));

    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);

    il2cppBase = getAbsoluteAddress(targetLibName, 0);

    // Resolve funções base
    il2cpp_class_from_name_t il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(RTLD_DEFAULT, "il2cpp_class_from_name");
    CurrentLocalPlayer_t CurrentLocalPlayer = (CurrentLocalPlayer_t)(il2cppBase + OFF_CURRENT_LOCAL_PLAYER);
    CurrentCameraManager_t CurrentCameraManager = (CurrentCameraManager_t)(il2cppBase + OFF_CURRENT_CAMERA_MANAGER);
    set_EulerAngles_t set_EulerAngles = (set_EulerAngles_t)(il2cppBase + OFF_SET_EULER_ANGLES);

    while (true) {
        if (featureHookToggle || Health) {
            
            if (il2cpp_class_from_name) {
                uintptr_t BaseGameClass = (uintptr_t)il2cpp_class_from_name("Assembly-CSharp.dll", "GCommon", "BaseGame");
                
                uintptr_t localPlayer = (uintptr_t)CurrentLocalPlayer();
                uintptr_t camManager = (uintptr_t)CurrentCameraManager();

                if (localPlayer && BaseGameClass) {
                    // Posição da SUA cabeça (para o cálculo do ângulo)
                    Vector3 localHeadPos = GetBonePosition(localPlayer, "Head"); 
                    if (localHeadPos.x == 0) {
                        // Fallback para a posição base se não achar o osso
                        get_Position_t get_Position = (get_Position_t)(il2cppBase + OFF_GET_POSITION);
                        localHeadPos = get_Position((void*)localPlayer);
                    }
                    
                    uintptr_t cameraTransform = 0;
                    if (camManager) {
                        cameraTransform = *(uintptr_t*)(camManager + 0x18); 
                    }

                    uintptr_t dictPtr = *(uintptr_t*)(BaseGameClass + 0x0); 
                    if (dictPtr) {
                        uintptr_t entries = *(uintptr_t*)(dictPtr + 0x18);
                        int count = *(int*)(dictPtr + 0x20);
                        
                        uintptr_t bestTarget = 0;
                        float closestDist = 9999.0f;

                        for (int i = 0; i < count; i++) {
                            uintptr_t entry = entries + (i * 24); 
                            uintptr_t entity = *(uintptr_t*)(entry + 0x10); 

                            if (!entity || entity == localPlayer) continue; 

                            // Para o Aimbot calcular a distância, usamos a posição geral do inimigo
                            get_Position_t get_Position = (get_Position_t)(il2cppBase + OFF_GET_POSITION);
                            Vector3 enemyPos = get_Position((void*)entity);

                            if (featureHookToggle) {
                                float dist = sqrt(pow(enemyPos.x - localHeadPos.x, 2) + pow(enemyPos.y - localHeadPos.y, 2) + pow(enemyPos.z - localHeadPos.z, 2));
                                
                                if (dist < closestDist) {
                                    closestDist = dist;
                                    bestTarget = entity;
                                }
                            }
                        }

                        // ==========================================
                        // APLICA O AIMBOT
                        // ==========================================
                        if (featureHookToggle && bestTarget && cameraTransform) {
                            
                            // Define qual osso puxar baseado na escolha do menu
                            const char* boneToAim = (aimBone == 0) ? "Head" : "Chest";
                            
                            // Pega a posição exata do osso escolhido
                            Vector3 targetPos = GetBonePosition(bestTarget, boneToAim);
                            
                            // Se falhar ao achar o osso, usa a posição base do inimigo
                            if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
                                get_Position_t get_Position = (get_Position_t)(il2cppBase + OFF_GET_POSITION);
                                targetPos = get_Position((void*)bestTarget);
                            }
                            
                            Vector3 aimAngles = CalculateAngle(localHeadPos, targetPos);
                            set_EulerAngles((void*)cameraTransform, aimAngles);
                        }
                    }
                }
            }
        }
        
        usleep(10000); // 10ms
    }
    return NULL;
}

// ==========================================
// JNI CALLS (O Menu do LGL)
// ==========================================
extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_uk_lgl_modmenu_FloatingModMenuService_getFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;
    MakeToast(env, context, OBFUSCATE("Modded by LGL"), Toast::LENGTH_LONG);

    const char *features[] = {
        OBFUSCATE("Category_Aimbot & ESP"),
        OBFUSCATE("Toggle_Aimbot"),                   // Feature 0
        OBFUSCATE("Spinner_Target Bone_Head,Chest"),  // Feature 1 (0 = Head, 1 = Chest)
        OBFUSCATE("Toggle_ESP")                       // Feature 2
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

JNIEXPORT void JNICALL
Java_uk_lgl_modmenu_Preferences_Changes(JNIEnv *env, jclass clazz, jobject obj,
                                        jint featNum, jstring featName, jint value,
                                        jboolean boolean, jstring str) {

    switch (featNum) {
        case 0:
            featureHookToggle = boolean; // Ativa/Desativa Aimbot
            break;
        case 1:
            aimBone = value; // 0 = Cabeça, 1 = Peito
            break;
        case 2:
            Health = boolean; // Ativa/Desativa ESP
            break;
    }
}

}

__attribute__((constructor))
void lib_main() {
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}
// Assigning feature numbers is optional. Without it, it will automatically count for you, starting from 0
// Assigned feature numbers can be like any numbers 1,3,200,10... instead in order 0,1,2,3,4,5...
// ButtonLink, Category, RichTextView and RichWebView is not counted. They can't have feature number assigned
// Toggle, ButtonOnOff and Checkbox can be switched on by default, if you add True_. Example: CheckBox_True_The Check Box
// To learn HTML, go to this page: https://www.w3schools.com/

jobjectArray GetFeatureList_Example(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
            OBFUSCATE("Toggle_No death"),
            OBFUSCATE("Button_Start Invcibility (30 sec duration)"),
            OBFUSCATE("SeekBar_Score multiplier_1_100"),
            OBFUSCATE("SeekBar_Coins multiplier_1_1000"),
            OBFUSCATE("Category_Examples"), //Not counted
            OBFUSCATE("Toggle_The toggle"),
            OBFUSCATE(
                    "100_Toggle_True_The toggle 2"), //This one have feature number assigned, and switched on by default
            OBFUSCATE("110_Toggle_The toggle 3"), //This one too
            OBFUSCATE("SeekBar_The slider_1_100"),
            OBFUSCATE("SeekBar_Kittymemory slider example_1_5"),
            OBFUSCATE("Spinner_The spinner_Items 1,Items 2,Items 3"),
            OBFUSCATE("Button_The button"),
            OBFUSCATE("ButtonLink_The button with link_https://www.youtube.com/"), //Not counted
            OBFUSCATE("ButtonOnOff_The On/Off button"),
            OBFUSCATE("CheckBox_The Check Box"),
            OBFUSCATE("InputValue_Input number"),
            OBFUSCATE("InputValue_1000_Input number 2"), //Max value
			OBFUSCATE("1111_InputLValue_Input long number"),
            OBFUSCATE("InputLValue_1000000000000_Input long number 2"), //Max value
            OBFUSCATE("InputText_Input text"),
            OBFUSCATE("RadioButton_Radio buttons_OFF,Mod 1,Mod 2,Mod 3"),

            //Create new collapse
            OBFUSCATE("Collapse_Collapse 1"),
            OBFUSCATE("CollapseAdd_Toggle_The toggle"),
            OBFUSCATE("123_CollapseAdd_Toggle_The toggle"),
            OBFUSCATE("122_CollapseAdd_CheckBox_Check box"),
            OBFUSCATE("CollapseAdd_Button_The button"),

            //Create new collapse again
            OBFUSCATE("Collapse_Collapse 2_True"),
            OBFUSCATE("CollapseAdd_SeekBar_The slider_1_100"),
            OBFUSCATE("CollapseAdd_InputValue_Input number"),

            OBFUSCATE("RichTextView_This is text view, not fully HTML."
                      "<b>Bold</b> <i>italic</i> <u>underline</u>"
                      "<br />New line <font color='red'>Support colors</font>"
                      "<br/><big>bigger Text</big>"),
            OBFUSCATE("RichWebView_<html><head><style>body{color: white;}</style></head><body>"
                      "This is WebView, with REAL HTML support!"
                      "<div style=\"background-color: darkblue; text-align: center;\">Support CSS</div>"
                      "<marquee style=\"color: green; font-weight:bold;\" direction=\"left\" scrollamount=\"5\" behavior=\"scroll\">This is <u>scrollable</u> text</marquee>"
                      "</body></html>")
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

void Changes_Example(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {

    switch (featNum) {
        case 0:
        {
            if (boolean)
                // gPatches.noDeath.Modify();
            else
                // gPatches.noDeath.Restore();
            break;
        }
        case 1:
            // btnPressed = true;
            break;
        case 2:
            // scoreMul = value;
            break;
        case 3:
            // coinsMul = value;
            break;
    }
}
-------------------------- */



void *hack_thread(void *) {
  LOGD(OBFUSCATE("pthread created"));

  do {
    sleep(1);
  } while (!isLibraryLoaded(targetLibName));

  LOGD(OBFUSCATE("%s has been loaded"), (const char *)targetLibName);

#if defined(__aarch64__)
  
#else

  LOGD(OBFUSCATE("Done"));
#endif

  return nullptr;
}

jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
  jobjectArray ret;

  const char *features[] = {
          
  };

  int Total_Feature = (sizeof features / sizeof features[0]);
  ret = (jobjectArray)env->NewObjectArray(
      Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
      env->NewStringUTF(""));

  for (int i = 0; i < Total_Feature; ++i)
    env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

  return (ret);
}

void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum,
             jstring featName, jint value, jboolean boolean, jstring str) {

  const char *name = env->GetStringUTFChars(featName, nullptr);
  const char *text = str != nullptr ? env->GetStringUTFChars(str, nullptr) : "";

  LOGD(OBFUSCATE("Feature name: %d - %s | Value: = %d | Bool: = %d | Text: = %s"),
       featNum, name, static_cast<int>(value), boolean, text);

  switch (featNum) {
      break;
  }
}

__attribute__((constructor)) void lib_main() {
  pthread_t ptid;
  pthread_create(&ptid, nullptr, hack_thread, nullptr);
}

int RegisterMenu(JNIEnv *env) {
  JNINativeMethod methods[] = {
      {OBFUSCATE("Icon"), OBFUSCATE("()Ljava/lang/String;"),
       reinterpret_cast<void *>(Icon)},
      {OBFUSCATE("IconWebViewData"), OBFUSCATE("()Ljava/lang/String;"),
       reinterpret_cast<void *>(IconWebViewData)},
      {OBFUSCATE("IsGameLibLoaded"), OBFUSCATE("()Z"),
       reinterpret_cast<void *>(isGameLibLoaded)},
      {OBFUSCATE("Init"),
       OBFUSCATE("(Landroid/content/Context;Landroid/widget/TextView;Landroid/"
                 "widget/TextView;)V"),
       reinterpret_cast<void *>(Init)},
      {OBFUSCATE("SettingsList"), OBFUSCATE("()[Ljava/lang/String;"),
       reinterpret_cast<void *>(SettingsList)},
      {OBFUSCATE("GetFeatureList"), OBFUSCATE("()[Ljava/lang/String;"),
       reinterpret_cast<void *>(GetFeatureList)},
  };

  jclass clazz = env->FindClass(OBFUSCATE("com/android/support/Menu"));
  if (!clazz)
    return JNI_ERR;
  if (env->RegisterNatives(clazz, methods,
                           sizeof(methods) / sizeof(methods[0])) != 0)
    return JNI_ERR;
  return JNI_OK;
}

int RegisterPreferences(JNIEnv *env) {
  JNINativeMethod methods[] = {
      {OBFUSCATE("Changes"),
       OBFUSCATE("(Landroid/content/Context;ILjava/lang/String;IZLjava/lang/"
                 "String;)V"),
       reinterpret_cast<void *>(Changes)},
  };
  jclass clazz = env->FindClass(OBFUSCATE("com/android/support/Preferences"));
  if (!clazz)
    return JNI_ERR;
  if (env->RegisterNatives(clazz, methods,
                           sizeof(methods) / sizeof(methods[0])) != 0)
    return JNI_ERR;
  return JNI_OK;
}

int RegisterMain(JNIEnv *env) {
  JNINativeMethod methods[] = {
      {OBFUSCATE("CheckOverlayPermission"),
       OBFUSCATE("(Landroid/content/Context;)V"),
       reinterpret_cast<void *>(CheckOverlayPermission)},
  };
  jclass clazz = env->FindClass(OBFUSCATE("com/android/support/Main"));
  if (!clazz)
    return JNI_ERR;
  if (env->RegisterNatives(clazz, methods,
                           sizeof(methods) / sizeof(methods[0])) != 0)
    return JNI_ERR;

  return JNI_OK;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK)
    return JNI_ERR;

  if (RegisterMenu(env) != 0)
    return JNI_ERR;
  if (RegisterPreferences(env) != 0)
    return JNI_ERR;
  if (RegisterMain(env) != 0)
    return JNI_ERR;
  return JNI_VERSION_1_6;
}
