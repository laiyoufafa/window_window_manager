/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_WINDOW_SCENE_JS_SCENE_UTILS_H
#define OHOS_WINDOW_SCENE_JS_SCENE_UTILS_H

#include <js_runtime_utils.h>
#include <native_engine/native_engine.h>
#include <native_engine/native_value.h>

#include "interfaces/include/ws_common.h"

namespace OHOS::Rosen {
bool GetAbilityInfoFromJs(NativeEngine& engine, NativeObject* jsObject, SessionInfo& sessionInfo);
NativeValue* CreateJsSessionInfo(NativeEngine& engine, const SessionInfo& sessionInfo);
NativeValue* CreateJsSessionState(NativeEngine& engine, const SessionState& state);
NativeValue* SessionStateInit(NativeEngine* engine);
NativeValue* CreateJsSessionRect(NativeEngine& engine, const WSRect& rect);
} // namespace OHOS::Rosen

#endif // OHOS_WINDOW_SCENE_JS_SCENE_UTILS_H
