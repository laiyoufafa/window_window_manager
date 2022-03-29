/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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

#include "js_window_extension.h"
#include "window_manager_hilog.h"
#include "wm_common.h"

namespace OHOS {
namespace Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "JSWindowExtension"};
}

class DispatchInputEventListener : public IDispatchInputEventListener {
public:
    void OnDispatchPointerEvent(std::shared_ptr<MMI::PointerEvent>& inputEvent) override
    {
    }
    void OnDispatchKeyEvent(std::shared_ptr<MMI::KeyEvent>& keyEvent) override
    {
    }
}

JsWindowExtension* JsWindowExtension::Create(const std::unique_ptr<AbilityRuntime::Runtime>& runtime)
{
    WLOGFD("Create runtime");
    return new JsWindowExtension(static_cast<AbilityRuntime::JsRuntime&>(*runtime));
}

JsWindowExtension::JsWindowExtension(AbilityRuntime::JsRuntime& jsRuntime) : jsRuntime_(jsRuntime) {}
JsWindowExtension::~JsWindowExtension() = default;

void JsWindowExtension::Init(const std::shared_ptr<AbilityLocalRecord> &record,
    const std::shared_ptr<OHOSApplication> &application, std::shared_ptr<AbilityHandler> &handler,
    const sptr<IRemoteObject> &token)
{
    WindowExtension::Init(record, application, handler, token);
    std::string srcPath = "";
    GetSrcPath(srcPath);
    if (srcPath.empty()) {
        WLOGFE("Failed to get srcPath");
        return;
    }

    std::string moduleName(Extension::abilityInfo_->moduleName);
    moduleName.append("::").append(abilityInfo_->name);
    WLOGFI("JsWindowExtension::Init module:%{public}s,srcPath:%{public}s.", moduleName.c_str(), srcPath.c_str());
    HandleScope handleScope(jsRuntime_);
    auto& engine = jsRuntime_.GetNativeEngine();

    jsObj_ = jsRuntime_.LoadModule(moduleName, srcPath);
    if (jsObj_ == nullptr) {
        WLOGFE("Failed to get jsObj_");
        return;
    }
    WLOGFI("JsWindowExtension::Init ConvertNativeValueTo.");
    NativeObject* obj = ConvertNativeValueTo<NativeObject>(jsObj_->Get());
    if (obj == nullptr) {
        WLOGFE("Failed to get JsWindowExtension object");
        return;
    }

    auto context = GetContext();
    if (context == nullptr) {
        WLOGFE("Failed to get context");
        return;
    }
    WLOGFI("JsWindowExtension::Init CreateJsWindowExtensionContext.");
    NativeValue* contextObj = CreateJsWindowExtensionContext(engine, context);
    shellContextRef_ = jsRuntime_.LoadSystemModule("application.WindowExtensionContext", &contextObj, ARGC_ONE);
    contextObj = shellContextRef_->Get();
    WLOGFI("JsWindowExtension::Init Bind.");
    context->Bind(jsRuntime_, shellContextRef_.get());
    WLOGFI("JsWindowExtension::SetProperty.");
    obj->SetProperty("context", contextObj);

    auto nativeObj = ConvertNativeValueTo<NativeObject>(contextObj);
    if (nativeObj == nullptr) {
        WLOGFE("Failed to get extension native object");
        return;
    }

    WLOGFI("Set extension context pointer: %{public}p", context.get());

    nativeObj->SetNativePointer(new std::weak_ptr<AbilityRuntime::Context>(context),
        [](NativeEngine*, void* data, void*) {
            WLOGFI("Finalizer for weak_ptr extension context is called");
            delete static_cast<std::weak_ptr<AbilityRuntime::Context>*>(data);
        }, nullptr);

    WLOGFI("JsWindowExtension::Init end.");
}

sptr<IRemoteObject> JsWindowExtension::OnConnect(const AAFwk::Want &want)
{
    WLOGFI("called.");
    Extension::OnConnect(want);

    sptr<WindowExtensionClientStub> stub = new WindowExtensionClientStub();
    if (!stub) {
        WLOGFE("stub is nullptr.");
        return nullptr;
    }
    WLOGFD("Create stub successfully");
    return stub->AsObject();
}

void JsWindowExtension::OnDisconnect(const AAFwk::Want &want)
{
    BYTRACE_NAME(BYTRACE_TAG_ABILITY_MANAGER, __PRETTY_FUNCTION__);
    Extension::OnDisconnect(want);
    if (window_ != nullptr) {
        window_->RemoveDispatchInoutEventLisenser();
        window_->Destroy();
    }
}

void JsWindowExtension::OnStart(const AAFwk::Want &want)
{
    Extension::OnStart(want);
    WLOGFI("JsWindowExtension OnStart begin..");
    if (want == nullptr) {
        return;
    }

    sptr<WindowOption> option =  new (std::nothrow)WindowOption();
    if (option == nullptr) {
        WLOGFE("Get option failed");
        return;
    }

    option->SetWindowType(WindowType::WINDOW_TYPE_APP_COMPONENT);
    option->SetWindowMode(OHOS::Rosen::WindowMode::WINDOW_MODE_FLOATING);

    Rect rect { want.GetIntParam(RECT_FORM_KEY_POS_X), 
        want.GetIntParam(RECT_FORM_KEY_POS_Y),
        want.GetIntParam(RECT_FORM_KEY_HEIGHT),
        want.GetIntParam(RECT_FORM_KEY_WIDTH) };
    option->SetWindowRect(rect);

    ElementName elementName = want.GetElementName();
    stt::String windowName;
    if (elementName != nullptr) {
        windowName = elementName.GetBundleName();
    }

    window_ = Window::Create(windowName, option, nullptr);
    if (window_ == nullptr) {
        WLOGFE("create window failed");
        return;
    }

    dispatchInputEventListener_ = new DispatchInputEventListener();
    window_->AddDispatchInputEventListener(dispatchInputEventListener_);
}

bool JsWindowExtension::ConnectToClient(sptr<IWindowExtensionServer>& token)
{
    extensionToken_ = token;
}

void JsWindowExtension::Resize(Rect rect)
{
    if (window_ != nullptr) {
        window_->Resize(rect);
    }
}

void JsWindowExtension::Hide()
{
    if (window_ != nullptr) {
        window_->Hide();
    }
}

void JsWindowExtension::Show()
{
    if (window_ != nullptr) {
        window_->Show();
    }
}

void JsWindowExtension::RequestFocus()
{
    if (window_ != nullptr) {
        window_->RequestFocus();
    }
}
} // namespace Rosen
} // namespace OHOS