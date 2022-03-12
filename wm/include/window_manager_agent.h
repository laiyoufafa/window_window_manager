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

#ifndef OHOS_WINDOW_MANAGER_AGENT_H
#define OHOS_WINDOW_MANAGER_AGENT_H

#include "window_manager.h"
#include "zidl/window_manager_agent_stub.h"

namespace OHOS {
namespace Rosen {
class WindowManagerAgent : public WindowManagerAgentStub {
public:
    WindowManagerAgent() = default;
    ~WindowManagerAgent() = default;

    void UpdateFocusStatus(uint32_t windowId, const sptr<IRemoteObject>& abilityToken, WindowType windowType,
        DisplayId displayId, bool focused) override;
    void UpdateFocusChangeInfo(const sptr<FocusChangeInfo>& focusChangeInfo, bool focused) override;
    void UpdateSystemBarRegionTints(DisplayId displayId, const SystemBarRegionTints& props) override;
    void NotifyAccessibilityWindowInfo(const sptr<AccessibilityWindowInfo>& windowInfo, WindowUpdateType type) override;
    void UpdateWindowVisibilityInfo(const std::vector<sptr<WindowVisibilityInfo>>& visibilityInfos) override;
};
} // namespace Rosen
} // namespace OHOS
#endif // OHOS_WINDOW_MANAGER_AGENT_H
