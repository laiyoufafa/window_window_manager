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

#include "common/include/message_scheduler.h"
namespace OHOS::Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "WSMessageScheduler"};
}
MessageScheduler::MessageScheduler(const std::string& threadName)
{
    auto runner = AppExecFwk::EventRunner::Create(threadName);
    handler_ = std::make_shared<AppExecFwk::EventHandler>(runner);
}

MessageScheduler::~MessageScheduler()
{
    handler_ = nullptr;
}

void MessageScheduler::PostVoidSyncTask(Task task)
{
    if (handler_ == nullptr) {
        WLOGFE("Failed to post task, handler is null!");
        return;
    }

    bool ret = handler_->PostSyncTask(std::move(task), AppExecFwk::EventQueue::Priority::IMMEDIATE);
    if (!ret) {
        WLOGFE("EventHandler PostTask Failed");
    }
}

void MessageScheduler::PostAsyncTask(Task task, int64_t delayTime)
{
    if (handler_ == nullptr) {
        WLOGFE("Failed to post task, handler is null!");
        return;
    }
    bool ret = handler_->PostTask(std::move(task), delayTime, AppExecFwk::EventQueue::Priority::IMMEDIATE);
    if (!ret) {
        WLOGFE("EventHandler PostTask Failed");
    }
}
} // namespace OHOS::Rosen