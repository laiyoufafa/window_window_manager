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

#include "js_screen_session_manager.h"

#include <memory>
#include <string>

#include <js_runtime_utils.h>

#include "js_screen_session.h"
#include "session/screen/include/screen_session.h"
#include "session_manager/screen/include/screen_session_manager.h"
#include "utils/include/window_scene_common.h"
#include "utils/include/window_scene_hilog.h"

namespace OHOS::Rosen {
using namespace AbilityRuntime;
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "JsScreenSessionManager"};
    const std::string SCREEN_CONNECTION_CALLBACK = "screenConnect";
}

NativeValue* JsScreenSessionManager::Init(NativeEngine* engine, NativeValue* exportObj)
{
    WLOGFI("Init.");
    if (engine == nullptr || exportObj == nullptr) {
        WLOGFE("Failed to init, engine or exportObj is null!");
        return nullptr;
    }

    auto object = ConvertNativeValueTo<NativeObject>(exportObj);
    if (object == nullptr) {
        WLOGFE("Failed to convert native value to native object, Object is null!");
        return nullptr;
    }

    auto jsScreenSessionManager = std::make_unique<JsScreenSessionManager>();
    object->SetNativePointer(jsScreenSessionManager.release(), JsScreenSessionManager::Finalizer, nullptr);

    const char* moduleName = "JsScreenSessionManager";
    BindNativeFunction(*engine, *object, "on", moduleName, JsScreenSessionManager::RegisterCallback);
    return engine->CreateUndefined();
}

void JsScreenSessionManager::Finalizer(NativeEngine* engine, void* data, void* hint)
{
    WLOGFI("Finalizer.");
    std::unique_ptr<JsScreenSessionManager>(static_cast<JsScreenSessionManager*>(data));
}

NativeValue* JsScreenSessionManager::RegisterCallback(NativeEngine* engine, NativeCallbackInfo* info)
{
    WLOGFI("Register callback.");
    JsScreenSessionManager* me = CheckParamsAndGetThis<JsScreenSessionManager>(engine, info);
    return (me != nullptr) ? me->OnRegisterCallback(*engine, *info) : nullptr;
}

NativeValue* JsScreenSessionManager::OnRegisterCallback(NativeEngine& engine, const NativeCallbackInfo& info)
{
    WLOGFI("On register callback.");
    if (info.argc < 2) { // 2: params num
        WLOGFE("Argc is invalid: %{public}zu", info.argc);
        engine.Throw(CreateJsError(engine, static_cast<int32_t>(WSErrorCode::WS_ERROR_INVALID_PARAM)));
        return engine.CreateUndefined();
    }

    std::string callbackType;
    if (!ConvertFromJsValue(engine, info.argv[0], callbackType)) {
        WLOGFE("Failed to convert parameter to callback type.");
        engine.Throw(CreateJsError(engine, static_cast<int32_t>(WSErrorCode::WS_ERROR_INVALID_PARAM)));
        return engine.CreateUndefined();
    }

    NativeValue* value = info.argv[1];
    if (!value->IsCallable()) {
        WLOGFE("Failed to register callback, callback is not callable!");
        engine.Throw(CreateJsError(engine, static_cast<int32_t>(WSErrorCode::WS_ERROR_INVALID_PARAM)));
        return engine.CreateUndefined();
    }

    std::shared_ptr<NativeReference> callbackRef(engine.CreateReference(value, 1));
    if (callbackType == SCREEN_CONNECTION_CALLBACK) {
        ProcessRegisterScreenConnection(engine, callbackRef);
    }

    return engine.CreateUndefined();
}

void JsScreenSessionManager::ProcessRegisterScreenConnection(NativeEngine& engine,
    const std::shared_ptr<NativeReference>& callback)
{
    WLOGFI("Process register screen connection.");
    auto screenConnectionCallback = [&engine, callback](sptr<ScreenSession> screenSession) {
        wptr<ScreenSession> screenSessionWeak(screenSession);
        std::unique_ptr<AsyncTask::CompleteCallback> complete = std::make_unique<AsyncTask::CompleteCallback> (
            [callback, screenSessionWeak] (NativeEngine &engine, AsyncTask &task, int32_t status) {
                auto screenSession = screenSessionWeak.promote();
                if (screenSession == nullptr) {
                    WLOGFE("Failed to call screen connection callback, screen session is null!");
                    return;
                }

                auto jsScreenSession = JsScreenSession::Create(engine, screenSession);
                NativeValue* argv[] = { jsScreenSession };
                NativeValue* method = callback->Get();
                if (method == nullptr) {
                    WLOGFE("Failed to get method callback from object!");
                    return;
                }

                engine.CallFunction(engine.CreateUndefined(), method, argv, ArraySize(argv));
            }
        );

        NativeReference* callback = nullptr;
        std::unique_ptr<AsyncTask::ExecuteCallback> execute = nullptr;
        AsyncTask::Schedule("JsScreenSessionManager::OnScreenConnect",
            engine, std::make_unique<AsyncTask>(callback, std::move(execute), std::move(complete)));
    };

    ScreenSessionManager::GetInstance().RegisterScreenConnectionCallback(screenConnectionCallback);
}
} // namespace OHOS::Rosen
