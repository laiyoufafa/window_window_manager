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

#include "js_scene_utils.h"

#include <js_runtime_utils.h>

#include "interfaces/include/ws_common.h"
#include "window_manager_hilog.h"

namespace OHOS::Rosen {
using namespace AbilityRuntime;
namespace {
constexpr HiviewDFX::HiLogLabel LABEL = { LOG_CORE, HILOG_DOMAIN_WINDOW, "JsSceneUtils" };
}

bool GetAbilityInfoFromJs(NativeEngine& engine, NativeObject* jsObject, SessionInfo& sessionInfo)
{
    NativeValue* jsBundleName = jsObject->GetProperty("bundleName");
    NativeValue* jsAbilityName = jsObject->GetProperty("abilityName");
    if (jsBundleName->TypeOf() != NATIVE_UNDEFINED) {
        std::string bundleName;
        if (!ConvertFromJsValue(engine, jsBundleName, bundleName)) {
            WLOGFE("[NAPI]Failed to convert parameter to bundleName");
            return false;
        }
        sessionInfo.bundleName_ = bundleName;
    }
    if (jsAbilityName->TypeOf() != NATIVE_UNDEFINED) {
        std::string abilityName;
        if (!ConvertFromJsValue(engine, jsAbilityName, abilityName)) {
            WLOGFE("[NAPI]Failed to convert parameter to abilityName");
            return false;
        }
        sessionInfo.abilityName_ = abilityName;
    }
    return true;
}

NativeValue* CreateJsSceneInfo(NativeEngine& engine, const SessionInfo& sessionInfo)
{
    WLOGFI("[NAPI]CreateJsSceneInfo");
    NativeValue* objValue = engine.CreateObject();
    NativeObject* object = ConvertNativeValueTo<NativeObject>(objValue);
    if (object == nullptr) {
        WLOGFE("[NAPI]Failed to convert sessionInfo to jsObject");
        return nullptr;
    }
    object->SetProperty("bundleName", CreateJsValue(engine, sessionInfo.bundleName_));
    object->SetProperty("abilityName", CreateJsValue(engine, sessionInfo.abilityName_));
    return objValue;
}
} // namespace OHOS::Rosen