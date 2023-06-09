/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "vsync_station.h"
#include "transaction/rs_interfaces.h"
#include "window_manager_hilog.h"

namespace OHOS {
namespace Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "VsyncStation"};
    const std::string VSYNC_TIME_OUT_TASK = "vsync_time_out_task";
    constexpr int64_t VSYNC_TIME_OUT_MILLISECONDS = 600;
}
WM_IMPLEMENT_SINGLE_INSTANCE(VsyncStation)

void VsyncStation::RequestVsync(CallbackType type, const std::shared_ptr<VsyncCallback>& vsyncCallback)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto iter = vsyncCallbacks_.find(type);
        if (iter == vsyncCallbacks_.end()) {
            WLOGFE("wrong callback type.");
            return;
        }
        iter->second.insert(vsyncCallback);

        if (vsyncHandler_ == nullptr) {
            auto mainEventRunner = AppExecFwk::EventRunner::GetMainEventRunner();
            if (mainEventRunner != nullptr && isMainHandlerAvailable_) {
                vsyncHandler_ = std::make_shared<AppExecFwk::EventHandler>(mainEventRunner);
            } else {
                WLOGFE("MainEventRunner is not available, create a new EventRunner for vsyncHandler_.");
                vsyncHandler_ = std::make_shared<AppExecFwk::EventHandler>(
                    AppExecFwk::EventRunner::Create(VSYNC_THREAD_ID));
            }
            auto& rsClient = OHOS::Rosen::RSInterfaces::GetInstance();
            while (receiver_ == nullptr) {
                receiver_ = rsClient.CreateVSyncReceiver("WM_" + std::to_string(::getpid()), vsyncHandler_);
            }
            receiver_->Init();
        }
        if (hasRequestedVsync_) {
            return;
        }
        hasRequestedVsync_ = true;
        vsyncCount_++;
        if (vsyncCount_ & 0x01) { // write log every 2 vsync
            WLOGFI("Request next vsync.");
        }
        vsyncHandler_->RemoveTask(VSYNC_TIME_OUT_TASK);
        vsyncHandler_->PostTask(vsyncTimeoutCallback_, VSYNC_TIME_OUT_TASK, VSYNC_TIME_OUT_MILLISECONDS);
    }
    receiver_->RequestNextVSync(frameCallback_);
}

void VsyncStation::RemoveCallback(CallbackType type, const std::shared_ptr<VsyncCallback>& vsyncCallback)
{
    WLOGFI("Remove callback, type: %{public}u", type);
    std::lock_guard<std::mutex> lock(mtx_);
    auto iter = vsyncCallbacks_.find(type);
    if (iter == vsyncCallbacks_.end()) {
        WLOGFE("wrong callback type.");
        return;
    }
    iter->second.erase(vsyncCallback);
}

void VsyncStation::VsyncCallbackInner(int64_t timestamp)
{
    std::map<CallbackType, std::unordered_set<std::shared_ptr<VsyncCallback>>> vsyncCallbacks;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        hasRequestedVsync_ = false;
        vsyncCallbacks = vsyncCallbacks_;
        for (auto& vsyncCallbacksSet: vsyncCallbacks_) {
            vsyncCallbacksSet.second.clear();
        }
        vsyncHandler_->RemoveTask(VSYNC_TIME_OUT_TASK);
        if (vsyncCount_ & 0x01) { // write log every 2 vsync
            WLOGFI("On vsync callback.");
        }
    }
    for (auto& vsyncCallbacksSet: vsyncCallbacks) {
        for (const auto& callback: vsyncCallbacksSet.second) {
            callback->onCallback(timestamp);
        }
    }
}

void VsyncStation::OnVsync(int64_t timestamp, void* client)
{
    auto vsyncClient = static_cast<VsyncStation*>(client);
    if (vsyncClient) {
        vsyncClient->VsyncCallbackInner(timestamp);
    } else {
        WLOGFE("VsyncClient is null");
    }
}

void VsyncStation::OnVsyncTimeOut()
{
    WLOGFE("Vsync time out");
    std::lock_guard<std::mutex> lock(mtx_);
    hasRequestedVsync_ = false;
}
}
}