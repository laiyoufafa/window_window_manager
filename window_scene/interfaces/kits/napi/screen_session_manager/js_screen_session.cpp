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

#include "js_screen_session.h"

#include <memory>
#include <string>
#include <type_traits>

#include <js_runtime_utils.h>
#include "js_screen_utils.h"
#include "interfaces/include/ws_common.h"
#include "window_manager_hilog.h"

namespace OHOS::Rosen {
using namespace AbilityRuntime;
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "JsScreenSession"};
    const std::string ON_CONNECTION_CALLBACK = "connect";
    const std::string ON_DISCONNECTION_CALLBACK = "disconnect";
    const std::string ON_PROPERTY_CHANGE_CALLBACK = "propertyChange";
}

NativeValue* JsScreenSession::Create(NativeEngine& engine, const sptr<ScreenSession>& screenSession)
{
    WLOGFD("Create.");
    NativeValue* objValue = engine.CreateObject();
    NativeObject* object = ConvertNativeValueTo<NativeObject>(objValue);

    auto jsScreenSession = std::make_unique<JsScreenSession>(engine, screenSession);
    object->SetNativePointer(jsScreenSession.release(), JsScreenSession::Finalizer, nullptr);
    object->SetProperty("screenId", CreateJsValue(engine, static_cast<int64_t>(screenSession->GetScreenId())));

    const char* moduleName = "JsScreenSession";
    BindNativeFunction(engine, *object, "on", moduleName, JsScreenSession::RegisterCallback);

    return objValue;
}

void JsScreenSession::Finalizer(NativeEngine* engine, void* data, void* hint)
{
    WLOGFD("Finalizer.");
    std::unique_ptr<JsScreenSession>(static_cast<JsScreenSession*>(data));
}

JsScreenSession::JsScreenSession(NativeEngine& engine, const sptr<ScreenSession>& screenSession)
    : engine_(engine), screenSession_(screenSession)
{
    RegisterScreenChangeListener();
}

JsScreenSession::~JsScreenSession()
{
}

NativeValue* JsScreenSession::RegisterCallback(NativeEngine* engine, NativeCallbackInfo* info)
{
    WLOGFD("Register callback.");
    JsScreenSession* me = CheckParamsAndGetThis<JsScreenSession>(engine, info);
    return (me != nullptr) ? me->OnRegisterCallback(*engine, *info) : nullptr;
}

NativeValue* JsScreenSession::OnRegisterCallback(NativeEngine& engine, NativeCallbackInfo& info)
{
    WLOGFD("On register callback.");
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
    mCallback_[callbackType] = callbackRef;

    return engine.CreateUndefined();
}

void JsScreenSession::CallJsCallback(std::string callbackType, const ValueFunction& valueFun)
{
    WLOGFD("Call js callback: %{public}s.", callbackType.c_str());
    if (mCallback_.count(callbackType) == 0) {
        WLOGFE("Callback is unregistered!");
        return;
    }

    if (valueFun == nullptr) {
        WLOGFE("Failed to call js callback, value function is null!");
        return;
    }

    auto jsCallbackRef = mCallback_[callbackType];
    wptr<ScreenSession> screenSessionWeak(screenSession_);
    std::unique_ptr<AsyncTask::CompleteCallback> complete = std::make_unique<AsyncTask::CompleteCallback> (
        [jsCallbackRef, valueFun] (NativeEngine &engine, AsyncTask &task, int32_t status) {
            auto method = jsCallbackRef->Get();
            auto value = valueFun(engine);
            if (method == nullptr) {
                WLOGFE("Failed to get method callback from object!");
                return;
            }

            engine.CallFunction(engine.CreateUndefined(), method, value.first, value.second);
        }
    );

    NativeReference* callback = nullptr;
    std::unique_ptr<AsyncTask::ExecuteCallback> execute = nullptr;
    AsyncTask::Schedule("JsScreenSessionManager::" + callbackType,
        engine_, std::make_unique<AsyncTask>(callback, std::move(execute), std::move(complete)));
}

void JsScreenSession::RegisterScreenChangeListener()
{
    if (screenSession_ == nullptr) {
        WLOGFE("Failed to register screen change listener, session is null!");
        return;
    };

    sptr<IScreenChangeListener> screenChangeListener(this);
    screenSession_->RegisterScreenChangeListener(screenChangeListener);
}

void JsScreenSession::OnConnect()
{
    wptr<ScreenSession> screenSessionWeak(screenSession_);
    auto valueFun = [screenSessionWeak](NativeEngine& engine) -> std::pair<NativeValue* const*, size_t> {
        auto screenSession = screenSessionWeak.promote();
        if (screenSession == nullptr) {
            WLOGFE("Failed to call on connect callback, session is null!");
            return {};
        }

        NativeValue* argv[] = { CreateJsValue(engine, static_cast<int64_t>(screenSession->GetScreenId())),
            JsScreenUtils::CreateJsScreenProperty(engine, screenSession->GetScreenProperty()) };
        return {argv, ArraySize(argv)};
    };

    CallJsCallback(ON_CONNECTION_CALLBACK, valueFun);
}

void JsScreenSession::OnDisconnect()
{
    auto valueFun = [](NativeEngine& engine) -> std::pair<NativeValue* const*, size_t> {
        return {};
    };

    CallJsCallback(ON_DISCONNECTION_CALLBACK, valueFun);
}

void JsScreenSession::OnPropertyChange(const ScreenProperty& newProperty)
{
    wptr<ScreenSession> screenSessionWeak(screenSession_);
    auto valueFun = [screenSessionWeak](NativeEngine& engine) -> std::pair<NativeValue* const*, size_t> {
        auto screenSession = screenSessionWeak.promote();
        if (screenSession == nullptr) {
            return {};
        }

        NativeValue* argv[] = { JsScreenUtils::CreateJsScreenProperty(engine,screenSession->GetScreenProperty()) };
        return {argv, ArraySize(argv)};
    };

    CallJsCallback(ON_PROPERTY_CHANGE_CALLBACK, valueFun);
}

sptr<ScreenSession> JsScreenSession::GetNativeSession() const
{
    return screenSession_;
}
} // namespace OHOS::Rosen
