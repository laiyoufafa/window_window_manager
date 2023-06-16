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

#include "session/container/include/window_event_channel.h"

#include <functional>
#include <utility>

#include "unistd.h"
#include "sys/types.h"

#include <axis_event.h>
#include <key_event.h>
#include <pointer_event.h>

#include "anr_handler.h"
#include "window_manager_hilog.h"

namespace OHOS::Rosen {
namespace {
constexpr HiviewDFX::HiLogLabel LABEL = { LOG_CORE, HILOG_DOMAIN_WINDOW, "WindowEventChannel" };
}

WSError WindowEventChannel::TransferKeyEvent(const std::shared_ptr<MMI::KeyEvent>& keyEvent)
{
    WLOGFD("WindowEventChannel receive key event");
    if (!sessionStage_) {
        WLOGFE("session stage is null!");
        return WSError::WS_ERROR_NULLPTR;
    }
    sessionStage_->NotifyKeyEvent(keyEvent);
    return WSError::WS_OK;
}

WSError WindowEventChannel::TransferPointerEvent(const std::shared_ptr<MMI::PointerEvent>& pointerEvent)
{
    CALL_DEBUG_ENTER;
    WLOGFD("WindowEventChannel receive pointer event, eventId:%{public}d", pointerEvent->GetId());
    if (!sessionStage_) {
        WLOGFE("session stage is null!");
        return WSError::WS_ERROR_NULLPTR;
    }
    // 这个可以考虑优化，在应用进程启动或应用窗口被创建的时候只初始化一遍就可以，不必每次都设置
    ANRHDL->SetSessionStage(sessionStage_);
    if (pointerEvent != nullptr) {
        WLOGFD("SetProcessedCallback enter");
        pointerEvent->SetProcessedCallback(dispatchCallback_);
        WLOGFD("SetProcessedCallback leave");
    }
    static auto checkInAnrRegin = [](const std::shared_ptr<MMI::PointerEvent> pointerEvent) -> bool {
        std::pair<int32_t, int32_t> leftUp {0, 0};
        std::pair<int32_t, int32_t> rightDown {500, 500};
        int32_t displayX = pointerEvent->GetDisplayX();
        int32_t displayY = pointerEvent->GetDisplayY();
        return (displayX >= leftUp.first && displayX < rightDown.first &&
                displayY >= leftUp.second && displayY < rightDown.second);
    };
    if (checkInAnrRegin(pointerEvent)) {
        WLOGFD("The pointerEvent eventId: %{public}d in anr regin, sleep 6 seconds to trigger anr", pointerEvent->GetId());
        sleep(6);
    }
    sessionStage_->NotifyPointerEvent(pointerEvent); // sessionStage_ 就是 windowSessionImpl,windowSessionImpl 里有 hostSession_
    return WSError::WS_OK;
}

int32_t WindowEventChannel::GetApplicationPid()
{
    CALL_DEBUG_ENTER;
    int32_t applicationPid = static_cast<int32_t>(::getpid());
    WLOGFD("WindowEventChannel GetApplicationPid, pid:%{public}d", applicationPid);
    return applicationPid;
}

void WindowEventChannel::OnDispatchEventProcessed(int32_t eventId, int64_t actionTime)
{
    CALL_DEBUG_ENTER;
    ANRHDL->SetLastProcessedEventId(eventId, actionTime);
}

} // namespace OHOS::Rosen
