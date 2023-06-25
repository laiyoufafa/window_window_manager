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

#include "session_manager_agent_controller.h"
#include "window_manager_hilog.h"
#include "wm_common.h"

namespace OHOS {
namespace Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "SessionManagerAgentController"};
}
WM_IMPLEMENT_SINGLE_INSTANCE(SessionManagerAgentController)

WMError SessionManagerAgentController::RegisterWindowManagerAgent(const sptr<IWindowManagerAgent>& windowManagerAgent,
    WindowManagerAgentType type)
{
    WLOGFD("RegisterWindowManagerAgent");
    return smAgentContainer_.RegisterAgent(windowManagerAgent, type) ? WMError::WM_OK : WMError::WM_ERROR_NULLPTR;
}

WMError SessionManagerAgentController::UnregisterWindowManagerAgent(const sptr<IWindowManagerAgent>& windowManagerAgent,
    WindowManagerAgentType type)
{
    WLOGFD("UnregisterWindowManagerAgent");
    return smAgentContainer_.UnregisterAgent(windowManagerAgent, type) ? WMError::WM_OK : WMError::WM_ERROR_NULLPTR;
}

void SessionManagerAgentController::UpdateCameraFloatWindowStatus(uint32_t accessTokenId, bool isShowing)
{
    for (auto& agent : smAgentContainer_.GetAgentsByType(
        WindowManagerAgentType::WINDOW_MANAGER_AGENT_TYPE_CAMERA_FLOAT)) {
        if (agent != nullptr) {
            agent->UpdateCameraFloatWindowStatus(accessTokenId, isShowing);
        }
    }
}

void SessionManagerAgentController::UpdateFocusChangeInfo(const sptr<FocusChangeInfo>& focusChangeInfo, bool isFocused)
{
    for (auto& agent : smAgentContainer_.GetAgentsByType(
        WindowManagerAgentType::WINDOW_MANAGER_AGENT_TYPE_FOCUS)) {
        if (agent != nullptr) {
            agent->UpdateFocusChangeInfo(focusChangeInfo, isFocused);
        }
    }
}
} // namespace Rosen
} // namespace OHOS