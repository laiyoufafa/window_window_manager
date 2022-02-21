/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "display.h"
#include "display_info.h"
#include "display_manager_adapter.h"
#include "window_manager_hilog.h"

namespace OHOS::Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, 0, "Display"};
}
class Display::Impl : public RefBase {
public:
    Impl(const std::string& name, sptr<DisplayInfo> info)
    {
        name_= name;
        if (info == nullptr) {
            WLOGFE("DisplayInfo is nullptr.");
        }
        displayInfo_ = info;
    }
    ~Impl() = default;
    DEFINE_VAR_FUNC_GET_SET(std::string, Name, name);
    DEFINE_VAR_FUNC_GET_SET(sptr<DisplayInfo>, DisplayInfo, displayInfo);
};

Display::Display(const std::string& name, sptr<DisplayInfo> info)
    : pImpl_(new Impl(name, info))
{
}

DisplayId Display::GetId() const
{
    return pImpl_->GetDisplayInfo()->GetDisplayId();
}

int32_t Display::GetWidth() const
{
    SingletonContainer::Get<DisplayManagerAdapter>().UpdateDisplayInfo(GetId());
    return pImpl_->GetDisplayInfo()->GetWidth();
}

int32_t Display::GetHeight() const
{
    SingletonContainer::Get<DisplayManagerAdapter>().UpdateDisplayInfo(GetId());
    return pImpl_->GetDisplayInfo()->GetHeight();
}

uint32_t Display::GetFreshRate() const
{
    SingletonContainer::Get<DisplayManagerAdapter>().UpdateDisplayInfo(GetId());
    return pImpl_->GetDisplayInfo()->GetFreshRate();
}

ScreenId Display::GetScreenId() const
{
    SingletonContainer::Get<DisplayManagerAdapter>().UpdateDisplayInfo(GetId());
    return pImpl_->GetDisplayInfo()->GetScreenId();
}

void Display::UpdateDisplayInfo(sptr<DisplayInfo> displayInfo)
{
    if (displayInfo == nullptr) {
        WLOGFE("displayInfo is invalid");
        return;
    }
    pImpl_->SetDisplayInfo(displayInfo);
}

float Display::GetVirtualPixelRatio() const
{
    // TODO: Should get from DMS
#ifdef PRODUCT_RK
    return 1.0f;
#else
    return 2.0f;
#endif
}
} // namespace OHOS::Rosen