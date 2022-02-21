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

#ifndef FOUNDATION_DM_DISPLAY_MANAGER_ADAPTER_H
#define FOUNDATION_DM_DISPLAY_MANAGER_ADAPTER_H

#include <map>
#include <mutex>
#include <surface.h>

#include "display.h"
#include "screen.h"
#include "screen_group.h"
#include "dm_common.h"
#include "display_manager_interface.h"
#include "singleton_delegator.h"

namespace OHOS::Rosen {
class BaseAdapter {
public:
    virtual bool RegisterDisplayManagerAgent(const sptr<IDisplayManagerAgent>& displayManagerAgent,
        DisplayManagerAgentType type);
    virtual bool UnregisterDisplayManagerAgent(const sptr<IDisplayManagerAgent>& displayManagerAgent,
        DisplayManagerAgentType type);
    virtual void Clear();
protected:
    bool InitDMSProxyLocked();
    std::recursive_mutex mutex_;
    sptr<IDisplayManager> displayManagerServiceProxy_ = nullptr;
    sptr<IRemoteObject::DeathRecipient> dmsDeath_ = nullptr;
};

class DMSDeathRecipient : public IRemoteObject::DeathRecipient {
public:
    DMSDeathRecipient(BaseAdapter& adapter);
    virtual void OnRemoteDied(const wptr<IRemoteObject>& wptrDeath) override;
private:
    BaseAdapter& adapter_;
};

class DisplayManagerAdapter : public BaseAdapter {
WM_DECLARE_SINGLE_INSTANCE(DisplayManagerAdapter);
public:
    virtual DisplayId GetDefaultDisplayId();
    virtual sptr<Display> GetDisplayById(DisplayId displayId);
    virtual std::shared_ptr<Media::PixelMap> GetDisplaySnapshot(DisplayId displayId);
    virtual bool WakeUpBegin(PowerStateChangeReason reason);
    virtual bool WakeUpEnd();
    virtual bool SuspendBegin(PowerStateChangeReason reason);
    virtual bool SuspendEnd();
    virtual bool SetScreenPowerForAll(DisplayPowerState state, PowerStateChangeReason reason);
    virtual bool SetDisplayState(DisplayState state);
    virtual DisplayState GetDisplayState(DisplayId displayId);
    virtual void NotifyDisplayEvent(DisplayEvent event);
    virtual void UpdateDisplayInfo(DisplayId);
private:
    sptr<DisplayInfo> GetDisplayInfo(DisplayId displayId);

    static inline SingletonDelegator<DisplayManagerAdapter> delegator;

    std::map<DisplayId, sptr<Display>> displayMap_;
    DisplayId defaultDisplayId_;
};

class ScreenManagerAdapter : public BaseAdapter {
WM_DECLARE_SINGLE_INSTANCE(ScreenManagerAdapter);
public:
    virtual bool RequestRotation(ScreenId screenId, Rotation rotation);
    virtual ScreenId CreateVirtualScreen(VirtualScreenOption option);
    virtual DMError DestroyVirtualScreen(ScreenId screenId);
    virtual DMError SetVirtualScreenSurface(ScreenId screenId, sptr<Surface> surface);
    virtual sptr<Screen> GetScreenById(ScreenId screenId);
    virtual sptr<ScreenGroup> GetScreenGroupById(ScreenId screenId);
    virtual std::vector<sptr<Screen>> GetAllScreens();
    virtual DMError MakeMirror(ScreenId mainScreenId, std::vector<ScreenId> mirrorScreenId);
    virtual DMError MakeExpand(std::vector<ScreenId> screenId, std::vector<Point> startPoint);
    virtual bool SetScreenActiveMode(ScreenId screenId, uint32_t modeId);
    virtual void UpdateScreenInfo(ScreenId);

    // colorspace, gamut
    virtual DMError GetScreenSupportedColorGamuts(ScreenId screenId, std::vector<ScreenColorGamut>& colorGamuts);
    virtual DMError GetScreenColorGamut(ScreenId screenId, ScreenColorGamut& colorGamut);
    virtual DMError SetScreenColorGamut(ScreenId screenId, int32_t colorGamutIdx);
    virtual DMError GetScreenGamutMap(ScreenId screenId, ScreenGamutMap& gamutMap);
    virtual DMError SetScreenGamutMap(ScreenId screenId, ScreenGamutMap gamutMap);
    virtual DMError SetScreenColorTransform(ScreenId screenId);
private:
    sptr<ScreenInfo> GetScreenInfo(ScreenId screenId);

    static inline SingletonDelegator<ScreenManagerAdapter> delegator;

    std::map<ScreenId, sptr<Screen>> screenMap_;
    std::map<ScreenId, sptr<ScreenGroup>> screenGroupMap_;
};
} // namespace OHOS::Rosen
#endif // FOUNDATION_DM_DISPLAY_MANAGER_ADAPTER_H
