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

#ifndef FOUNDATION_DM_SCREEN_MANAGER_H
#define FOUNDATION_DM_SCREEN_MANAGER_H

#include <refbase.h>
#include "screen.h"
#include "screen_group.h"
#include "wm_single_instance.h"

namespace OHOS::Rosen {
class IScreenChangeListener : public RefBase {
public:
    virtual void OnCreate(ScreenId) = 0;
    virtual void OnDestroy(ScreenId) = 0;
    virtual void OnChange(std::vector<ScreenId>) = 0;
};

class ScreenManager : public RefBase {
WM_DECLARE_SINGLE_INSTANCE_BASE(ScreenManager);
public:
    sptr<Screen> GetScreenById(ScreenId id);
    std::vector<const sptr<Screen>> GetAllScreens();
    sptr<ScreenGroup> makeExpand(std::vector<ScreenId> screenId, std::vector<Point> startPoint);
    sptr<ScreenGroup> makeMirror(ScreenId mainScreenId, std::vector<ScreenId> mirrorScreenId);
    sptr<Screen> createVirtualScreen(VirtualScreenOption option);

private:
    ScreenManager();
    ~ScreenManager();

    class Impl;
    sptr<Impl> pImpl_;
};
} // namespace OHOS::Rosen

#endif // FOUNDATION_DM_SCREEN_MANAGER_H