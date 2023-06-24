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

#ifndef OHOS_ROSEN_WINDOW_SCENE_EXTENSION_SESSION_H
#define OHOS_ROSEN_WINDOW_SCENE_EXTENSION_SESSION_H

#include "session/host/include/session.h"
#include "want.h"
#include "want_params.h"

namespace OHOS::Rosen {
using NotifyUpdateAbilityResultFunc = std::function<void(uint32_t resultCode, const AAFwk::Want& want)>;
using NotifySendExtensionDataFunc = std::function<void(const AAFwk::WantParams& wantParams)>;
class ExtensionSession : public Session {
public:
    ExtensionSession(const SessionInfo& info);
    ~ExtensionSession() = default;

    WSError UpdateAbilityResult(uint32_t resultCode, const AAFwk::Want& want) override;
    void SetUpdateAbilityResultListener(const NotifyUpdateAbilityResultFunc& func);
    WSError SendExtensionData(const AAFwk::WantParams& wantParams) override;
    void SetSendExtensionDataListener(const NotifySendExtensionDataFunc& func);
private:
    NotifyUpdateAbilityResultFunc updateAbilityResultFunc_;
    NotifySendExtensionDataFunc sendExtensionDataFunc_;
};
} // namespace OHOS::Rosen

#endif // OHOS_ROSEN_WINDOW_SCENE_EXTENSION_SESSION_H
