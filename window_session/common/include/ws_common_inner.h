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
#ifndef OHOS_ROSEN_WINDOW_SESSION_COMMON_INNER_H
#define OHOS_ROSEN_WINDOW_SESSION_COMMON_INNER_H

#include <string>
#include <map>
#include "iremote_broker.h"
namespace OHOS::Rosen {
struct AbilityInfo {
    std::string bundleName_ = "";
    std::string abilityName_ = "";
    sptr<IRemoteObject> callerToken_ = nullptr;
    bool isExtension_ = false;
    uint32_t extensionType_ = 0;
};
}
#endif // OHOS_ROSEN_WINDOW_SESSION_COMMON_INNER_H