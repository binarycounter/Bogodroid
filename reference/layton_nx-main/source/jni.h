/* jni.h -- fake JNI environment for libll1.so (Level-5 LT1R)
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __JNI_H__
#define __JNI_H__

// the JNIEnv* / JavaVM* handed to the game
extern void *fake_env;
extern void *fake_vm;

void jni_init(void);

#endif
